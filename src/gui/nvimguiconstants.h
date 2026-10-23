#pragma once

namespace QNVim {
namespace Gui {
namespace Constants {

// Editor id of the embedded Neovim editor. It is stored in Qt Creator's
// "EditorManager/PreferredEditorFactories" setting, so keep it stable.
const char EDITOR_ID[] = "QNVim.NeovimGuiEditor";
const char EDITOR_DISPLAY_NAME[] = "Neovim (qnvim)";
const char DOCUMENT_ID[] = "QNVim.NeovimGuiDocument";

const char RESTART_ACTION_ID[] = "QNVim.Gui.Restart";
const char FOCUS_ACTION_ID[] = "QNVim.Gui.Focus";

// Notification method name used by qnvim_gui.lua.
const char RPC_METHOD[] = "QNVimGui";

} // namespace Constants
} // namespace Gui
} // namespace QNVim
