#pragma once

#include <coreplugin/dialogs/ioptionspage.h>

namespace QNVim {
namespace Internal {

class QNVimSettingsPage final : public Core::IOptionsPage {
  public:
    QNVimSettingsPage();
};

} // namespace Internal
} // namespace QNVim
