#include "qnvimplugin.h"

#include "qnvimcore.h"
#include "qnvimsettings.h"
#include "gui/nvimguiconstants.h"
#include "gui/nvimguisession.h"
#include "qnvimsettingspage.h"
#include "log.h"
#include "qnvimconstants.h"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>

#include <texteditor/textdocument.h>
#include <texteditor/texteditorconstants.h>

#include <utils/filepath.h>

#include <QAction>
#include <QMenu>

namespace QNVim {
namespace Internal {

namespace {

// Qt Creator's own plain text editor widget (that one is internal to the
// TextEditor plugin): marks the document as plain text.
class PlainTextEditorWidget final : public TextEditor::TextEditorWidget {
  public:
    void finalizeInitialization() final {
        textDocument()->setMimeType(QLatin1String(TextEditor::Constants::C_TEXTEDITOR_MIMETYPE_TEXT));
    }
};

// TextEditorFactory leaves its document creator unset (only a default editor
// widget creator is set up), and creating an editor with an unset document
// creator calls an empty std::function (std::bad_function_call). Set up the
// same document and widget creators that Qt Creator's own plain text editor
// factory sets up.
void setupPlainTextCreators(TextEditor::TextEditorFactory &factory) {
    factory.setDocumentCreator(
        [] { return new TextEditor::TextDocument(Core::Constants::K_DEFAULT_TEXT_EDITOR_ID); });
    factory.setEditorWidgetCreator([] { return new PlainTextEditorWidget; });
    factory.setUseGenericHighlighter(true);
}

} // namespace

QNVimPlugin::QNVimPlugin() = default;

QNVimPlugin::~QNVimPlugin() = default;

Utils::Result<> QNVimPlugin::initialize(const QStringList &arguments) {
    Q_UNUSED(arguments)

    Core::IOptionsPage::registerCategory(Constants::SETTINGS_CATEGORY_ID, tr("QNVim"),
                                         Utils::FilePath());
    m_settingsPage = std::make_unique<QNVimSettingsPage>();

    Core::ActionContainer *menu = Core::ActionManager::createMenu(Constants::MENU_ID);
    menu->menu()->setTitle(tr("QNVim"));
    Core::ActionManager::actionContainer(Core::Constants::M_TOOLS)->addMenu(menu);

    qunsetenv("NVIM_LISTEN_ADDRESS");

    m_guiMode = settings().editorMode() == EditorMode::NeovimGui;
    if (m_guiMode)
        setupGuiMode();
    else
        setupClassicMode();

    return {};
}

void QNVimPlugin::setupClassicMode() {
    new HelpEditorFactory();
    new TerminalEditorFactory();

    auto action = new QAction(tr("Toggle QNVim"), this);
    Core::Command *cmd = Core::ActionManager::registerAction(action, Constants::TOGGLE_ID,
                                                             Core::Context(Core::Constants::C_GLOBAL));
    cmd->setDefaultKeySequence(QKeySequence(tr("Alt+Shift+V,Alt+Shift+V")));
    connect(action, &QAction::triggered, this, &QNVimPlugin::toggleQNVim);
    Core::ActionManager::actionContainer(Constants::MENU_ID)->addAction(cmd);

    m_core = std::make_unique<QNVimCore>();
}

void QNVimPlugin::setupGuiMode() {
    // The factory must exist before Core reads the preferred editors (in its
    // extensionsInitialized(), after ours).
    m_guiSession = std::make_unique<Gui::NvimGuiSession>();

    Core::ActionContainer *menu = Core::ActionManager::actionContainer(Constants::MENU_ID);

    auto showAction = new QAction(tr("Show Neovim"), this);
    Core::Command *showCmd = Core::ActionManager::registerAction(
        showAction, Gui::Constants::FOCUS_ACTION_ID, Core::Context(Core::Constants::C_GLOBAL));
    showCmd->setDefaultKeySequence(QKeySequence(tr("Alt+Shift+V,Alt+Shift+V")));
    connect(showAction, &QAction::triggered, this, &QNVimPlugin::showNeovim);
    menu->addAction(showCmd);

    auto restartAction = new QAction(tr("Restart Neovim"), this);
    Core::Command *restartCmd = Core::ActionManager::registerAction(
        restartAction, Gui::Constants::RESTART_ACTION_ID, Core::Context(Core::Constants::C_GLOBAL));
    connect(restartAction, &QAction::triggered, this, [this] {
        if (m_guiSession)
            m_guiSession->restart();
    });
    menu->addAction(restartCmd);
}

void QNVimPlugin::showNeovim() {
    // Focus an editor that shows Neovim, or open an empty one (Neovim needs an
    // editor to be drawn in, e.g. for its start screen without any file).
    for (Core::IEditor *editor : Core::EditorManager::visibleEditors()) {
        if (editor->document()->id() == Gui::Constants::DOCUMENT_ID) {
            Core::EditorManager::activateEditor(editor);
            return;
        }
    }
    QString title = tr("Neovim");
    Core::EditorManager::openEditorWithContents(Gui::Constants::EDITOR_ID, &title);
}

void QNVimPlugin::extensionsInitialized() {
    // Runs before Core's extensionsInitialized(), which reads the preferred
    // editor of each MIME type.
    if (m_guiMode)
        m_guiSession->takeOverTextEditors();
    else
        Gui::NvimGuiSession::releaseTextEditors();
}

ExtensionSystem::IPlugin::ShutdownFlag QNVimPlugin::aboutToShutdown() {
    m_core = nullptr;
    if (m_guiSession)
        m_guiSession->shutdown();
    return SynchronousShutdown;
}

void QNVimPlugin::toggleQNVim() {
    qDebug(Main) << "QNVimPlugin::toggleQNVim";

    if (m_core)
        m_core = nullptr;
    else
        m_core = std::make_unique<QNVimCore>();
}

HelpEditorFactory::HelpEditorFactory() : TextEditorFactory() {
    setId("Help");
    setDisplayName("Help");
    addMimeType("text/plain");

    setupPlainTextCreators(*this);
}

TerminalEditorFactory::TerminalEditorFactory() : TextEditorFactory() {
    setId("Terminal");
    setDisplayName("Terminal");
    addMimeType("text/plain");

    setupPlainTextCreators(*this);
}

} // namespace Internal
} // namespace QNVim
