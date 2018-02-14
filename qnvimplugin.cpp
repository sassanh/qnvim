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
#include <texteditor/texteditor.h>
#include <texteditor/textdocument.h>
#include <utils/osspecificaspects.h>
#include <gui/input.h>
#include <neovimconnector.h>
#include <msgpackrequest.h>

#include <QTextEdit>
#include <QApplication>
#include <QGuiApplication>
#include <QPlainTextEdit>
#include <QAction>
#include <QMessageBox>
#include <QMainWindow>
#include <QMenu>
#include <QPainter>

namespace QNVim {
namespace Internal {

QNVimPlugin::QNVimPlugin(): mEnabled(true), mNVim(NULL), mInputConv(new NeovimQt::InputConv),
    mVimChanges(0), mWidth(80), mHeight(35),
    mForegroundColor(Qt::black), mBackgroundColor(Qt::white), mSpecialColor(QColor()),
    mCursorColor(Qt::white),
    mBusy(false), mMouse(false)
{
}

QNVimPlugin::~QNVimPlugin() {
    if (mNVim)
        mNVim->deleteLater();
}

QString QNVimPlugin::filename(Core::IEditor *editor) const {
    if (not editor)
        editor = Core::EditorManager::currentEditor();
    if (editor)
        return editor->document()->filePath().toString();
    return "";
}

void QNVimPlugin::fixSize(Core::IEditor *editor) {
    if (not editor)
        editor = Core::EditorManager::currentEditor();
//    unsigned desiredWidth = editor->widget()->width() / shellWidget(editor)->cellSize().width();
//    unsigned desiredHeight = editor->widget()->height() / shellWidget(editor)->cellSize().height();
//    unsigned width = shellWidget(editor)->columns();
//    unsigned height = shellWidget(editor)->rows();
//    if (width != desiredWidth or height != desiredHeight) {
//        shellWidget(editor)->resize(editor->widget()->width(), editor->widget()->height());
//        shellWidget(editor)->resizeShell(desiredHeight, desiredWidth);
//    }
}

void QNVimPlugin::syncCursorToVim() {
    Core::IEditor *editor = Core::EditorManager::currentEditor();
    TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    QString text = textEditor->toPlainText();
    unsigned cursorPosition = textEditor->textCursor().position();
    unsigned line = text.left(cursorPosition).count("\n") + 1;
    unsigned col = text.left(cursorPosition).section("\n", -1).toUtf8().length() + 1;
    mCursor.setY(line);
    mCursor.setX(col);
    mNVim->api2()->nvim_command(QString("call setpos('.', [%1,%2,%3])").arg(mBuffers[filename()]).
            arg(line).arg(col).toUtf8());
}

void QNVimPlugin::syncToVim(bool force) {
    if (not mSyncMutex.tryLock()) {
        if (force)
            QTimer::singleShot(0, [=]() {syncToVim(true);});
        return;
    }
    Core::IEditor *editor = Core::EditorManager::currentEditor();
    TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    QString text = textEditor->toPlainText();
    QList<QByteArray> textLines = text.toUtf8().split('\n');
    mNVim->api2()->nvim_command(QString("buffer %1").arg(mBuffers[filename()]).toUtf8());
    mNVim->api2()->nvim_buf_set_lines(mBuffers[filename()], 0, textLines.size(), false, textLines);
    syncCursorToVim();
    mSyncMutex.unlock();
}

void QNVimPlugin::syncFromVim(bool force) {
    if (not mSyncMutex.tryLock()) {
        if (force)
            QTimer::singleShot(0, [=]() {syncFromVim(true);});
        return;
    }
    qWarning() << "SYNCING FROM VIM";
    Core::IEditor *editor = Core::EditorManager::currentEditor();
    TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    mNVim->api2()->nvim_command(QString("buffer %1").arg(mBuffers[filename()]).toUtf8());
    connect(mNVim->api2()->nvim_eval("[winsaveview(), &modified, line2byte('.')]"), &NeovimQt::MsgpackRequest::finished,
            [=](quint32, quint64, const QVariant &v) {
        QVariantMap wview = v.toList()[0].toMap();
        bool modified = v.toList()[1].toBool();
        connect(mNVim->api2()->nvim_buf_get_lines(mBuffers[filename()], 0, -1, true),
                &NeovimQt::MsgpackRequest::finished, [=](quint32, quint64, const QVariant &lines) {
            mText = "";
            for (auto t: lines.toList())
                mText += QString::fromUtf8(t.toByteArray()) + '\n';
            /* for (auto t: lines.toList()) */
            /*     mText += t.toByteArray() + '\n'; */
            mText.chop(1);
//            textEditor->document()->findBlockByLineNumber
            QString oldText = textEditor->toPlainText();
            /* textEditor->textDocument()->setContents(mText); */
            diff_match_patch differ;
            QList<Diff> diffs = differ.diff_main(oldText, mText);
            differ.diff_cleanupEfficiency(diffs);
            QList<Patch> patches = differ.patch_make(oldText, diffs);

            QTextCursor cursor = textEditor->textCursor();
            for (auto patch: patches) {
                cursor.setPosition(patch.start1);
                cursor.setPosition(patch.start1 + patch.length1, QTextCursor::KeepAnchor);
                cursor.insertText(mText.mid(patch.start2, patch.length2));
            }

            textEditor->document()->setModified(modified);
            unsigned long long cursorPosition = v.toList()[2].toULongLong();
            unsigned line = wview["lnum"].toULongLong();
            unsigned col = wview["col"].toULongLong();
            cursorPosition = QString::fromUtf8(mText.toUtf8().left(cursorPosition + col - 1)).length();
            mCursor.setY(line);
            mCursor.setX(col);
            textEditor->setCursorPosition(cursorPosition);
            mSyncMutex.unlock();
        });
    });
}

bool QNVimPlugin::initialize(const QStringList &arguments, QString *errorString)
{
    Q_UNUSED(arguments)
    Q_UNUSED(errorString)
    return initialize();
}

bool QNVimPlugin::initialize()
{
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
    connect(Core::EditorManager::instance(), &Core::EditorManager::editorOpened,
            this, &QNVimPlugin::editorOpened);
    connect(Core::EditorManager::instance(), &Core::EditorManager::currentEditorChanged,
            this, &QNVimPlugin::editorOpened);

    mNVim = NeovimQt::NeovimConnector::spawn(QStringList() << "--cmd" << "autocmd VimEnter * set nowrap|set nonumber|set norelativenumber|set signcolumn=no",
                                             "/usr/local/bin/nvim");
    connect(mNVim, &NeovimQt::NeovimConnector::ready, [=]() {
        connect(mNVim->api2(), &NeovimQt::NeovimApi2::neovimNotification,
                this, &QNVimPlugin::handleNotification);

        QVariantMap options;
        options.insert("ext_popupmenu", false);
        options.insert("ext_tabline", false);
        options.insert("ext_cmdline", false);
        options.insert("ext_wildmenu", false);
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

void QNVimPlugin::extensionsInitialized()
{
    // Retrieve objects from the plugin manager's object pool
    // In the extensionsInitialized function, a plugin can be sure that all
    // plugins that depend on it are completely initialized.
}

ExtensionSystem::IPlugin::ShutdownFlag QNVimPlugin::aboutToShutdown()
{
    // Save settings
    // Disconnect from signals that are not needed during shutdown
    // Hide UI (if you add UI that is not in the main window directly)
    return SynchronousShutdown;
}

bool QNVimPlugin::eventFilter(QObject *object, QEvent *event) {
    if (not mEnabled)
        return false;
    if (qobject_cast<QTextEdit *>(object) or qobject_cast<QPlainTextEdit *>(object)) {
        if (event->type() == QEvent::Resize) {
            QResizeEvent *resizeEvent = static_cast<QResizeEvent *>(event);
//            unsigned width = resizeEvent->size().width() / shellWidget()->cellSize().width();
//            unsigned height = resizeEvent->size().height() / shellWidget()->cellSize().height();
//            mNVim->api2()->nvim_ui_try_resize(width, height);
            return false;
        }
    }
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
        QString key = mInputConv->convertKey(keyEvent->text(), keyEvent->key(), modifiers);
        qWarning() << key;
        mNVim->api2()->nvim_input(mNVim->encode(key));
        return true;
    }
    else if (event->type() == QEvent::Shortcut) {
        return true;
    }
    return false;
}

void QNVimPlugin::toggleQNVim() {
    mEnabled = not mEnabled;
    if (mEnabled)
        this->initialize();
    else {
        for(auto key: mEditors.keys()) {
            Core::IEditor *editor = mEditors[key];
            if (!editor)
                continue;

            QWidget *widget = editor->widget();
            if (!widget)
                continue;

            if (!qobject_cast<QTextEdit *>(widget) and !qobject_cast<QPlainTextEdit *>(widget))
                continue;

            TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(widget);
            textEditor->setCursorWidth(1);
            widget->removeEventFilter(this);
            mEditors.remove(key);
            mBuffers.remove(key);
        }
    }
}

void QNVimPlugin::editorOpened(Core::IEditor *editor)
{
    if (not mEnabled)
        return;

    if (not editor)
        return;

    QWidget *widget = editor->widget();
    if (not widget)
        return;

    if (not qobject_cast<QTextEdit *>(widget) and not qobject_cast<QPlainTextEdit *>(widget))
        return;

    if (mEditors.contains(filename(editor))) {
    }
    else {
        TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
        widget->installEventFilter(this);

        if (mNVim and mNVim->isReady()) {
            mNVim->api2()->nvim_command("enew");
            connect(mNVim->api2()->nvim_eval("bufnr('$')"), &NeovimQt::MsgpackRequest::finished,
                    [=](quint32, quint64, const QVariant &v) {
                mBuffers[filename(editor)] = v.toULongLong();

                mNVim->api2()->nvim_buf_set_option(mBuffers[filename(editor)], "buftype", "acwrite");
                mNVim->api2()->nvim_buf_set_option(mBuffers[filename(editor)], "undolevels", -1);
                syncToVim();
                mNVim->api2()->nvim_buf_set_option(mBuffers[filename(editor)], "undolevels", -123456);
                mNVim->api2()->nvim_buf_set_option(mBuffers[filename(editor)], "modified", false);
                mNVim->api2()->nvim_buf_set_name(mBuffers[filename(editor)], mNVim->encode(filename(editor)));
            });
        }

        mEditors[filename(editor)] = editor;
        Core::IDocument *document = editor->document();
        connect(textEditor, &TextEditor::TextEditorWidget::cursorPositionChanged, [=]() {
            syncCursorToVim();
        });
        connect(document, &Core::IDocument::changed, [=]() {
            mNVim->api2()->nvim_buf_set_option(mBuffers[filename(editor)], "modified", document->isModified());
        });
        connect(document, &Core::IDocument::contentsChanged, [=]() {
            QString newText = textEditor->toPlainText();
            if (newText == mText)
                return;

            syncToVim();
            // diff_match_patch differ;
            // QList<Diff> diffs = differ.diff_main(mText, newText);
            // differ.diff_cleanupEfficiency(diffs);
            // QList<Patch> patches = differ.patch_make(mText, diffs);
            //
            // QString command("let b:_pos=getpos('.')|");
            // for (auto patch: patches) {
            //     unsigned line = mText.left(patch.start1).count("\n") + 1;
            //     unsigned col = mText.left(patch.start1).section("\n", -1).toUtf8().length();
            //     unsigned sourceLength = mText.mid(patch.start1, patch.length1).toUtf8().length();
            //     command += QString("call setpos('.',[%1,%2,%3])|call execute('normal! c%4l%5')|")
            //         .arg(mBuffers[filename(editor)]).arg(line).arg(col)
            //         .arg(sourceLength).arg(mText.mid(patch.start2, patch.length2));
            // }
            // command += "call setposs('.', b:_pos)|unlet b:_pos";
            // qWarning() << command;
            //
            // mNVim->api2()->nvim_command(command.toUtf8());
        });

        fixSize(editor);
    }
}

void QNVimPlugin::editorAboutToClose(Core::IEditor *editor)
{
    if (not mEditors.contains(filename(editor)))
        return;
    mNVim->api2()->nvim_command(QString("bw %1").arg(mBuffers[filename(editor)]).toUtf8());
    mEditors.remove(filename(editor));
    mBuffers.remove(filename(editor));
}

void QNVimPlugin::handleNotification(const QByteArray &name, const QVariantList &args)
{
//    NeovimQt::MsgpackRequest *req = mNVim->api2()->nvim_eval("1+2");
//    connect(req, &NeovimQt::MsgpackRequest::finished, [=](quint32 id, quint64 fun, const QVariant &v) {
//        qWarning() << "HEY" << id << fun << v;
//    });
    if (name == "redraw") {
        redraw(args);
    }
    else
        qWarning() << name << args;
}

void QNVimPlugin::redraw(const QVariantList &args) {
    if (!Core::EditorManager::currentEditor())
        return;
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
            mMode = line.first().toList().first().toByteArray();
        }
        else if (command == "set_scroll_region") {
            mScrollRegion.setTop(line.first().toList()[0].toString().toInt());
            mScrollRegion.setBottom(line.first().toList()[1].toString().toInt());
            mScrollRegion.setLeft(line.first().toList()[2].toString().toInt());
            mScrollRegion.setRight(line.first().toList()[3].toString().toInt());
        }
        else if (command == "scroll") {
//            const int scroll = -line.first().toList()[0].toString().toInt();
//            qWarning() << mScrollRegion << scroll;
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
        else {
            /* qWarning() << command << line; */
        }
    }

    syncFromVim();
    if (mBusy)
        textEditor->setCursorWidth(0);
    else if (mMode == "insert")
        textEditor->setCursorWidth(1);
    else if (mMode == "normal")
        textEditor->setCursorWidth(11);
}

} // namespace Internal
} // namespace QNVim
