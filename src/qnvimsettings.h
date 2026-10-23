#pragma once

#include <utils/aspects.h>

namespace QNVim {
namespace Internal {

// Which side renders Neovim's command line, messages and completion menu.
enum class WidgetsProvider {
    QNVim,
    NeovimPlugins,
};

// How Neovim is shown in Qt Creator.
enum class EditorMode {
    // Neovim's own screen, drawn by neovim-qt's Shell in the editor area.
    NeovimGui,
    // Neovim edits, Qt Creator's text editors draw the text (original qnvim).
    Classic,
};

class QNVimSettings final : public Utils::AspectContainer {
  public:
    QNVimSettings();

    WidgetsProvider widgetsProvider() const;
    EditorMode editorMode() const;

    Utils::SelectionAspect editorModeAspect{this};

    // GUI mode
    Utils::StringAspect nvimExecutable{this};
    Utils::StringAspect extraArguments{this};
    Utils::BoolAspect takeOverTextFiles{this};
    Utils::StringAspect excludedMimeTypes{this};
    Utils::StringAspect passthroughKeys{this};
    Utils::BoolAspect useEditorFont{this};
    Utils::BoolAspect followProjectDirectory{this};

    // Classic mode
    Utils::SelectionAspect widgetsProviderAspect{this};
};

QNVimSettings &settings();

} // namespace Internal
} // namespace QNVim
