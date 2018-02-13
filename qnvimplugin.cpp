#include "qnvimplugin.h"
#include "qnvimconstants.h"

#include <coreplugin/icontext.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/coreconstants.h>
#include <texteditor/texteditor.h>
#include <utils/osspecificaspects.h>
#include <neovimconnector.h>
#include <msgpackrequest.h>

#include <QTextEdit>
#include <QGuiApplication>
#include <QPlainTextEdit>
#include <QAction>
#include <QMessageBox>
#include <QMainWindow>
#include <QMenu>

namespace QNVim {
namespace Internal {

QNVimPlugin::QNVimPlugin(): mNVim(NULL), mHeight(35), mBusy(false), mMouse(false)
{
    for (unsigned i = 0; i < mHeight; i++)
        mContent << QString("");
}

QNVimPlugin::~QNVimPlugin()
{
    if (mNVim)
        mNVim->deleteLater();
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

    mNVim = NeovimQt::NeovimConnector::spawn(QStringList(), "/usr/local/bin/nvim");
    connect(mNVim, &NeovimQt::NeovimConnector::ready, [=]() {
        connect(mNVim->api2(), &NeovimQt::NeovimApi2::neovimNotification,
                this, &QNVimPlugin::handleNotification);

        QVariantMap options;
        options.insert("ext_popupmenu", false);
        options.insert("ext_tabline", false);
        options.insert("ext_cmdline", false);
        options.insert("ext_wildmenu", false);
        options.insert("rgb", true);
        NeovimQt::MsgpackRequest *req = mNVim->api2()->nvim_ui_attach(115, mHeight, options);
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
    if (event->type() == QEvent::Shortcut) {
        return true;
    }
    if (event->type() == QEvent::Resize) {
        TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(object);
        unsigned height = textEditor->rowCount() - 1;
        unsigned width = textEditor->columnCount();
        mNVim->api2()->nvim_ui_try_resize(width, height);
    }
    else if (event->type() == QEvent::KeyPress) {
        QKeyEvent *kev = static_cast<QKeyEvent *>(event);

        const int key = kev->key();

        if (key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Control
                || key == Qt::Key_AltGr || key == Qt::Key_Meta)
        {
            return true;
        }

        Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
        QString text = kev->text();
//        const Qt::KeyboardModifier controlKey = Utils::OsSpecificAspects::;
        qWarning() << text << kev->key();
        if (text.isEmpty() && text != " ")
            text = QChar(kev->key());
        assert(text.length() == 1);
        qWarning() << text;
        if (text == "\x7f")
            text = "\x08";
        text = "char-" + QString::number(text.at(0).unicode());

        if (modifiers & Qt::ShiftModifier)
            text = "s-" + text;
        if (modifiers & Qt::MetaModifier)
            text = "c-" + text;
        if (modifiers & Qt::AltModifier)
            text = "a-" + text;

        text = '<' + text + '>';
        qWarning() << text;
        mNVim->api2()->nvim_input(text.toUtf8());
        return true;
    }
    return false;
}

void QNVimPlugin::toggleQNVim() {
    mEnabled = not mEnabled;
    if (mEnabled)
        this->initialize();
    else {
        connect(mNVim->api2()->nvim_command(":q!"), &NeovimQt::MsgpackRequest::finished, [=] () {mNVim->deleteLater(); mNVim = NULL;});
        foreach (QString key, mEditors.keys()) {
            Core::IEditor *editor = mEditors[key];
            if (!editor)
                return;

            QWidget *widget = editor->widget();
            if (!widget)
                return;

            if (!qobject_cast<QTextEdit *>(widget) && !qobject_cast<QPlainTextEdit *>(widget))
                return;

            TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(widget);
            textEditor->setCursorWidth(1);
            widget->removeEventFilter(this);
            mEditors.remove(key);
        }
    }
}

void QNVimPlugin::editorOpened(Core::IEditor *editor)
{
    if (!editor)
        return;

    QWidget *widget = editor->widget();
    if (!widget)
        return;

    if (!qobject_cast<QTextEdit *>(widget) && !qobject_cast<QPlainTextEdit *>(widget))
        return;

    widget->installEventFilter(this);
    mEditors[editor->document()->filePath().toString()] = editor;
    if (mNVim && mNVim->isReady())
        mNVim->api2()->nvim_command(QByteArray() + "e " + editor->document()->filePath().toString().toUtf8());
}

void QNVimPlugin::editorAboutToClose(Core::IEditor *editor)
{
    mNVim->api2()->nvim_command(QString("bw bufnr('%1").arg(editor->document()->filePath().toString()).toUtf8());
    mEditors.remove(editor->document()->filePath().toString());
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
    TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(
                Core::EditorManager::currentEditor()->widget());
    foreach (QVariant arg, args) {
        QVariantList line = arg.toList();
        QByteArray command = line.first().toByteArray();
        line = line.mid(1);
        if (command == "cursor_goto") {
            mCursor.setY(line.first().toList()[0].toString().toInt() + 1);
            mCursor.setX(line.first().toList()[1].toString().toInt());
            textEditor->gotoLine(mCursor.y(), mCursor.x());
//            qWarning() << mCursor;
        }
        else if (command == "put") {
            if (mCursor.y() > mContent.size())
                continue;
            QString l = mContent[mCursor.y() - 1];
            QString s;
            foreach(QVariant j, line)
                foreach(QVariant k, j.toList())
                    s += k.toString();
//            qWarning() << s << line;
            l = l.mid(0, mCursor.x()) + QString(mCursor.x() - l.size(), ' ') + s + l.mid(mCursor.x() + s.length());
            mCursor.setX(mCursor.x() + s.length());
            mContent[mCursor.y() - 1] = l;
        }
        else if (command == "clear") {
            for (unsigned i = 0; i < (unsigned)mContent.size(); i++)
                mContent[i] = "";
        }
        else if (command == "eol_clear") {
            mContent[mCursor.y() - 1] = mContent[mCursor.y() - 1].mid(0, mCursor.x());
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
            const int scroll = -line.first().toList()[0].toString().toInt();
//            qWarning() << mScrollRegion << scroll;
            for (signed i = scroll < 0 ? mScrollRegion.top() : mScrollRegion.bottom(); scroll < 0 ? i <= mScrollRegion.bottom() : i >= mScrollRegion.top(); scroll < 0 ? i++ : i--) {
                QString toScroll = mContent[i].mid(mScrollRegion.left(), mScrollRegion.left() + mScrollRegion.right());
                if ((scroll < 0 and i + scroll >= mScrollRegion.top()) or (scroll > 0 and i + scroll <= mScrollRegion.bottom()))
                    mContent[i+scroll] = mContent[i+scroll].left(mScrollRegion.left()) + toScroll + mContent[i+scroll].mid(mScrollRegion.left() + mScrollRegion.right());
                mContent[i] = mContent[i].left(mScrollRegion.left()) + QString(toScroll.length(), ' ') + mContent[i].mid(mScrollRegion.left() + mScrollRegion.right());
            }
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
            mHeight = line.first().toList()[1].toString().toInt();
            if ((unsigned)mContent.size() < mHeight)
                for (unsigned i = (unsigned)mContent.size(); i < mHeight; i++)
                    mContent << QString();
            while ((unsigned)mContent.size() > mHeight)
                mContent.removeLast();
        }
        else {
            qWarning() << command << line;
        }
    }

    textEditor->setPlainText(mContent.join('\n'));
    textEditor->gotoLine(mCursor.y(), mCursor.x());
    if (mBusy)
        textEditor->setCursorWidth(0);
    if (mMode == "insert")
        textEditor->setCursorWidth(1);
    else if (mMode == "normal")
        textEditor->setCursorWidth(11);
}

} // namespace Internal
} // namespace QNVim
