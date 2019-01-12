#include "qnvimplugin.h"
#include "qnvimconstants.h"
#include "diff_match_patch.h"

#include <coreplugin/icontext.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/statusbarmanager.h>
#include <texteditor/texteditor.h>
#include <texteditor/textdocument.h>
#include <texteditor/fontsettings.h>
#include <texteditor/displaysettings.h>
#include <texteditor/texteditorsettings.h>
#include <utils/osspecificaspects.h>
#include <gui/input.h>
#include <neovimconnector.h>
#include <msgpackrequest.h>
#include <utils/fancylineedit.h>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QGuiApplication>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextEdit>
#include <QThread>
#include <QtMath>

namespace QNVim {
namespace Internal {

NumbersColumn::NumbersColumn(): mNumber(false), mEditor(nullptr) {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    connect(TextEditor::TextEditorSettings::instance(),
        &TextEditor::TextEditorSettings::displaySettingsChanged,
        this, &NumbersColumn::updateGeometry);
}

void NumbersColumn::setEditor(TextEditor::TextEditorWidget *editor) {
    if (editor == mEditor)
        return;
    if (mEditor) {
        mEditor->removeEventFilter(this);
        disconnect(mEditor, &QPlainTextEdit::cursorPositionChanged,
                this, &NumbersColumn::updateGeometry);
        disconnect(mEditor->verticalScrollBar(), &QScrollBar::valueChanged,
                this, &NumbersColumn::updateGeometry);
        disconnect(mEditor->document(), &QTextDocument::contentsChanged,
                this, &NumbersColumn::updateGeometry);
    }
    mEditor = editor;
    setParent(mEditor);
    if (mEditor) {
        mEditor->installEventFilter(this);
        connect(mEditor, &QPlainTextEdit::cursorPositionChanged,
                this, &NumbersColumn::updateGeometry);
        connect(mEditor->verticalScrollBar(), &QScrollBar::valueChanged,
                this, &NumbersColumn::updateGeometry);
        connect(mEditor->document(), &QTextDocument::contentsChanged,
                this, &NumbersColumn::updateGeometry);
        show();
    }
    updateGeometry();
}

void NumbersColumn::setNumber(bool number) {
    mNumber = number;
    updateGeometry();
}

void NumbersColumn::paintEvent(QPaintEvent *event) {
    if (not mEditor)
        return;
    QTextCursor firstVisibleCursor = mEditor->cursorForPosition(QPoint(0, 0));
    QTextBlock firstVisibleBlock = firstVisibleCursor.block();
    if (firstVisibleCursor.positionInBlock() > 0) {
        firstVisibleBlock = firstVisibleBlock.next();
        firstVisibleCursor.setPosition(firstVisibleBlock.position());
    }

    QTextBlock block = mEditor->textCursor().block();
    bool forward = firstVisibleBlock.blockNumber() > block.blockNumber();
    int n = 0;
    while (block.isValid() and block != firstVisibleBlock) {
        block = forward ? block.next() : block.previous();
        if (block.isVisible())
            n += forward ? 1 : -1;
    }

    QPainter p(this);
    QPalette pal = mEditor->extraArea()->palette();
    const QColor fg = pal.color(QPalette::Dark);
    const QColor bg = pal.color(QPalette::Background);
    p.setPen(fg);

    QFontMetricsF fm(mEditor->textDocument()->fontSettings().font());
    qreal lineHeight = fm.lineSpacing();
    QRectF rect(0, mEditor->cursorRect(firstVisibleCursor).y(), width(), lineHeight);
    bool hideLineNumbers = mEditor->lineNumbersVisible();
    while (block.isValid()) {
        if (block.isVisible()) {
            if ((not mNumber or n != 0) and rect.intersects(event->rect())) {
                const int line = qAbs(n);
                const QString number = QString::number(line);
                if (hideLineNumbers)
                    p.fillRect(rect, bg);
                if (hideLineNumbers or line < 100)
                    p.drawText(rect, Qt::AlignRight | Qt::AlignVCenter, number);
            }

            rect.translate(0, lineHeight * block.lineCount());
            if (rect.y() > height())
                break;

            ++n;
        }

        block = block.next();
    }
}

bool NumbersColumn::eventFilter(QObject *, QEvent *event) {
    if (event->type() == QEvent::Resize or event->type() == QEvent::Move)
        updateGeometry();
    return false;
}

void NumbersColumn::updateGeometry() {
    if (not mEditor)
        return;
    QFontMetrics fm(mEditor->textDocument()->fontSettings().font());
    int lineHeight = fm.lineSpacing();
    setFont(mEditor->extraArea()->font());

    QRect rect = mEditor->extraArea()->geometry().adjusted(0, 0, -3, 0);
    bool marksVisible = mEditor->marksVisible();
    bool lineNumbersVisible = mEditor->lineNumbersVisible();
    bool foldMarksVisible = mEditor->codeFoldingVisible();
    if (marksVisible and lineNumbersVisible)
        rect.setLeft(lineHeight);
    if (foldMarksVisible and (marksVisible or lineNumbersVisible))
        rect.setRight(rect.right() - (lineHeight + lineHeight % 2));
    setGeometry(rect);

    update();
}

QNVimPlugin::QNVimPlugin(): mEnabled(true), mCMDLine(nullptr), mNumbersColumn(nullptr),
    mNVim(nullptr), mInputConv(new NeovimQt::InputConv), mVimChanges(0),
    mText(""), mWidth(80), mHeight(35),
    mForegroundColor(Qt::black), mBackgroundColor(Qt::white), mSpecialColor(QColor()),
    mCursorColor(Qt::white), mBusy(false), mMouse(false),
    mNumber(true), mRelativeNumber(true), mWrap(false),
    mCMDLineVisible(false), mUIMode("normal"), mMode("n"), settingBufferFromVim(false),
    mSyncCounter(0) {
    qputenv("MYQVIMRC", QDir().home().filePath(".qnvimrc").toUtf8());
}

QNVimPlugin::~QNVimPlugin() {
    if (mCMDLine)
        Core::StatusBarManager::destroyStatusBarWidget(mCMDLine);
    if (mNVim)
        mNVim->deleteLater();
}

QString QNVimPlugin::filename(Core::IEditor *editor) const {
    if (not editor)
        return "";
    QString filename = editor->document()->filePath().toString();
    if (filename.isEmpty())
        filename = editor->document()->displayName();
    return filename;
}

void QNVimPlugin::fixSize(Core::IEditor *editor) {
    qWarning() << 11;
    if (not editor)
        editor = Core::EditorManager::currentEditor();
    if (not mNVim or not mNVim->isReady())
        return;
    qWarning() << 22;
    TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    QFontMetricsF fm(textEditor->textDocument()->fontSettings().font());
    // -1 is visual whitespaces that Qt Creator put space for (whether it renders them or not)
    // TODO: After ext_columns +4 should be removed
    int width = qFloor(textEditor->viewport()->width() / fm.width('A')) - 1 + 3;
    int height = qFloor(textEditor->height() / fm.lineSpacing());
    qWarning() << textEditor->viewport()->width() << textEditor->extraArea()->width();
    qWarning() << width;
    if (width != mWidth or height != mHeight)
        mNVim->api6()->nvim_ui_try_resize_grid(1, width, height);
}

void QNVimPlugin::syncCursorToVim(Core::IEditor *editor) {
    if (not editor)
        editor = Core::EditorManager::currentEditor();
    if (not editor or not mEditors.contains(filename(editor)))
        return;
    TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    if (mMode == "v" or mMode == "V" or mMode == "\x16" or
            textEditor->textCursor().position() != textEditor->textCursor().anchor())
        return;
    QString text = textEditor->toPlainText();
    int cursorPosition = textEditor->textCursor().position();
    int line = text.left(cursorPosition).count('\n') + 1;
    int col = mNVim->encode(text.left(cursorPosition).section('\n', -1)).length() + 1;
    if (line == mCursor.y() and col == mCursor.x()) {
        return;
    }
    mCursor.setY(line);
    mCursor.setX(col);
    mNVim->api2()->nvim_command(mNVim->encode(QString("buffer %1|call SetCursor(%2,%3)").arg(mBuffers[filename(editor)]).arg(line).arg(col)));
}

void QNVimPlugin::syncSelectionToVim(Core::IEditor *editor) {
    if (not editor)
        editor = Core::EditorManager::currentEditor();
    if (not editor or not mEditors.contains(filename(editor)))
        return;
    TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    QString text = textEditor->toPlainText();
    QTextCursor cursor = textEditor->hasBlockSelection() ? textEditor->blockSelection() : textEditor->textCursor();
    int cursorPosition = cursor.position();
    int anchorPosition = cursor.anchor();
    int line, col, vLine, vCol;
    if (anchorPosition == cursorPosition)
        return;
    QString visualCommand;
    if (textEditor->hasBlockSelection()) {
        line = text.left(cursorPosition).count('\n') + 1;
        col = text.left(cursorPosition).section('\n', -1).length() + 1;
        vLine = text.left(anchorPosition).count('\n') + 1;
        vCol = text.left(anchorPosition).section('\n', -1).length() + 1;
        if (vCol < col)
            --col;
        else if (vCol > col)
            --vCol;
        visualCommand = "\x16";
    }
    else if (mMode == "V") {
        return;
    }
    else {
        if (anchorPosition < cursorPosition)
            --cursorPosition;
        else
            --anchorPosition;
        line = text.left(cursorPosition).count('\n') + 1;
        col = text.left(cursorPosition).section('\n', -1).length() + 1;
        vLine = text.left(anchorPosition).count('\n') + 1;
        vCol = text.left(anchorPosition).section('\n', -1).length() + 1;
        visualCommand = "v";
    }
    if (line == mCursor.y() and col == mCursor.x() and vLine == mVCursor.y() and vCol == mVCursor.x())
        return;
    mCursor.setY(line);
    mCursor.setX(col);
    mVCursor.setY(vLine);
    mVCursor.setX(vCol);
    connect(mNVim->api2()->nvim_command(
                mNVim->encode(QString("buffer %1|normal! \x03%3G%4|%2%5G%6|").arg(mBuffers[filename(editor)]).arg(visualCommand)
                              .arg(vLine).arg(vCol).arg(line).arg(col))),
            &NeovimQt::MsgpackRequest::finished, [=]() {
    });
}

void QNVimPlugin::syncToVim(Core::IEditor *editor, std::function<void()> callback) {
    qWarning() << "QNVimPlugin::syncToVim";
    if (not editor)
        editor = Core::EditorManager::currentEditor();
    if (not editor or not mEditors.contains(filename(editor)))
        return;
    TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    QString text = textEditor->toPlainText();
    int cursorPosition = textEditor->textCursor().position();
    int line = text.left(cursorPosition).count('\n') + 1;
    int col = mNVim->encode(text.left(cursorPosition).section('\n', -1)).length() + 1;

    diff_match_patch differ;
    QList<Patch> patches = differ.patch_make(mText, text);
    mCursor.setY(line);
    mCursor.setX(col);

    if (patches.count()) {
        QString patchCommand = "set paste|call execute('normal! ";
        for (auto patch: patches) {
            int startLine = mText.left(patch.start1).count('\n') + 1;
            int startCol = mNVim->encode(mText.left(patch.start1).section('\n', -1)).length() + 1;
            int endLine = mText.left(patch.start1 + patch.length1 - 1).count('\n') + 1;
            int endCol = mNVim->encode(mText.left(patch.start1 + patch.length1 - 1).section('\n', -1)).length() + 1;
            mText.replace(patch.start1, patch.length1, text.mid(patch.start2, patch.length2));
            patchCommand += QString("G$v%1G%2|o%3G%4|c%5\x03").arg(startLine).arg(startCol).arg(endLine).arg(endCol).
                    arg(text.mid(patch.start2, patch.length2).replace("'", "''"));
        }
        patchCommand += QString("i\x07u\x03')|set nopaste|call cursor(%1,%2)\n").arg(line).arg(col);
        connect(mNVim->api2()->nvim_command(mNVim->encode(patchCommand)),
                &NeovimQt::MsgpackRequest::finished, [=]() {
            if (callback)
                callback();
        });
    }
    else if (callback)
        callback();
}

void QNVimPlugin::syncCursorFromVim(const QVariantList &pos, const QVariantList &vPos, QByteArray mode) {
    if (not mEnabled)
        return;
    Core::IEditor *editor = Core::EditorManager::currentEditor();
    if (not editor or not mBuffers.contains(filename(editor)))
        return;
    TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    int line = pos[0].toInt();
    int col = pos[1].toInt();
    col = mNVim->decode(mNVim->encode(mText.section('\n', line - 1, line - 1)).left(col - 1)).length() + 1;

    int vLine = vPos[0].toInt();
    int vCol = vPos[1].toInt();
    vCol = mNVim->decode(mNVim->encode(mText.section('\n', vLine - 1, vLine - 1)).left(vCol)).length();

    mMode = mode;
    mCursor.setY(line);
    mCursor.setX(col);
    mVCursor.setY(vLine);
    mVCursor.setX(vCol);

    int a = QString("\n" + mText).section('\n', 0, vLine - 1).length() + vCol - 1;
    int p = QString("\n" + mText).section('\n', 0, line - 1).length() + col - 1;
    if (mMode == "V") {
        if (a < p) {
            a = QString("\n" + mText).section('\n', 0, vLine - 1).length();
            p = QString("\n" + mText).section('\n', 0, line).length() - 1;
        }
        else {
            a = QString("\n" + mText).section('\n', 0, vLine).length() - 1;
            p = QString("\n" + mText).section('\n', 0, line - 1).length();
        }
        QTextCursor cursor = textEditor->textCursor();
        cursor.setPosition(a);
        cursor.setPosition(p, QTextCursor::KeepAnchor);
        if (textEditor->textCursor().anchor() != cursor.anchor() or
                textEditor->textCursor().position() != cursor.position())
            textEditor->setTextCursor(cursor);
    }
    else if (mMode == "v") {
        if (a > p)
            ++a;
        else
            ++p;
        QTextCursor cursor = textEditor->textCursor();
        cursor.setPosition(a);
        cursor.setPosition(p, QTextCursor::KeepAnchor);
        if (textEditor->textCursor().anchor() != cursor.anchor() or
                textEditor->textCursor().position() != cursor.position())
            textEditor->setTextCursor(cursor);
    }
    else if (mMode == "\x16") {
        if (vCol > col)
            ++a;
        else
            ++p;
        QTextCursor cursor = textEditor->textCursor();
        cursor.setPosition(a);
        cursor.setPosition(p, QTextCursor::KeepAnchor);
        textEditor->setBlockSelection(cursor);
    }
    else {
        QTextCursor cursor = textEditor->textCursor();
        cursor.clearSelection();
        cursor.setPosition(p);
        if (textEditor->textCursor().position() != cursor.position() or textEditor->textCursor().hasSelection())
            textEditor->setTextCursor(cursor);
    }
}

void QNVimPlugin::syncFromVim() {
    if (not mEnabled)
        return;
    qWarning() << "QNVimPlugin::syncFromVim";
    Core::IEditor *editor = Core::EditorManager::currentEditor();
    if (not editor or not mInitialized.contains(filename(editor)) or not mInitialized[filename(editor)])
        return;
    TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    unsigned long long syncCoutner = ++mSyncCounter;
    connect(mNVim->api2()->nvim_eval("[bufnr(''), b:changedtick, mode(1), &modified, getpos('.'), getpos('v'), &number, &relativenumber, &wrap, execute('1messages')]"),
        &NeovimQt::MsgpackRequest::finished, [=](quint32, quint64, const QVariant &v) {
        QVariantList state = v.toList();
        if (mSyncCounter != syncCoutner)
            return;
        if (not mBuffers.contains(filename(editor))) {
            return;
        }
        int bufferNumber = mBuffers[filename(editor)];
        if (state[0].toString().toLong() != bufferNumber)
            return;
        unsigned long long changedtick = state[1].toULongLong();
        QByteArray mode = state[2].toByteArray();
        bool modified = state[3].toBool();
        QVariantList pos = state[4].toList().mid(1, 2);
        QVariantList vPos = state[5].toList().mid(1, 2);
        mNumber = state[6].toBool();
        mRelativeNumber = state[7].toBool();
        mWrap = state[8].toBool();
        QString cmdLine = mNVim->decode(state[9].toByteArray()).mid(1);
        if (not cmdLine.isEmpty()) {
            mCMDLineDisplay = cmdLine;
            if (not mCMDLineVisible)
                mCMDLine->setPlainText(mCMDLineDisplay);
        }
        mNumbersColumn->setNumber(mNumber);
        mNumbersColumn->setEditor(mRelativeNumber ? textEditor : nullptr);
        if (textEditor->wordWrapMode() != (mWrap ? QTextOption::WrapAnywhere : QTextOption::NoWrap))
            textEditor->setWordWrapMode(mWrap ? QTextOption::WrapAnywhere : QTextOption::NoWrap);
        if (mChangedTicks.value(bufferNumber, 0) == changedtick) {
            syncCursorFromVim(pos, vPos, mode);
            return;
        }
        mChangedTicks[bufferNumber] = changedtick;
        connect(mNVim->api2()->nvim_buf_get_lines(bufferNumber, 0, -1, true),
                &NeovimQt::MsgpackRequest::finished, [=](quint32, quint64, const QVariant &lines) {
            if (not mBuffers.contains(filename(editor))) {
                return;
            }

            mText = "";
            for (auto t: lines.toList())
                mText += mNVim->decode(t.toByteArray()) + '\n';
            mText.chop(1);
            QString oldText = textEditor->toPlainText();
            diff_match_patch differ;
            QList<Patch> patches = differ.patch_make(oldText, mText);

            if (patches.size()) {
                QTextCursor cursor = textEditor->textCursor();
                cursor.beginEditBlock();
                for (auto patch: patches) {
                    cursor.setPosition(patch.start1);
                    cursor.setPosition(patch.start1 + patch.length1, QTextCursor::KeepAnchor);
                    cursor.insertText(mText.mid(patch.start2, patch.length2));
                }
                cursor.endEditBlock();
            }

            if (textEditor->document()->isModified() != modified)
                textEditor->document()->setModified(modified);
            syncCursorFromVim(pos, vPos, mode);
        });
    });
}

void QNVimPlugin::triggerCommand(const QByteArray &commandId) {
    Core::ActionManager::command(commandId.constData())->action()->trigger();
}

bool QNVimPlugin::initialize(const QStringList &arguments, QString *errorString) {
    Q_UNUSED(arguments)
    Q_UNUSED(errorString)

    new HelpEditorFactory();
    new TerminalEditorFactory();

    return initialize();
}

bool QNVimPlugin::initialize() {
    qWarning() << "QNVimPlugin::initialize";
    mCMDLine = new QPlainTextEdit;
    Core::StatusBarManager::addStatusBarWidget(mCMDLine, Core::StatusBarManager::First);
    mCMDLine->document()->setDocumentMargin(0);
    mCMDLine->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mCMDLine->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mCMDLine->setLineWrapMode(QPlainTextEdit::NoWrap);
    mCMDLine->setMinimumWidth(200);
    mCMDLine->setFocusPolicy(Qt::StrongFocus);
    mCMDLine->installEventFilter(this);
    mCMDLine->setFont(TextEditor::TextEditorSettings::instance()->fontSettings().font());

    qobject_cast<QWidget *>(mCMDLine->parentWidget()->children()[2])->hide();

    auto action = new QAction(tr("Toggle QNVim"), this);
    Core::Command *cmd = Core::ActionManager::registerAction(action, Constants::ACTION_ID,
                                                             Core::Context(Core::Constants::C_GLOBAL));
    cmd->setDefaultKeySequence(QKeySequence(tr("Alt+Shift+V,Alt+Shift+V")));
    connect(action, &QAction::triggered, this, &QNVimPlugin::toggleQNVim);

    Core::ActionContainer *menu = Core::ActionManager::createMenu(Constants::MENU_ID);
    menu->menu()->setTitle(tr("QNVim"));
    menu->addAction(cmd);
    Core::ActionManager::actionContainer(Core::Constants::M_TOOLS)->addMenu(menu);

    connect(Core::EditorManager::instance(), &Core::EditorManager::editorAboutToClose,
            this, &QNVimPlugin::editorAboutToClose);
    connect(Core::EditorManager::instance(), &Core::EditorManager::currentEditorChanged,
            this, &QNVimPlugin::editorOpened);

    mNumbersColumn = new NumbersColumn();
    mNVim = NeovimQt::NeovimConnector::spawn(QStringList() << "--cmd" << "let g:QNVIM=1",
            "nvim");
    connect(mNVim, &NeovimQt::NeovimConnector::ready, [=]() {
        mNVim->api2()->nvim_command(mNVim->encode(QString("\
let g:neovim_channel=%1\n\
execute \"command -bar Build call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Build')\"\n\
execute \"command -bar BuildProject call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Build')\"\n\
execute \"command -bar BuildAll call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.BuildSession')\"\n\
execute \"command -bar Rebuild call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Rebuild')\"\n\
execute \"command -bar RebuildProject call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Rebuild')\"\n\
execute \"command -bar RebuildAll call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.RebuildSession')\"\n\
execute \"command -bar Clean call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Clean')\"\n\
execute \"command -bar CleanProject call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Clean')\"\n\
execute \"command -bar CleanAll call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.CleanSession')\"\n\
execute \"command -bar Deploy call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Deploy')\"\n\
execute \"command -bar DeployProject call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Deploy')\"\n\
execute \"command -bar DeployAll call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.DeploySession')\"\n\
execute \"command -bar Run call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Run')\"\n\
execute \"command -bar Debug call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Debug')\"\n\
execute \"command -bar DebugStart call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Debug')\"\n\
execute \"command -bar DebugContinue call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Continue')\"\n\
execute \"command -bar QMake call rpcnotify(%1, 'Gui', 'triggerCommand', 'Qt4Builder.RunQMake')\"\n\
execute \"command -bar Target call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.SelectTargetQuick')\"\n\
\
execute \"autocmd BufReadCmd * :call rpcnotify(%1, 'Gui', 'fileAutoCommand', 'BufReadCmd', expand('<abuf>'), expand('<afile>:p'), &buftype, &buflisted, &bufhidden)\"\n\
execute \"autocmd TermOpen * :call rpcnotify(%1, 'Gui', 'fileAutoCommand', 'TermOpen', expand('<abuf>'), expand('<afile>:p'), &buftype, &buflisted, &bufhidden)\"\n\
execute \"autocmd BufWriteCmd * :call rpcnotify(%1, 'Gui', 'fileAutoCommand', 'BufWriteCmd', expand('<abuf>'), expand('<afile>:p'), &buftype, &buflisted, &bufhidden)|set nomodified\"\n\
execute \"autocmd BufEnter * nested :call rpcnotify(%1, 'Gui', 'fileAutoCommand', 'BufEnter', expand('<abuf>'), expand('<afile>:p'), &buftype, &buflisted, &bufhidden)\"\n\
execute \"autocmd BufDelete * nested :call rpcnotify(%1, 'Gui', 'fileAutoCommand', 'BufDelete', expand('<abuf>'), expand('<afile>:p'), &buftype, &buflisted, &bufhidden)\"\n\
execute \"autocmd BufHidden * nested :call rpcnotify(%1, 'Gui', 'fileAutoCommand', 'BufHidden', expand('<abuf>'), expand('<afile>:p'), &buftype, &buflisted, &bufhidden)\"\n\
execute \"autocmd BufWipeout * nested :call rpcnotify(%1, 'Gui', 'fileAutoCommand', 'BufWipeout', expand('<abuf>'), expand('<afile>:p'), &buftype, &buflisted, &bufhidden)\"\n\
execute \"autocmd FileType help echom 123|set modifiable|read <afile>|set nomodifiable\"\n\
\
function! SetCursor(line, col)\n\
    call cursor(a:line, a:col)\n\
    if mode()[0] ==# 'i' or mode()[0] ==# 'R'\n\
        normal! i\x07u\x03\n\
    endif\n\
    call cursor(a:line, a:col)\n\
endfunction\n\
source ~/.qnvimrc").arg(mNVim->channel())));
        connect(mNVim->api2(), &NeovimQt::NeovimApi2::neovimNotification,
                this, &QNVimPlugin::handleNotification);

        QVariantMap options;
        options.insert("ext_popupmenu", true);
        options.insert("ext_tabline", false);
        options.insert("ext_cmdline", true);
        options.insert("ext_multigrid", true);
        options.insert("ext_wildmenu", true);
        options.insert("rgb", true);
        NeovimQt::MsgpackRequest *req = mNVim->api2()->nvim_ui_attach(mWidth, mHeight, options);
        connect(req, &NeovimQt::MsgpackRequest::timeout, mNVim, &NeovimQt::NeovimConnector::fatalTimeout);
        connect(req, &NeovimQt::MsgpackRequest::timeout, [=]() {
            qWarning() << "THE FUCK HAPPENED!";
        });
        // FIXME grab timeout from connector
        req->setTimeout(10000);
        connect(req, &NeovimQt::MsgpackRequest::finished, [=]() {
            qWarning() << "ATTACHED!";
        });

        mNVim->api2()->vim_subscribe("Gui");
    });

    return true;
}

void QNVimPlugin::extensionsInitialized() {
    // Retrieve objects from the plugin manager's object pool
    // In the extensionsInitialized function, a plugin can be sure that all
    // plugins that depend on it are completely initialized.
}

ExtensionSystem::IPlugin::ShutdownFlag QNVimPlugin::aboutToShutdown() {
    mEnabled = false;
    // Save settings
    // Disconnect from signals that are not needed during shutdown
    // Hide UI (if you add UI that is not in the main window directly)
    return SynchronousShutdown;
}

bool QNVimPlugin::eventFilter(QObject *object, QEvent *event) {
    if (not mEnabled)
        return false;
    /* if (qobject_cast<QLabel *>(object)) */
    if (qobject_cast<TextEditor::TextEditorWidget *>(object) or qobject_cast<QPlainTextEdit *>(object)) {
        if (event->type() == QEvent::Resize) {
            QTimer::singleShot(100, [=]() { fixSize(); });
            return false;
        }
    }
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
        QString text = keyEvent->text();
#ifdef Q_OS_MAC
        if (QChar(keyEvent->key()).isLetterOrNumber())
            text = modifiers & Qt::ShiftModifier ? QChar(keyEvent->key()) : QChar(keyEvent->key()).toLower();
#endif
        QString key = mInputConv->convertKey(text, keyEvent->key(), modifiers);
        mNVim->api2()->nvim_input(mNVim->encode(key));
        qWarning() << key;
        return true;
    }
    else if (event->type() == QEvent::ShortcutOverride) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
        QString text = keyEvent->text();
#ifdef Q_OS_MAC
        if (modifiers & Qt::AltModifier and QChar(keyEvent->key()).isLetterOrNumber())
            text = modifiers & Qt::ShiftModifier ? QChar(keyEvent->key()) : QChar(keyEvent->key()).toLower();
#endif
        QString key = mInputConv->convertKey(text, keyEvent->key(), modifiers);
        if (keyEvent->key() == Qt::Key_Escape) {
            mNVim->api2()->nvim_input(mNVim->encode(key));
            qWarning() << "ESCAPE" << key;
        }
        else
            keyEvent->accept();
        return true;
    }
    return false;
}

