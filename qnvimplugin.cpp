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
#include <coreplugin/locator/locatormanager.h>
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

#include <QScrollBar>
#include <QLabel>
#include <QtMath>
#include <QTextEdit>
#include <QApplication>
#include <QGuiApplication>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QAction>
#include <QMessageBox>
#include <QMainWindow>
#include <QMenu>
#include <QPainter>

namespace QNVim {
namespace Internal {

NumbersColumn::NumbersColumn(): mNumber(false), mEditor(NULL) {
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

    QFontMetrics fm(mEditor->textDocument()->fontSettings().font());
    unsigned lineHeight = fm.lineSpacing();
    QRect rect(0, mEditor->cursorRect(firstVisibleCursor).y(), width(), lineHeight);
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
    unsigned lineHeight = fm.lineSpacing();
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

QNVimPlugin::QNVimPlugin(): mEnabled(true), mCMDLine(NULL), mNumbersColumn(NULL), mNVim(NULL),
    mInputConv(new NeovimQt::InputConv), mVimChanges(0),
    mText(""), mWidth(80), mHeight(35),
    mForegroundColor(Qt::black), mBackgroundColor(Qt::white), mSpecialColor(QColor()),
    mCursorColor(Qt::white), mBusy(false), mMouse(false),
    mNumber(true), mRelativeNumber(true), mWrap(false),
    mCMDLineVisible(false), mUIMode("normal"), mMode("n") {
}

QNVimPlugin::~QNVimPlugin() {
    if (mCMDLine)
        Core::StatusBarManager::destroyStatusBarWidget(mCMDLine);
    if (mNVim)
        mNVim->deleteLater();
}

QString QNVimPlugin::filename(Core::IEditor *editor) const {
    if (not editor)
        editor = Core::EditorManager::currentEditor();
    if (not editor)
        return "";
    return editor->document()->filePath().toString();
}

void QNVimPlugin::fixSize(Core::IEditor *editor) {
    if (not editor)
        editor = Core::EditorManager::currentEditor();
    if (not editor or not mEditors.contains(filename(editor)))
        return;
    TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    QFontMetricsF fm(textEditor->textDocument()->fontSettings().font());
    unsigned width = qFloor((textEditor->width() - textEditor->extraArea()->width()) / fm.width('A')) + 2;
    unsigned height = qFloor(textEditor->height() / fm.lineSpacing());
    qWarning() << width << height;
    if (width != mWidth or height != mHeight)
        mNVim->api2()->nvim_ui_try_resize(width, height);
}

void QNVimPlugin::syncCursorToVim(Core::IEditor *editor, bool force) {
    if (not editor)
        editor = Core::EditorManager::currentEditor();
    if (not editor or not mEditors.contains(filename(editor)))
        return;
    TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    if (mMode == "v" or mMode == "V" or mMode == "\x16" or
            textEditor->textCursor().position() != textEditor->textCursor().anchor())
        return;
    QString text = textEditor->toPlainText();
    unsigned cursorPosition = textEditor->textCursor().position();
    int line = text.left(cursorPosition).count('\n') + 1;
    int col = mNVim->encode(text.left(cursorPosition).section('\n', -1)).length() + 1;
    if (line == mCursor.y() and col == mCursor.x()) {
        return;
    }
    qWarning() << "TRYING TO SYNC CURSOR TO NEOVIM";
    // if (force)
    //     mSyncMutex.lock();
    // else if (not mSyncMutex.tryLock())
    //     return;
    mCursor.setY(line);
    mCursor.setX(col);
    qWarning() << "SYNCING CURSOR TO NEOVIM" << mCursor << cursorPosition;
    connect(mNVim->api2()->nvim_command(mNVim->encode(QString("call SetCursor(%1,%2)").arg(line).arg(col))),
            &NeovimQt::MsgpackRequest::finished, [=]() {
        // mSyncMutex.unlock();
        qWarning() << "CURSOR SYNCED TO NEOVIM";
    });
}

void QNVimPlugin::syncSelectionToVim(Core::IEditor *editor, bool force) {
    if (not editor)
        editor = Core::EditorManager::currentEditor();
    if (not editor or not mEditors.contains(filename(editor)))
        return;
    // qWarning() << "TRYING TO SYNC SELECTION TO NEOVIM";
    // if (force)
    //     mSyncMutex.lock();
    // else if (not mSyncMutex.tryLock())
    //     return;
    // qWarning() << "SYNCING SELECTION TO NEOVIM";
    TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    QString text = textEditor->toPlainText();
    QTextCursor cursor = textEditor->hasBlockSelection() ? textEditor->blockSelection() : textEditor->textCursor();
    unsigned cursorPosition = cursor.position();
    unsigned anchorPosition = cursor.anchor();
    unsigned line, col, vLine, vCol;
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
                mNVim->encode(QString("normal! \x03%2G%3|%1%4G%5|").arg(visualCommand)
                              .arg(vLine).arg(vCol).arg(line).arg(col))),
            &NeovimQt::MsgpackRequest::finished, [=]() {
    });

