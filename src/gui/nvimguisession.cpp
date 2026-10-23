#include "nvimguisession.h"

#include "nvimguiargs.h"
#include "nvimguiconstants.h"
#include "nvimguieditor.h"

#include "../log.h"
#include "../qnvimsettings.h"

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/icore.h>
#include <coreplugin/messagemanager.h>

#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>

#include <texteditor/fontsettings.h>
#include <texteditor/texteditorsettings.h>

#include <utils/mimeutils.h>
#include <utils/qtcsettings.h>

#include <gui/shell.h>
#include <msgpackrequest.h>
#include <neovimconnector.h>

#include <QApplication>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QKeyEvent>
#include <QMessageBox>
#include <QProcess>
#include <QTimer>

namespace QNVim {
namespace Gui {

using Internal::settings;

namespace {

NvimGuiSession *s_instance = nullptr;

QString tr(const char *text) {
    return QCoreApplication::translate("QNVim", text);
}

const Utils::Key kPreferredEditors("EditorManager/PreferredEditorFactories");
const Utils::Key kTakenOver("QNVim/Gui/TakenOverMimeTypes");

QStringList splitCommaList(const QString &value) {
    QStringList result;
    for (const QString &part : value.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty())
            result << trimmed;
    }
    return result;
}

// Copies a resource to the cache directory, so Neovim can read it.
QString extractResource(const QString &resource, const QString &name) {
    const Utils::FilePath dir = Core::ICore::cacheResourcePath(QStringLiteral("qnvim-gui"));
    dir.createDir();
    const Utils::FilePath target = dir.pathAppended(name);
    QFile in(resource);
    if (!in.open(QIODevice::ReadOnly)) {
        qWarning(Main) << "qnvim gui: missing resource" << resource;
        return {};
    }
    const QByteArray data = in.readAll();
    if (target.fileContents().value_or(QByteArray()) != data)
        target.writeFileContents(data);
    return target.toUserOutput();
}

} // namespace

NvimGuiSession::NvimGuiSession() {
    s_instance = this;
    m_factory = std::make_unique<NvimEditorFactory>(this);

    for (const QString &k : splitCommaList(settings().passthroughKeys())) {
        const QKeySequence seq = QKeySequence::fromString(k, QKeySequence::PortableText);
        if (!seq.isEmpty())
            m_passthrough << seq;
    }

    connect(Core::EditorManager::instance(), &Core::EditorManager::currentEditorChanged,
            this, &NvimGuiSession::onCurrentEditorChanged);
    connect(ProjectExplorer::ProjectManager::instance(),
            &ProjectExplorer::ProjectManager::startupProjectChanged, this,
            &NvimGuiSession::syncWorkingDirectory);

    m_parking = new QWidget;
    m_parking->hide();

    start();
}

NvimGuiSession::~NvimGuiSession() {
    shutdown();
    delete m_parking;
    if (s_instance == this)
        s_instance = nullptr;
}

NvimGuiSession *NvimGuiSession::instance() {
    return s_instance;
}

// Making the editor the default ---------------------------------------------
//
// Qt Creator picks the editor for a file from the factories registered for its
// MIME type, in registration order, unless the user chose a default editor
// for that type (Environment > MIME Types, or "Open With" > default). Those
// choices live in "EditorManager/PreferredEditorFactories" and are read in
// Core's extensionsInitialized(), which runs after ours. Writing our id there
// for every text type is the public way to become the default editor.

void NvimGuiSession::takeOverTextEditors() {
    Utils::QtcSettings *s = Core::ICore::settings();
    QVariantMap preferred = s->value(kPreferredEditors).toMap();

    // Undo the last run first, so removed exclusions take effect.
    const QStringList previous = s->value(kTakenOver).toStringList();
    for (const QString &mime : previous) {
        if (preferred.value(mime).toString() == QLatin1String(Constants::EDITOR_ID))
            preferred.remove(mime);
    }

    QStringList taken;
    if (settings().takeOverTextFiles()) {
        const QStringList excluded = splitCommaList(settings().excludedMimeTypes());
        const auto isExcluded = [&excluded](const Utils::MimeType &mt) {
            for (const QString &ex : excluded) {
                if (mt.matchesName(ex) || mt.inherits(ex))
                    return true;
            }
            return false;
        };
        for (const Utils::MimeType &mt : Utils::allMimeTypes()) {
            if (!mt.inherits(QLatin1String("text/plain")) && mt.name() != QLatin1String("text/plain"))
                continue;
            if (isExcluded(mt))
                continue;
            // Respect a default the user picked for this type himself.
            const QString current = preferred.value(mt.name()).toString();
            if (!current.isEmpty() && current != QLatin1String(Constants::EDITOR_ID))
                continue;
            preferred.insert(mt.name(), QString::fromLatin1(Constants::EDITOR_ID));
            taken << mt.name();
        }
    }
    s->setValue(kPreferredEditors, preferred);
    s->setValue(kTakenOver, taken);
    qDebug(Main) << "qnvim gui: default editor for" << taken.size() << "MIME types";
}

void NvimGuiSession::releaseTextEditors() {
    Utils::QtcSettings *s = Core::ICore::settings();
    const QStringList previous = s->value(kTakenOver).toStringList();
    if (previous.isEmpty())
        return;
    QVariantMap preferred = s->value(kPreferredEditors).toMap();
    for (const QString &mime : previous) {
        if (preferred.value(mime).toString() == QLatin1String(Constants::EDITOR_ID))
            preferred.remove(mime);
    }
    s->setValue(kPreferredEditors, preferred);
    s->remove(kTakenOver);
}

// Neovim process ---------------------------------------------------------------

void NvimGuiSession::start() {
    m_ready = false;
    m_startTimer.start();

    const QString luaFile = extractResource(QStringLiteral(":/qnvim/gui/qnvim_gui.lua"),
                                            QStringLiteral("qnvim_gui.lua"));
    const QString shimFile = extractResource(QStringLiteral(":/qnvim/gui/nvim_gui_shim.vim"),
                                             QStringLiteral("runtime/plugin/nvim_gui_shim.vim"));

    const QStringList args = nvimGuiArguments(
        luaFile, shimFile, QString::fromLocal8Bit(qgetenv("QNVIM_LISTEN")),
        QProcess::splitCommand(settings().extraArguments()));

    QString exe = settings().nvimExecutable().trimmed();
    if (exe.isEmpty())
        exe = QStringLiteral("nvim");

    // Do not let the child attach to a Neovim Qt Creator was started from
    // (e.g. from a :terminal).
    const QByteArray oldNvim = qgetenv("NVIM");
    const QByteArray oldListen = qgetenv("NVIM_LISTEN_ADDRESS");
    qunsetenv("NVIM");
    qunsetenv("NVIM_LISTEN_ADDRESS");
    m_nvim = NeovimQt::NeovimConnector::spawn(args, exe);
    if (!oldNvim.isEmpty())
        qputenv("NVIM", oldNvim);
    if (!oldListen.isEmpty())
        qputenv("NVIM_LISTEN_ADDRESS", oldListen);

    qDebug(Main) << "qnvim gui: spawned" << exe << args;

    connect(m_nvim, &NeovimQt::NeovimConnector::ready, this, &NvimGuiSession::onNeovimReady);
    connect(m_nvim, &NeovimQt::NeovimConnector::processExited, this,
            &NvimGuiSession::onNeovimExited);
    connect(m_nvim, &NeovimQt::NeovimConnector::error, this, [this, exe] {
        if (!m_nvim)
            return;
        const QString msg = tr("qnvim: Neovim (%1) is not available: %2")
                                .arg(exe, m_nvim->errorString());
        qCritical(Main) << msg;
        Core::MessageManager::writeFlashing(msg);
    });

    m_shell = new NeovimQt::Shell(m_nvim, m_parking);
    m_shell->installEventFilter(this);
    applyFont();
    connect(m_shell, &NeovimQt::Shell::neovimAttachmentChanged, this, [this](bool attached) {
        if (!attached)
            return;
        m_ready = true;
        sendActions();
        syncWorkingDirectory();
        const auto pending = std::exchange(m_pending, {});
        for (const auto &call : pending)
            call();
    });
    // :GuiClose / VimLeave: the process exit is handled in onNeovimExited().
    connect(m_shell, &NeovimQt::Shell::neovimGuiCloseRequest, this, [] {});

    // An editor may already be current (session restore before we started).
    if (auto editor = qobject_cast<NvimEditor *>(Core::EditorManager::currentEditor()))
        attachShellTo(editor->host());
}

void NvimGuiSession::onNeovimReady() {
    if (!m_nvim || !m_nvim->api2())
        return;
    // Before the UI attaches Neovim has not run its startup yet; the variable is
    // there in time for VimEnter.
    m_nvim->api2()->nvim_set_var(QByteArrayLiteral("qnvim_gui_channel"),
                                 QVariant::fromValue<quint64>(m_nvim->channel()));
    connect(m_nvim->api0(), &NeovimQt::NeovimApi0::neovimNotification, this,
            &NvimGuiSession::onNotification);
}

void NvimGuiSession::onNeovimExited() {
    m_ready = false;
    m_pending.clear();
    if (m_shuttingDown)
        return;

    qDebug(Main) << "qnvim gui: Neovim exited";
    // Neovim already decided about unsaved changes (:q refuses, :q! discards).
    QList<Core::IDocument *> docs;
    for (NvimDocument *doc : std::as_const(m_documents)) {
        doc->closingFromNvim = true;
        docs << doc;
    }
    if (!docs.isEmpty())
        Core::EditorManager::closeDocuments(docs, false);

    if (m_currentHost)
        m_currentHost->release();
    m_currentHost = nullptr;
    delete m_shell;
    if (m_nvim)
        m_nvim->deleteLater();
    m_nvim = nullptr;
    m_nvimCurrentFile.clear();

    // Start a fresh one for the next file, unless it died right after starting
    // (a broken config would make that a restart loop).
    const bool requested = std::exchange(m_restartRequested, false);
    if (!requested && m_startTimer.elapsed() < 3000) {
        Core::MessageManager::writeDisrupting(
            tr("qnvim: Neovim exited right after starting. Check your Neovim configuration "
               "(start it once in a terminal), then use Tools > QNVim > Restart Neovim."));
        return;
    }
    QTimer::singleShot(0, this, &NvimGuiSession::start);
}

void NvimGuiSession::restart() {
    bool modified = false;
    for (NvimDocument *doc : std::as_const(m_documents))
        modified = modified || doc->isModified();
    if (modified
        && QMessageBox::question(Core::ICore::dialogParent(), tr("Restart Neovim"),
                                 tr("Some buffers have unsaved changes. Restart Neovim and "
                                    "discard them?"))
               != QMessageBox::Yes) {
        return;
    }
    m_restartRequested = true;
    if (m_nvim && m_nvim->isReady() && m_nvim->api2())
        m_nvim->api2()->nvim_command(QByteArrayLiteral("qall!"));
    else if (m_nvim)
        onNeovimExited();
    else
        start(); // the previous one died at startup and was not restarted
}

void NvimGuiSession::shutdown() {
    if (m_shuttingDown)
        return;
    m_shuttingDown = true;
    if (m_currentHost)
        m_currentHost->release();
    if (m_nvim && m_nvim->isReady() && m_nvim->api2())
        m_nvim->api2()->nvim_command(QByteArrayLiteral("qall!"));
    delete m_shell;
    // The connector's process ends when Neovim quits or its stdin closes.
}

QWidget *NvimGuiSession::shellWidget() const {
    return m_shell;
}

// Calls into Neovim ------------------------------------------------------------

void NvimGuiSession::luaCall(const QByteArray &code, const QVariantList &args) {
    if (!m_ready || !m_nvim || !m_nvim->api2()) {
        m_pending << [this, code, args] { luaCall(code, args); };
        return;
    }
    QVariant arg;
    if (args.size() == 1)
        arg = args.first();
    else
        arg = args;
    auto req = m_nvim->api2()->nvim_call_function(QByteArrayLiteral("luaeval"),
                                                 {QString::fromUtf8(code), arg});
    connect(req, &NeovimQt::MsgpackRequest::error, this,
            [code](quint32, quint64, const QVariant &err) {
                qWarning(Main) << "qnvim gui: lua call failed:" << code << err;
            });
}

void NvimGuiSession::documentOpened(NvimDocument *document) {
    const Utils::FilePath path = document->filePath();
    m_documents.insert(key(path), document);
    if (path != m_nvimCurrentFile)
        luaCall("QNVimGui.add(_A)", {path.toUserOutput()});
}

void NvimGuiSession::documentClosing(NvimDocument *document) {
    const Utils::FilePath path = document->filePath();
    if (path.isEmpty())
        return;
    if (m_documents.value(key(path)) == document)
        m_documents.remove(key(path));
    if (m_shuttingDown || document->closingFromNvim)
        return;
    if (m_nvimCurrentFile == path)
        m_nvimCurrentFile.clear();
    luaCall("QNVimGui.close(_A)", {path.toUserOutput()});
}

bool NvimGuiSession::writeBuffer(const Utils::FilePath &path, QString *error) {
    if (!m_ready || !m_nvim || !m_nvim->api2()) {
        *error = tr("Neovim is not running.");
        return false;
    }
    // Saving is synchronous for Qt Creator: "save all" before a build must have
    // written the files when it returns.
    auto req = m_nvim->api2()->nvim_call_function(
        QByteArrayLiteral("luaeval"),
        {QStringLiteral("QNVimGui.write(_A)"), path.toUserOutput()});
    QEventLoop loop;
    bool done = false;
    QString result;
    connect(req, &NeovimQt::MsgpackRequest::finished, &loop,
            [&](quint32, quint64, const QVariant &resp) {
                done = true;
                result = resp.toString();
                loop.quit();
            });
    connect(req, &NeovimQt::MsgpackRequest::error, &loop,
            [&](quint32, quint64, const QVariant &err) {
                done = true;
                result = err.toString();
                if (result.isEmpty())
                    result = tr("Neovim could not write the file.");
                loop.quit();
            });
    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    loop.exec(QEventLoop::ExcludeUserInputEvents);
    if (!done) {
        *error = tr("Neovim did not answer in time (is it waiting for input?).");
        return false;
    }
    if (!result.isEmpty()) {
        *error = result;
        return false;
    }
    return true;
}

void NvimGuiSession::checkTime() {
    luaCall("vim.cmd('silent! checktime')", {QVariant()});
}

void NvimGuiSession::gotoPosition(const Utils::FilePath &path, int line, int column, bool center) {
    if (path.isEmpty())
        return;
    m_nvimCurrentFile = path;
    luaCall("QNVimGui.open(_A[1], _A[2], _A[3], _A[4])",
            {path.toUserOutput(), line, column, center});
}

void NvimGuiSession::triggerAction(const QByteArray &id) {
    Core::Command *command = Core::ActionManager::command(Utils::Id::fromName(id));
    if (!command || !command->action()) {
        luaCall("vim.notify(_A, vim.log.levels.WARN)",
                {tr("Qt Creator has no action \"%1\"").arg(QString::fromUtf8(id))});
        return;
    }
    if (!command->action()->isEnabled()) {
        luaCall("vim.notify(_A, vim.log.levels.WARN)",
                {tr("Qt Creator action \"%1\" is not available now").arg(QString::fromUtf8(id))});
        return;
    }
    // Let the notification handler return before the action runs (it may open
    // dialogs or editors).
    QTimer::singleShot(0, command->action(), &QAction::trigger);
}

void NvimGuiSession::sendActions() {
    QVariantList list;
    const QList<Core::Command *> commands = Core::ActionManager::commands();
    for (Core::Command *cmd : commands) {
        const QString id = cmd->id().toString();
        if (id.isEmpty())
            continue;
        QString description = cmd->description();
        description.remove(QLatin1Char('&'));
        list << QVariant(QVariantList{id, description});
    }
    luaCall("QNVimGui.set_actions(_A)", {QVariant(list)});
}

void NvimGuiSession::syncWorkingDirectory() {
    if (!settings().followProjectDirectory())
        return;
    ProjectExplorer::Project *project = ProjectExplorer::ProjectManager::startupProject();
    if (!project)
        return;
    const Utils::FilePath dir = project->projectDirectory();
    if (dir.isEmpty() || !dir.isLocal())
        return;
    luaCall("vim.cmd.cd(vim.fn.fnameescape(_A))", {dir.toUserOutput()});
}

void NvimGuiSession::applyFont() {
    if (!m_shell || !settings().useEditorFont())
        return;
    const TextEditor::FontSettings &fs = TextEditor::TextEditorSettings::fontSettings();
    const QString desc = QStringLiteral("%1:h%2").arg(fs.family()).arg(fs.fontSize());
    m_shell->setGuiFont(desc, true);
}

// Notifications from Neovim ----------------------------------------------------

void NvimGuiSession::onNotification(const QByteArray &name, const QVariantList &args) {
    if (name != Constants::RPC_METHOD || args.isEmpty())
        return;
    const QByteArray event = args.at(0).toByteArray();
    const auto pathArg = [&args](int i) {
        return args.size() > i ? Utils::FilePath::fromUserInput(args.at(i).toString())
                               : Utils::FilePath();
    };

    if (event == "bufenter") {
        onBufEnter(pathArg(1));
    } else if (event == "bufdelete") {
        onBufDelete(pathArg(1));
    } else if (event == "modified") {
        if (NvimDocument *doc = documentFor(pathArg(1)))
            doc->setModifiedFromNvim(args.value(2).toBool());
    } else if (event == "written") {
        if (NvimDocument *doc = documentFor(pathArg(1)))
            doc->setModifiedFromNvim(false);
    } else if (event == "cursor") {
        if (NvimDocument *doc = documentFor(pathArg(1)))
            doc->setCursorFromNvim(args.value(2).toInt(), args.value(3).toInt());
    } else if (event == "action") {
        triggerAction(args.value(1).toByteArray());
    } else if (event == "actions") {
        sendActions();
    }
}

void NvimGuiSession::onBufEnter(const Utils::FilePath &path) {
    if (path.isEmpty())
        return;
    m_nvimCurrentFile = path;
    auto current = qobject_cast<NvimEditor *>(Core::EditorManager::currentEditor());
    if (current && current->document()->filePath() == path)
        return;
    if (path.isDir())
        return; // netrw/oil directory buffers
    // Make Qt Creator's current document follow Neovim's current buffer.
    m_syncingFromNvim = true;
    Core::EditorManager::openEditor(path, Constants::EDITOR_ID,
                                    Core::EditorManager::DoNotSwitchToDesignMode);
    m_syncingFromNvim = false;
    if (m_shell && m_currentHost)
        m_shell->setFocus();
}

void NvimGuiSession::onBufDelete(const Utils::FilePath &path) {
    NvimDocument *doc = documentFor(path);
    if (!doc)
        return;
    doc->closingFromNvim = true;
    if (m_nvimCurrentFile == path)
        m_nvimCurrentFile.clear();
    Core::EditorManager::closeDocuments({doc}, false);
}

// Editor area ------------------------------------------------------------------

void NvimGuiSession::onCurrentEditorChanged(Core::IEditor *editor) {
    auto nvimEditor = qobject_cast<NvimEditor *>(editor);
    if (!nvimEditor)
        return; // another editor type: Neovim stays where it was
    attachShellTo(nvimEditor->host());
    const Utils::FilePath path = nvimEditor->document()->filePath();
    if (!path.isEmpty() && !m_syncingFromNvim && path != m_nvimCurrentFile) {
        m_nvimCurrentFile = path;
        luaCall("QNVimGui.open(_A[1], _A[2], _A[3], _A[4])",
                {path.toUserOutput(), 0, 0, false});
    }
}

void NvimGuiSession::attachShellTo(NvimEditorHost *host) {
    if (!host || !m_shell)
        return;
    if (m_currentHost && m_currentHost != host)
        m_currentHost->release();
    m_currentHost = host;
    host->adopt(m_shell);
    if (host->isVisible())
        m_shell->setFocus();
}

// Keys -------------------------------------------------------------------------

bool NvimGuiSession::isPassthrough(const QKeySequence &seq) const {
    for (const QKeySequence &p : m_passthrough) {
        if (p.matches(seq) == QKeySequence::ExactMatch)
            return true;
    }
    return false;
}

bool NvimGuiSession::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_shell && event->type() == QEvent::ShortcutOverride) {
        auto ke = static_cast<QKeyEvent *>(event);
        const QKeySequence seq(ke->keyCombination());
        if (!isPassthrough(seq)) {
            // Neovim wants this key: keep Qt Creator's shortcuts from taking it.
            ke->accept();
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}

NvimDocument *NvimGuiSession::documentFor(const Utils::FilePath &path) const {
    return m_documents.value(key(path));
}

QString NvimGuiSession::key(const Utils::FilePath &path) {
    // Neovim reports paths as given (fnamemodify ':p'), Qt Creator may resolve
    // symlinks; compare the canonical files.
    const Utils::FilePath abs = path.absoluteFilePath().cleanPath();
    const Utils::FilePath canonical = abs.canonicalPath();
    return (canonical.isEmpty() ? abs : canonical).toUrlishString();
}

} // namespace Gui
} // namespace QNVim
