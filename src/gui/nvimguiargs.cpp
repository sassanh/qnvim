#include "nvimguiargs.h"

#include <QDir>
#include <QFileInfo>

namespace QNVim {
namespace Gui {

namespace {
// A Lua long string literal that can hold any path (spaces, quotes, ...).
QString luaString(const QString &s) {
    QString level = QStringLiteral("==");
    while (s.contains(QStringLiteral("]") + level + QStringLiteral("]")))
        level += QLatin1Char('=');
    return QStringLiteral("[") + level + QStringLiteral("[") + s + QStringLiteral("]") + level
           + QStringLiteral("]");
}
} // namespace

QStringList nvimGuiArguments(const QString &luaFile, const QString &shimFile,
                             const QString &listenAddress, const QStringList &extraArgs) {
    QStringList args;
    args << QStringLiteral("--cmd") << QStringLiteral("let g:QNVIM=1")
         << QStringLiteral("--cmd") << QStringLiteral("let g:QNVIM_GUI=1");
    if (!shimFile.isEmpty()) {
        // The shim provides :GuiFont, :GuiPopupmenu, ... It is put on
        // 'runtimepath' for nvim-qt's own `:runtime plugin/nvim_gui_shim.vim`,
        // and also sourced directly because plugin managers (lazy.nvim) may
        // reset that option. It guards against loading twice.
        const QString runtimeDir = QFileInfo(shimFile).dir().absolutePath() + QStringLiteral("/..");
        args << QStringLiteral("--cmd")
             << QStringLiteral("lua vim.opt.rtp:append(vim.fs.normalize(%1))")
                    .arg(luaString(QDir::cleanPath(runtimeDir)));
        args << QStringLiteral("--cmd")
             << QStringLiteral("lua vim.cmd.source(vim.fn.fnameescape(%1))").arg(luaString(shimFile));
    }
    if (!luaFile.isEmpty())
        args << QStringLiteral("--cmd") << QStringLiteral("lua dofile(%1)").arg(luaString(luaFile));
    if (!listenAddress.isEmpty())
        args << QStringLiteral("--listen") << listenAddress;
    args << extraArgs;
    return args;
}

} // namespace Gui
} // namespace QNVim