void QNVimPlugin::toggleQNVim() {
    qWarning() << "QNVimPlugin::toggleQNVim";
    mEnabled = not mEnabled;
    if (mEnabled) {
        this->initialize();
    }
    else {
        mNumbersColumn->deleteLater();
        connect(mNVim->api2()->nvim_command("q!"), &NeovimQt::MsgpackRequest::finished,
                [=]() {
            mNVim->deleteLater();
            mNVim = nullptr;
        });
        disconnect(Core::EditorManager::instance(), &Core::EditorManager::editorAboutToClose,
                this, &QNVimPlugin::editorAboutToClose);
        disconnect(Core::EditorManager::instance(), &Core::EditorManager::currentEditorChanged,
                this, &QNVimPlugin::editorOpened);
        for(auto key: mEditors.keys()) {
            Core::IEditor *editor = mEditors[key];
            if (not editor)
                continue;

            QWidget *widget = editor->widget();
            if (not widget)
                continue;

            if (not qobject_cast<TextEditor::TextEditorWidget *>(widget))
                continue;

            TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(widget);
            textEditor->setCursorWidth(1);
            widget->removeEventFilter(this);
            mEditors.remove(key);
        }
        mBuffers.clear();
        mFilenames.clear();
        mInitialized.clear();
        mChangedTicks.clear();
        mBufferType.clear();
    }
}