    // mSyncMutex.unlock();
    // qWarning() << "SELECTION SYNCED TO NEOVIM";
}

void QNVimPlugin::syncToVim(Core::IEditor *editor, bool force, std::function<void()> callback) {
    if (not editor)
        editor = Core::EditorManager::currentEditor();
    if (not editor or not mEditors.contains(filename(editor)))
        return;
    TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    QString text = textEditor->toPlainText();
    unsigned cursorPosition = textEditor->textCursor().position();
    int line = text.left(cursorPosition).count('\n') + 1;
    int col = mNVim->encode(text.left(cursorPosition).section('\n', -1)).length() + 1;
    qWarning() << "SYNC" << line << col;

    diff_match_patch differ;
    QList<Diff> diffs = differ.diff_main(mText, text);
    differ.diff_cleanupEfficiency(diffs);
    QList<Patch> patches = differ.patch_make(mText, diffs);

    QPoint cursor = mCursor;
    QString patchCommand = "set paste|call execute('normal! ";
    std::reverse(std::begin(patches), std::end(patches));
    for (auto patch: patches) {
        int startLine = mText.left(patch.start1).count('\n') + 1;
        int startCol = mNVim->encode(mText.left(patch.start1).section('\n', -1)).length() + 1;
        int endLine = mText.left(patch.start1 + patch.length1 - 1).count('\n') + 1;
        int endCol = mNVim->encode(mText.left(patch.start1 + patch.length1 - 1).section('\n', -1)).length() + 1;
        patchCommand += QString("%1G%2|v%3G%4|c%5\x03").arg(startLine).arg(startCol).arg(endLine).arg(endCol).
                arg(text.mid(patch.start2, patch.length2).replace("'", "''"));
    }
    patchCommand += QString("i\x07u\x03')|set nopaste|call cursor(%1,%2)\n").arg(line).arg(col);
    mCursor.setY(line);
    mCursor.setX(col);
    connect(mNVim->api2()->nvim_command(mNVim->encode(patchCommand)),
            &NeovimQt::MsgpackRequest::finished, [=]() {
        if (callback)
            callback();
    });
}

void QNVimPlugin::syncFromVim(bool force) {
    Core::IEditor *editor = Core::EditorManager::currentEditor();
    if (not editor or not mEditors.contains(filename(editor)))
        return;
    if (not mInitialized[filename(editor)])
        return;
    TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    connect(mNVim->api2()->nvim_command(mNVim->encode(QString("buffer %1").arg(mBuffers[filename(editor)]))),
            &NeovimQt::MsgpackRequest::finished, [=]() {
        connect(mNVim->api2()->nvim_eval("[bufnr(''), mode(1), &modified, getpos('.'), getpos('v'), &number, &relativenumber, &wrap, execute('1messages')]"),
                &NeovimQt::MsgpackRequest::finished, [=](quint32, quint64, const QVariant &v) {
            if (not mEditors.contains(filename(editor))) {
                return;
            }
            QVariantList state = v.toList();
            if (state[0].toString().toULongLong() != mBuffers[filename(editor)]) {
                return;
            }

            QByteArray mode = state[1].toByteArray();
            bool modified = state[2].toBool();
            QVariantList pos = state[3].toList().mid(1, 2);
            QVariantList vPos = state[4].toList().mid(1, 2);
            mNumber = state[5].toBool();
            mRelativeNumber = state[6].toBool();
            mWrap = state[7].toBool();
            QString cmdLine = mNVim->decode(state[8].toByteArray()).mid(1);
            if (not cmdline) {
                mCMDLineDisplay = cmdLine;
                if (not mCMDLineVisible)
                    mCMDLine->setPlainText(mCMDLineDisplay);
            }
            mNumbersColumn->setNumber(mNumber);
            mNumbersColumn->setEditor(mRelativeNumber ? textEditor : NULL);
            if (textEditor->wordWrapMode() != (mWrap ? QTextOption::WrapAnywhere : QTextOption::NoWrap))
                textEditor->setWordWrapMode(mWrap ? QTextOption::WrapAnywhere : QTextOption::NoWrap);
            connect(mNVim->api2()->nvim_buf_get_lines(mBuffers[filename(editor)], 0, -1, true),
                    &NeovimQt::MsgpackRequest::finished, [=](quint32, quint64, const QVariant &lines) {
                if (not mEditors.contains(filename(editor))) {
                    return;
                }

                mText = "";
                for (auto t: lines.toList())
                    mText += mNVim->decode(t.toByteArray()) + '\n';
                mText.chop(1);
                QString oldText = textEditor->toPlainText();
                diff_match_patch differ;
                QList<Diff> diffs = differ.diff_main(oldText, mText);
                differ.diff_cleanupEfficiency(diffs);
                QList<Patch> patches = differ.patch_make(oldText, diffs);

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
                unsigned line = pos[0].toULongLong();
                unsigned col = pos[1].toULongLong();
                col = mNVim->decode(mNVim->encode(mText.section('\n', line - 1, line - 1)).left(col - 1)).length() + 1;

                unsigned vLine = vPos[0].toULongLong();
                unsigned vCol = vPos[1].toULongLong();
                vCol = mNVim->decode(mNVim->encode(mText.section('\n', vLine - 1, vLine - 1)).left(vCol)).length();

                if (mCursor.y() == line and mCursor.x() == col and mVCursor.y() == vLine and mVCursor.x() == vCol and mMode == mode)
                    return;
                mMode = mode;
                mCursor.setY(line);
                mCursor.setX(col);
                mVCursor.setY(vLine);
                mVCursor.setX(vCol);

                unsigned a = QString("\n" + mText).section('\n', 0, vLine - 1).length() + vCol - 1;
                unsigned p = QString("\n" + mText).section('\n', 0, line - 1).length() + col - 1;
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
            });
        });
    });
}

void QNVimPlugin::triggerCommand(const QByteArray &commandId) {
    Core::ActionManager::command(commandId.constData())->action()->trigger();
}

bool QNVimPlugin::initialize(const QStringList &arguments, QString *errorString) {
    Q_UNUSED(arguments)
    Q_UNUSED(errorString)

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

    return initialize();
}

bool QNVimPlugin::initialize() {
    auto action = new QAction(tr("QNVim Action"), this);
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
            "/usr/local/bin/nvim");
    connect(mNVim, &NeovimQt::NeovimConnector::ready, [=]() {
        mNVim->api2()->nvim_command(mNVim->encode(QString("\
nnoremap <silent> == :call rpcnotify(%1, 'Gui', 'triggerCommand', 'TextEditor.AutoIndentSelection')<cr>\n\
vnoremap <silent> = :call rpcnotify(%1, 'Gui', 'triggerCommand', 'TextEditor.AutoIndentSelection')<cr>\n\
nnoremap <silent> <c-]> :call rpcnotify(%1, 'Gui', 'triggerCommand', 'TextEditor.FollowSymbolUnderCursor', 'TextEditor.JumpToFileUnderCursor')<cr>\n\
nnoremap <silent> <f4> :call rpcnotify(%1, 'Gui', 'triggerCommand', 'CppTools.SwitchHeaderSource')<cr>\n\
nnoremap <silent> <tab> :call rpcnotify(%1, 'Gui', 'triggerCommand', 'QtCreator.GotoPreviousInHistory')<cr>\n\
nnoremap <silent> <shift-tab> :call rpcnotify(%1, 'Gui', 'triggerCommand', 'QtCreator.GotoPreviousInHistory')<cr>\n\
inoremap <silent> <expr> <c-space> rpcnotify(%1, 'Gui', 'triggerCommand', 'TextEditor.CompleteThis') ? '<left><right>' : '<left><right>'\"\n\
\
nnoremap <silent> <d-1> :call rpcnotify(%1, 'Gui', 'triggerCommand', 'QtCreator.Pane.Issues')<cr>\n\
nnoremap <silent> <d-2> :call rpcnotify(%1, 'Gui', 'triggerCommand', 'QtCreator.Pane.SearchResults')<cr>\n\
nnoremap <silent> <d-3> :call rpcnotify(%1, 'Gui', 'triggerCommand', 'QtCreator.Pane.ApplicationOutput')<cr>\n\
nnoremap <silent> <d-4> :call rpcnotify(%1, 'Gui', 'triggerCommand', 'QtCreator.Pane.CompileOutput')<cr>\n\
nnoremap <silent> <d-5> :call rpcnotify(%1, 'Gui', 'triggerCommand', 'QtCreator.Pane.DebuggerConsole')<cr>\n\
nnoremap <silent> <d-6> :call rpcnotify(%1, 'Gui', 'triggerCommand', 'QtCreator.Pane.To-DoEntries')<cr>\n\
nnoremap <silent> <d-7> :call rpcnotify(%1, 'Gui', 'triggerCommand', 'QtCreator.Pane.GeneralMessages')<cr>\n\
nnoremap <silent> <d-8> :call rpcnotify(%1, 'Gui', 'triggerCommand', 'QtCreator.Pane.VersionControl')<cr>\n\
\
nnoremap <silent> <f1> :call rpcnotify(%1, 'Gui', 'triggerCommand', 'Help.Context')<cr>\n\
\
execute \"command Build :call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Build')\"\n\
execute \"command BuildProject :call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Build')\"\n\
execute \"command BuildAll :call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.BuildSession')\"\n\
execute \"command Rebuild :call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Rebuild')\"\n\
execute \"command RebuildProject :call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Rebuild')\"\n\
execute \"command RebuildAll :call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.RebuildSession')\"\n\
execute \"command Clean :call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Clean')\"\n\
execute \"command CleanProject :call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Clean')\"\n\
execute \"command CleanAll :call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.CleanSession')\"\n\
execute \"command Run :call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Run')\"\n\
execute \"command Debug :call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Debug')\"\n\
execute \"command DebugStart :call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Debug')\"\n\
execute \"command DebugContinue :call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Continue')\"\n\
\
execute \"autocmd BufReadCmd * nested :call rpcnotify(%1, 'Gui', 'fileAutoCommand', 'BufReadCmd', expand('<abuf>'), expand('<afile>'))\"\n\
execute \"autocmd BufWriteCmd * nested :call rpcnotify(%1, 'Gui', 'fileAutoCommand', 'BufWriteCmd', expand('<abuf>'), expand('<afile>'))|set nomodified\"\n\
execute \"autocmd BufEnter * nested :call rpcnotify(%1, 'Gui', 'fileAutoCommand', 'BufEnter', expand('<abuf>'), expand('<afile>'))\"\n\
\
function! SetCursor(line, col)\n\
    call cursor(a:line, a:col)\n\
    if mode()[0] ==# 'i' or mode()[0] ==# 'R'\n\
        normal! i\x07u\x03\n\
    endif\n\
    call cursor(a:line, a:col)\n\
endfunction").arg(mNVim->channel())));
        connect(mNVim->api2(), &NeovimQt::NeovimApi2::neovimNotification,
                this, &QNVimPlugin::handleNotification);
        connect(mNVim->api2(), &NeovimQt::NeovimApi2::neovimNotification,
                this, [=]() {syncFromVim();});

        QVariantMap options;
        options.insert("ext_popupmenu", true);
        options.insert("ext_tabline", false);
        options.insert("ext_cmdline", true);
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
            fixSize();
            return false;
        }
    }
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
        // qWarning() << keyEvent->text() << keyEvent->key();
        QString text = keyEvent->text();
#ifdef Q_OS_MAC
        if (modifiers & Qt::AltModifier and QChar(keyEvent->key()).isLetterOrNumber())
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
        // qWarning() << keyEvent->text() << keyEvent->key();
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
    mEnabled = not mEnabled;
    if (mEnabled) {
        this->initialize();
    }
    else {
        mNumbersColumn->deleteLater();
        connect(mNVim->api2()->nvim_command("q!"), &NeovimQt::MsgpackRequest::finished,
                [=]() {
            mNVim->deleteLater();
            mNVim = NULL;
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
            mBuffers.remove(key);
        }
    }
}

void QNVimPlugin::editorOpened(Core::IEditor *editor) {
    mInitialized[filename(editor)] = false;
    if (not mEnabled)
        return;

    if (not editor)
        return;

    QWidget *widget = editor->widget();
    if (not widget)
        return;

    if (not qobject_cast<TextEditor::TextEditorWidget *>(widget))
        return;
    TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());

