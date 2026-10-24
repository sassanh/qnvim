#pragma once

#include <QString>
#include <QStringList>

namespace QNVim {
namespace Gui {

// Writes qnvim_gui.lua and nvim-qt's shim (from the :/qnvim/gui resources)
// below runtimeRoot, creating directories as needed, so Neovim can read them:
//   <runtimeRoot>/qnvim_gui.lua
//   <runtimeRoot>/runtime/plugin/nvim_gui_shim.vim
// Unchanged files are not rewritten. Returns false and sets *error if a file
// could not be written; paths of files that exist are returned either way.
struct GuiRuntimeFiles {
    QString luaFile;
    QString shimFile;
};
bool extractGuiRuntime(const QString &runtimeRoot, GuiRuntimeFiles *files, QString *error);

// Neovim's command line arguments for GUI mode (NeovimConnector::spawn adds
// --embed). Kept free of Qt Creator types so tests/gui/shell_smoke.cpp can
// start Neovim exactly like the plugin does.
//
// luaFile:   path of qnvim_gui.lua
// shimFile:  path of runtime/plugin/nvim_gui_shim.vim (from neovim-qt)
// extraArgs: user arguments, already split
QStringList nvimGuiArguments(const QString &luaFile, const QString &shimFile,
                             const QString &listenAddress, const QStringList &extraArgs);

} // namespace Gui
} // namespace QNVim