void QNVimPlugin::editorOpened(Core::IEditor *editor) {
    if (not mEnabled)
        return;

    if (not editor)
        return;

    QString filename(this->filename(editor));
    qWarning() << "Opened " << filename << settingBufferFromVim;

    mInitialized[filename] = false;

    QWidget *widget = editor->widget();
    if (not widget)
        return;

    if (not qobject_cast<TextEditor::TextEditorWidget *>(widget))
        return;
    TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());

    if (mEditors.contains(filename)) {
        mInitialized[filename] = true;
        if (not settingBufferFromVim)
            mNVim->api2()->nvim_command(mNVim->encode(QString("buffer %1").arg(mBuffers[filename])));
    }
    else {
        mEditors[filename] = editor;
        if (mNVim and mNVim->isReady()) {
            if (settingBufferFromVim) {
                if (mBuffers.contains(filename)) {
                    initializeBuffer(mBuffers[filename], filename);
                }
            }
            else {
                if (mBuffers.contains(filename)) {
                    connect(mNVim->api2()->nvim_eval(mNVim->encode(QString("[execute('buffer %1')]").arg(mBuffers[filename]))),
                            &NeovimQt::MsgpackRequest::finished, [=]() {
                        initializeBuffer(mBuffers[filename], filename);
                    });
                }
                else {
                    connect(mNVim->api2()->nvim_eval(mNVim->encode(QString("[execute('e %1'), bufnr('')]").arg(filename))),
                            &NeovimQt::MsgpackRequest::finished, [=](quint32, quint64, const QVariant &v) {
                        mBuffers[filename] = v.toList()[1].toInt();
                        mFilenames[mBuffers[filename]] = filename;
                    });
                }
            }
        }
    }
    settingBufferFromVim = false;

    mNumbersColumn->setEditor(textEditor);

    widget->setAttribute(Qt::WA_KeyCompression, false);
    widget->installEventFilter(this);
    Core::IDocument *document = editor->document();

    connect(document, &Core::IDocument::contentsChanged, this, [=]() {
        QString bufferType = mBufferType[mBuffers[filename]];
        if (not mInitialized[filename] or (bufferType != "acwrite" and not bufferType.isEmpty()))
            return;
        if (Core::EditorManager::currentEditor() != editor)
            return;
        QString newText = textEditor->toPlainText();
        if (newText == mText)
            return;
        syncToVim(editor);
    }, Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));
    connect(textEditor, &TextEditor::TextEditorWidget::cursorPositionChanged, this, [=]() {
        if (Core::EditorManager::currentEditor() != editor)
            return;
        QString newText = textEditor->toPlainText();
        if (newText != mText)
            return;
        syncCursorToVim(editor);
    }, Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));
    connect(textEditor, &TextEditor::TextEditorWidget::selectionChanged, this, [=]() {
        if (Core::EditorManager::currentEditor() != editor)
            return;
        QString newText = textEditor->toPlainText();
        if (newText != mText)
            return;
        syncSelectionToVim(editor);
    }, Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));

    QTimer::singleShot(100, [=]() { fixSize(editor); });
}

