#pragma once

#include <utils/aspects.h>

namespace QNVim {
namespace Internal {

// Which side renders Neovim's command line, messages and completion menu.
enum class WidgetsProvider {
    QNVim,
    NeovimPlugins,
};

class QNVimSettings final : public Utils::AspectContainer {
  public:
    QNVimSettings();

    WidgetsProvider widgetsProvider() const;

    Utils::SelectionAspect widgetsProviderAspect{this};
};

QNVimSettings &settings();

} // namespace Internal
} // namespace QNVim
