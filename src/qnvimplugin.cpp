#include "qnvimplugin.h"

#include "qnvimcore.h"
#include "qnvimsettingspage.h"
#include "log.h"
#include "qnvimconstants.h"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/coreconstants.h>

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

    new HelpEditorFactory();
    new TerminalEditorFactory();

    auto action = new QAction(tr("Toggle QNVim"), this);
    Core::Command *cmd = Core::ActionManager::registerAction(action, Constants::TOGGLE_ID,
                                                             Core::Context(Core::Constants::C_GLOBAL));
    cmd->setDefaultKeySequence(QKeySequence(tr("Alt+Shift+V,Alt+Shift+V")));
    connect(action, &QAction::triggered, this, &QNVimPlugin::toggleQNVim);

    Core::ActionContainer *menu = Core::ActionManager::createMenu(Constants::MENU_ID);
    menu->menu()->setTitle(tr("QNVim"));
    menu->addAction(cmd);
    Core::ActionManager::actionContainer(Core::Constants::M_TOOLS)->addMenu(menu);

    qunsetenv("NVIM_LISTEN_ADDRESS");

    m_core = std::make_unique<QNVimCore>();

    return {};
}

void QNVimPlugin::extensionsInitialized() {
    // Retrieve objects from the plugin manager's object pool
    // In the extensionsInitialized function, a plugin can be sure that all
    // plugins that depend on it are completely initialized.
}

ExtensionSystem::IPlugin::ShutdownFlag QNVimPlugin::aboutToShutdown() {
    m_core = nullptr;
    // Save settings
    // Disconnect from signals that are not needed during shutdown
    // Hide UI (if you add UI that is not in the main window directly)
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