void QNVimPlugin::editorAboutToClose(Core::IEditor *editor) {
    qWarning() << "QNVimPlugin::editorAboutToClose";
    if (not mBuffers.contains(filename(editor)))
        return;
    if (Core::EditorManager::currentEditor() == editor)
        mNumbersColumn->setEditor(nullptr);
    int bufferNumber = mBuffers[filename(editor)];
    mNVim->api2()->nvim_command(mNVim->encode(QString("bd! %1").arg(mBuffers[filename(editor)])));
    mBuffers.remove(filename(editor));
    mEditors.remove(filename(editor));
    mFilenames.remove(bufferNumber);
    mInitialized.remove(filename(editor));
    mChangedTicks.remove(bufferNumber);
    mBufferType.remove(bufferNumber);
}

void QNVimPlugin::initializeBuffer(long buffer, QString filename) {
    QString bufferType = mBufferType[buffer];
    if (bufferType == "acwrite" or bufferType.isEmpty()) {
        connect(mNVim->api2()->nvim_buf_set_option(buffer, "undolevels", -1),
                &NeovimQt::MsgpackRequest::finished, this, [=]() {
            syncToVim(mEditors[filename], [=]() {
                mNVim->api2()->nvim_buf_set_option(buffer, "undolevels", -123456);
                mNVim->api2()->nvim_buf_set_option(buffer, "modified", false);
                if (bufferType.isEmpty())
                    mNVim->api2()->nvim_buf_set_option(mBuffers[filename], "buftype", "acwrite");
                mInitialized[filename] = true;
            });
        }, Qt::DirectConnection);
    }
    else {
        mNVim->api2()->nvim_buf_set_option(buffer, "modified", false);
        mInitialized[filename] = true;
    }
}

