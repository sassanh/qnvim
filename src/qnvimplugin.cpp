// SPDX-FileCopyrightText: 2018-2019 Sassan Haradji <sassanh@gmail.com>
// SPDX-FileCopyrightText: 2023 Mikhail Zolotukhin <mail@gikari.com>
// SPDX-License-Identifier: MIT

#include "qnvimplugin.h"

#include "qnvimcore.h"
#include "log.h"
#include "qnvimconstants.h"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>

#include <QAction>
#include <QMenu>

namespace QNVim {
namespace Internal {

Utils::Result<> QNVimPlugin::initialize(const QStringList &arguments) {
    Q_UNUSED(arguments)

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
}

TerminalEditorFactory::TerminalEditorFactory() : TextEditorFactory() {
    setId("Terminal");
    setDisplayName("Terminal");
    addMimeType("text/plain");
}

} // namespace Internal
} // namespace QNVim
