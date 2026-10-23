#pragma once

#include <QString>
#include <QStringList>

namespace QNVim {
namespace Gui {

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