void QNVimPlugin::handleNotification(const QByteArray &name, const QVariantList &args) {
    Core::IEditor *editor = Core::EditorManager::currentEditor();
    if (not editor or not mEditors.contains(filename(editor)))
        return;
    if (name == "Gui") {
        qWarning() << args;
        QByteArray method = args.first().toByteArray();
        QVariantList methodArgs = args.mid(1);
        if (method == "triggerCommand") {
            for (auto methodArg: methodArgs)
                triggerCommand(methodArg.toByteArray());
        }
        else if (method == "fileAutoCommand") {
            QByteArray cmd = methodArgs.first().toByteArray();
            long buffer = methodArgs[1].toByteArray().toLong();
            QString filename = mNVim->decode(methodArgs[2].toByteArray());
            QString bufferType = mNVim->decode(methodArgs[3].toByteArray());
            bool bufferListed = methodArgs[4].toInt();
            QString bufferHidden = mNVim->decode(methodArgs[5].toByteArray());

            if (cmd == "BufReadCmd" or cmd == "TermOpen") {
                mBufferType[buffer] = bufferType;
                if (mEditors.contains(filename)) {
                    initializeBuffer(buffer, filename);
                }
                else {
                    mBuffers[filename] = buffer;
                    if (cmd == "TermOpen")
                        mNVim->api2()->nvim_command("doautocmd BufEnter");
                }
            }
            else if (cmd == "BufWriteCmd") {
                if (mBuffers.values().contains(buffer)) {
                    QString currentFilename = mFilenames[buffer];
                    if (mEditors[currentFilename]->document()->save(nullptr, filename)) {
                        if (currentFilename != filename) {
                            mEditors.remove(currentFilename);
                            mFilenames.remove(mBuffers[currentFilename]);
                            mChangedTicks.remove(mBuffers[currentFilename]);
                            mBuffers.remove(currentFilename);
                            mInitialized.remove(currentFilename);
                            connect(mNVim->api2()->nvim_buf_set_name(buffer, mNVim->encode(filename)),
                                    &NeovimQt::MsgpackRequest::finished, [=](quint32, quint64, const QVariant &) {
                                mNVim->api2()->nvim_command("edit!");
                            });
                        }
                        else
                            mNVim->api2()->nvim_buf_set_option(buffer, "modified", false);
                    }
                    else
                        mNVim->api2()->nvim_buf_set_option(buffer, "modified", true);
                }
            }
            else if (cmd == "BufEnter") {
                mBufferType[buffer] = bufferType;
                if (not filename.isEmpty() and filename != this->filename(editor)) {
                    if (mEditors.contains(filename)) {
                        if (Core::EditorManager::currentEditor() != mEditors[filename]) {
                            settingBufferFromVim = true;
                            Core::EditorManager::activateEditor(mEditors[filename]);
                        }
                    }
                    else {
                        qWarning() << buffer << filename << bufferType;
                        if (bufferType.isEmpty())
                            Core::EditorManager::openEditor(filename);
                        else {
                            Core::IEditor *editor(nullptr);
                            if (bufferType == "terminal")
                                editor = Core::EditorManager::openEditorWithContents("Terminal", &filename, QByteArray(), filename);
                            else if (bufferType == "help") {
                                editor = Core::EditorManager::openEditorWithContents("Help", &filename, QByteArray(), "vim://help");
                                editor->document()->setFilePath(Utils::FileName::fromString(filename));
                                editor->document()->setPreferredDisplayName("Vim Help");
                                editor->document()->setTemporary(true);
                            }
                            else if (bufferType == "nowrite" or bufferType == "nofile") {
                                editor = Core::EditorManager::openEditorWithContents("Help", &filename, QByteArray(), "vim://help");
                                editor->document()->setFilePath(Utils::FileName::fromString(filename));
                                editor->document()->setPreferredDisplayName(filename);
                                editor->document()->setTemporary(true);
                            }
                        }
                    }
                }
            }
            else if (cmd == "BufDelete") {
                if (bufferListed and mEditors.contains(filename) and mEditors[filename]) {
                    if (Core::EditorManager::currentEditor() == mEditors[filename])
                        settingBufferFromVim = true;
                    Core::EditorManager::closeEditor(mEditors[filename]);
                }
            }
            else if (cmd == "BufHidden") {
                if (
                    (bufferHidden == "wipe" or bufferHidden == "delete" or bufferHidden == "unload" or bufferType == "help") and
                    mEditors.contains(filename) and mEditors[filename]
                ) {
                    if (Core::EditorManager::currentEditor() == mEditors[filename])
                        settingBufferFromVim = true;
                    Core::EditorManager::closeEditor(mEditors[filename]);
                }
            }
            else if (cmd == "BufWipeout") {
                if (!bufferListed and mEditors.contains(filename) and mEditors[filename]) {
                    if (Core::EditorManager::currentEditor() == mEditors[filename])
                        settingBufferFromVim = true;
                    Core::EditorManager::closeEditor(mEditors[filename]);
                }
            }
        }
    }
    else if (name == "redraw")
        redraw(args);
}