    if (mEditors.contains(filename(editor))) {
        mInitialized[filename(editor)] = true;
        mNVim->api2()->nvim_command(mNVim->encode(QString("buffer %1").arg(mBuffers[filename(editor)])));
    }
    else {
        mEditors[filename(editor)] = editor;
        if (mNVim and mNVim->isReady()) {
            connect(mNVim->api2()->nvim_eval(mNVim->encode(QString("[execute('e %1'), bufnr('$')]").arg(filename(editor)))),
                    &NeovimQt::MsgpackRequest::finished, [=](quint32, quint64, const QVariant &v) {
                mBuffers[filename(editor)] = v.toList()[1].toULongLong();
            });
        }
    }

    mNumbersColumn->setEditor(textEditor);

    widget->setAttribute(Qt::WA_KeyCompression, false);
    widget->installEventFilter(this);
    Core::IDocument *document = editor->document();

    // connect(document, &Core::IDocument::changed, this, [=]() {syncModifiedToVim(editor);},
    //         Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));
    connect(document, &Core::IDocument::contentsChanged, this, [=]() {
        if (not mInitialized[filename(editor)])
            return;
        if (Core::EditorManager::currentEditor() != editor)
            return;
        QString newText = textEditor->toPlainText();
        if (newText == mText)
            return;
        syncToVim(editor, true);
    }, Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));
    connect(textEditor, &TextEditor::TextEditorWidget::cursorPositionChanged, this, [=]() {
        if (Core::EditorManager::currentEditor() != editor)
            return;
        QString newText = textEditor->toPlainText();
        if (newText != mText)
            return;
        syncCursorToVim(editor, true);
    }, Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));
    connect(textEditor, &TextEditor::TextEditorWidget::selectionChanged, this, [=]() {
        if (Core::EditorManager::currentEditor() != editor)
            return;
        QString newText = textEditor->toPlainText();
        if (newText != mText)
            return;
        syncSelectionToVim(editor);
    }, Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));

    fixSize(editor);
}

