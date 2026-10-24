#include "nvimguiargs.h"

#include <QDir>
#include <QFile>
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
bool writeIfChanged(const QString &resource, const QString &target, QString *error) {
    QFile in(resource);
    if (!in.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("missing resource %1").arg(resource);
        return false;
    }
    const QByteArray data = in.readAll();
    QFile existing(target);
    if (existing.open(QIODevice::ReadOnly) && existing.readAll() == data)
        return true;
    existing.close();
    if (!QDir().mkpath(QFileInfo(target).absolutePath())) {
        *error = QStringLiteral("cannot create directory %1").arg(QFileInfo(target).absolutePath());
        return false;
    }
    QFile out(target);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(data) != data.size()) {
        *error = QStringLiteral("cannot write %1: %2").arg(target, out.errorString());
        return false;
    }
    return true;
}
} // namespace

bool extractGuiRuntime(const QString &runtimeRoot, GuiRuntimeFiles *files, QString *error) {
    const QString lua = runtimeRoot + QStringLiteral("/qnvim_gui.lua");
    const QString shim = runtimeRoot + QStringLiteral("/runtime/plugin/nvim_gui_shim.vim");
    bool ok = writeIfChanged(QStringLiteral(":/qnvim/gui/qnvim_gui.lua"), lua, error);
    ok = writeIfChanged(QStringLiteral(":/qnvim/gui/nvim_gui_shim.vim"), shim, error) && ok;
    // Only hand Neovim files that exist; a missing --cmd file aborts startup
    // with a "Press ENTER" prompt.
    files->luaFile = QFileInfo::exists(lua) ? lua : QString();
    files->shimFile = QFileInfo::exists(shim) ? shim : QString();
    return ok;
}

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