void QNVimPlugin::redraw(const QVariantList &args) {
    Core::IEditor *editor = Core::EditorManager::currentEditor();
    TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    bool shouldSync = false;
    bool flush = false;
    for(auto arg: args) {
        QVariantList line = arg.toList();
        QByteArray command = line.first().toByteArray();
        QVariantList args = line.mid(1).first().toList();
        if (not command.startsWith("cmdline") and command != "flush")
            shouldSync = true;
        if (command == "flush")
            flush = true;

        if (command == "bell") {
            QApplication::beep();
        }
        else if (command == "mode_change") {
            mUIMode = args.first().toByteArray();
        }
        else if (command == "busy_start") {
            mBusy = true;
        }
        else if (command == "busy_stop") {
            mBusy = false;
        }
        else if (command == "mouse_on") {
            mMouse = true;
        }
        else if (command == "mouse_off") {
            mMouse = false;
        }
        else if (command == "grid_resize") {
            if (line.first().toInt() == 1) {
                mWidth = args[0].toInt();
                mHeight = args[1].toInt();
            }
        }
        else if (command == "default_colors_set") {
            qint64 val = args[0].toLongLong();
            if (val != -1) {
                mForegroundColor = QRgb(val);
                QPalette palette = textEditor->palette();
                palette.setColor(QPalette::Foreground, mForegroundColor);
                textEditor->setPalette(palette);
            }

            val = args[1].toLongLong();
            if (val != -1) {
                mBackgroundColor = QRgb(val);
                QPalette palette = textEditor->palette();
                palette.setBrush(QPalette::Background, mBackgroundColor);
                textEditor->setPalette(palette);
            }

            val = args[2].toLongLong();
            if (val != -1) {
                mSpecialColor = QRgb(val);
            }
        }
        else if (command == "cmdline_show") {
            mCMDLineVisible = true;
            QVariantList contentList = args[0].toList();
            mCMDLineContent = "";
            for (auto contentItem: contentList)
                mCMDLineContent += mNVim->decode(contentItem.toList()[1].toByteArray());
            mCMDLinePos = args[1].toInt();
            mCMDLineFirstc = args[2].toString()[0];
            mCMDLinePrompt = mNVim->decode(args[3].toByteArray());
            mCMDLineIndent = args[4].toInt();
        }
        else if (command == "cmdline_pos") {
            mCMDLinePos = args[0].toInt();
        }
        else if (command == "cmdline_hide") {
            mCMDLineVisible = false;
        }
        else {
        }
    }

    if (shouldSync and flush)
        syncFromVim();

    if (mBusy) {
        if (textEditor->cursorWidth() != 0)
            textEditor->setCursorWidth(0);
    }
    else if (mUIMode == "insert" or mUIMode == "visual") {
        if (textEditor->cursorWidth() != 1)
            textEditor->setCursorWidth(1);
    }
    else if (mUIMode == "normal" or mUIMode == "operator") {
        if (textEditor->cursorWidth() != 11)
            textEditor->setCursorWidth(11);
    }

    if (mCMDLineVisible) {
        QFontMetrics fm(mCMDLine->font());
        QString text = mCMDLineFirstc + mCMDLinePrompt + QString(mCMDLineIndent, ' ') + mCMDLineContent;
        if (mCMDLine->toPlainText() != text)
            mCMDLine->setPlainText(text);
        if (mCMDLine->minimumWidth() != qMax(200, qMin(qCeil(fm.width(mCMDLine->toPlainText())) + 10, 400)))
            mCMDLine->setMinimumWidth(qMax(200, qMin(qCeil(fm.width(mCMDLine->toPlainText())) + 10, 400)));
        if (not mCMDLine->hasFocus())
            mCMDLine->setFocus();
        QTextCursor cursor = mCMDLine->textCursor();
        if (cursor.position() != (QString(mCMDLineFirstc + mCMDLinePrompt).length() + mCMDLineIndent + mCMDLinePos)) {
            cursor.setPosition(QString(mCMDLineFirstc + mCMDLinePrompt).length() + mCMDLineIndent + mCMDLinePos);
            mCMDLine->setTextCursor(cursor);
        }
        if (mUIMode == "cmdline_normal") {
            if (mCMDLine->cursorWidth() != 1)
                mCMDLine->setCursorWidth(1);
        }
        else if (mUIMode == "cmdline_insert") {
            if (mCMDLine->cursorWidth() != 11)
                mCMDLine->setCursorWidth(11);
        }
    }
    else {
        if (mCMDLine->toPlainText() != mCMDLineDisplay)
            mCMDLine->setPlainText(mCMDLineDisplay);
        if (mCMDLine->hasFocus())
            textEditor->setFocus();
    }
}


HelpEditorFactory::HelpEditorFactory()
    : PlainTextEditorFactory() {
    setId("Help");
    setDisplayName("Help");
    addMimeType("text/plain");
}

TerminalEditorFactory::TerminalEditorFactory()
    : PlainTextEditorFactory() {
    setId("Terminal");
    setDisplayName("Terminal");
    addMimeType("text/plain");
}

} // namespace Internal
} // namespace QNVim