void QNVimPlugin::editorAboutToClose(Core::IEditor *editor) {
    if (not mEditors.contains(filename(editor)))
        return;
    mNumbersColumn->setEditor(NULL);
    mEditors.remove(filename(editor));
    mNVim->api2()->nvim_command(mNVim->encode(QString("buffer %1|bw!").arg(mBuffers[filename(editor)])));
    mBuffers.remove(filename(editor));
    mInitialized.remove(filename(editor));
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
            unsigned long long buffer = methodArgs[1].toByteArray().toULongLong();
            QString filename = mNVim->decode(methodArgs[2].toByteArray());
            if (cmd == "BufWriteCmd") {
                if (mBuffers.values().contains(buffer)) {
                    QString currentFilename = mBuffers.key(buffer);
                    if (mEditors[currentFilename]->document()->save(NULL, filename)) {
                        if (currentFilename != filename) {
                            mEditors.remove(currentFilename);
                            mBuffers.remove(currentFilename);
                            mInitialized.remove(currentFilename);
                            connect(mNVim->api2()->nvim_buf_set_name(buffer, mNVim->encode(filename)),
                                    &NeovimQt::MsgpackRequest::finished, [=](quint32, quint64, const QVariant &v) {
                                mNVim->api2()->nvim_command("edit!");
                            });
                        }
                    }
                    else
                        mNVim->api2()->nvim_buf_set_option(mBuffers[currentFilename], "modified", true);
                }
            }
            else if (cmd == "BufReadCmd") {
                if (mEditors.contains(filename)) {
                    qWarning() << buffer << filename;
                    connect(mNVim->api2()->nvim_buf_set_option(buffer, "undolevels", -1),
                            &NeovimQt::MsgpackRequest::finished, this, [=]() {
                        syncToVim(mEditors[filename], true, [=]() {
                            mNVim->api2()->nvim_buf_set_option(buffer, "undolevels", -123456);
                            mNVim->api2()->nvim_buf_set_option(buffer, "modified", false);
                            mInitialized[filename] = true;
                        });
                    }, Qt::DirectConnection);
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
    for(auto arg: args) {
        QVariantList line = arg.toList();
        QByteArray command = line.first().toByteArray();
        line = line.mid(1);
        if (command == "cursor_goto") {
        }
        else if (command == "put") {
        }
        else if (command == "clear") {
        }
        else if (command == "eol_clear") {
        }
        else if (command == "bell") {
            QApplication::beep();
        }
        else if (command == "highlight_set") {
            // TODO
        }
        else if (command == "mode_change") {
            mUIMode = line.first().toList().first().toByteArray();
        }
        else if (command == "set_scroll_region") {
            mScrollRegion.setTop(line.first().toList()[0].toString().toInt());
            mScrollRegion.setBottom(line.first().toList()[1].toString().toInt());
            mScrollRegion.setLeft(line.first().toList()[2].toString().toInt());
            mScrollRegion.setRight(line.first().toList()[3].toString().toInt());
        }
        else if (command == "scroll") {
//            const int scroll = -line.first().toList()[0].toString().toInt();
//            for(signed i = scroll < 0 ? mScrollRegion.top() : mScrollRegion.bottom(); scroll < 0 ? i <= mScrollRegion.bottom() : i >= mScrollRegion.top(); scroll < 0 ? i++ : i--) {
//                QString toScroll = mContent[i].mid(mScrollRegion.left(), mScrollRegion.left() + mScrollRegion.right());
//                if ((scroll < 0 and i + scroll >= mScrollRegion.top()) or (scroll > 0 and i + scroll <= mScrollRegion.bottom()))
//                    mContent[i+scroll] = mContent[i+scroll].left(mScrollRegion.left()) + toScroll + mContent[i+scroll].mid(mScrollRegion.left() + mScrollRegion.right());
//                mContent[i] = mContent[i].left(mScrollRegion.left()) + QString(toScroll.length(), ' ') + mContent[i].mid(mScrollRegion.left() + mScrollRegion.right());
//            }
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
        else if (command == "resize") {
            mWidth = line.first().toList()[0].toULongLong();
            mHeight = line.first().toList()[1].toULongLong();
        }
        else if (command == "update_fg") {
            qint64 val = line.first().toList()[0].toLongLong();
            if (val != -1) {
                mForegroundColor = QRgb(val);
                QPalette palette = textEditor->palette();
                palette.setColor(QPalette::Foreground, mForegroundColor);
                textEditor->setPalette(palette);
            }
        }
        else if (command == "update_bg") {
            qint64 val = line.first().toList()[0].toLongLong();
            if (val != -1) {
                mBackgroundColor = QRgb(val);
                QPalette palette = textEditor->palette();
                palette.setBrush(QPalette::Background, mBackgroundColor);
                textEditor->setPalette(palette);
            }
        }
        else if (command == "update_sp") {
            qint64 val = line.first().toList()[0].toLongLong();
            if (val != -1) {
                mSpecialColor = QRgb(val);
            }
        }
        else if (command == "cmdline_show") {
            mCMDLineVisible = true;
            QVariantList contentList = line.first().toList()[0].toList();
            mCMDLineContent = "";
            for (auto contentItem: contentList)
                mCMDLineContent += mNVim->decode(contentItem.toList()[1].toByteArray());
            mCMDLinePos = line.first().toList()[1].toULongLong();
            mCMDLineFirstc = line.first().toList()[2].toString()[0];
            mCMDLinePrompt = mNVim->decode(line.first().toList()[3].toByteArray());
            mCMDLineIndent = line.first().toList()[4].toULongLong();
        }
        else if (command == "cmdline_pos") {
            mCMDLinePos = line.first().toList()[0].toULongLong();
        }
        else if (command == "cmdline_hide") {
            mCMDLineVisible = false;
        }
        else {
        }
    }

    if (mBusy)
        textEditor->setCursorWidth(0);
    else if (mUIMode == "insert" or mUIMode == "visual")
        textEditor->setCursorWidth(1);
    else if (mUIMode == "normal" or mUIMode == "operator")
        textEditor->setCursorWidth(11);

    if (mCMDLineVisible) {
        QFontMetrics fm(mCMDLine->font());
        mCMDLine->setPlainText(mCMDLineFirstc + mCMDLinePrompt + QString(mCMDLineIndent, ' ') + mCMDLineContent);
        mCMDLine->setMinimumWidth(qMax(200, qMin(qCeil(fm.width(mCMDLine->toPlainText())) + 10, 400)));
        mCMDLine->setFocus();
        QTextCursor cursor = mCMDLine->textCursor();
        cursor.setPosition(QString(mCMDLineFirstc + mCMDLinePrompt).length() + mCMDLineIndent + mCMDLinePos);
        mCMDLine->setTextCursor(cursor);
        if (mUIMode == "cmdline_normal")
            mCMDLine->setCursorWidth(1);
        else if (mUIMode == "cmdline_insert")
            mCMDLine->setCursorWidth(11);
    }
    else {
        mCMDLine->setPlainText(mCMDLineDisplay);
        textEditor->setFocus();
    }
}

} // namespace Internal
} // namespace QNVim
