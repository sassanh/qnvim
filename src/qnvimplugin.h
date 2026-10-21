#pragma once

#include <extensionsystem/iplugin.h>
#include <texteditor/texteditor.h>
#include <utils/result.h>

namespace QNVim {
namespace Internal {

class QNVimCore;
class QNVimSettingsPage;
class NumbersColumn;

class QNVimPlugin : public ExtensionSystem::IPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "QNVim.json")

  public:
    QNVimPlugin();
    ~QNVimPlugin() override;

    Utils::Result<> initialize(const QStringList &arguments) override;
    void extensionsInitialized() override;
    ShutdownFlag aboutToShutdown() override;

    void toggleQNVim();

  private:
    std::unique_ptr<QNVimCore> m_core;
    std::unique_ptr<QNVimSettingsPage> m_settingsPage;
};

class HelpEditorFactory : public TextEditor::TextEditorFactory {
  public:
    explicit HelpEditorFactory();
};

class TerminalEditorFactory : public TextEditor::TextEditorFactory {
  public:
    explicit TerminalEditorFactory();
};

} // namespace Internal
} // namespace QNVim
