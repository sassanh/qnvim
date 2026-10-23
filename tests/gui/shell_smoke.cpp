// Smoke test for qnvim's GUI mode without Qt Creator.
//
// Starts Neovim with exactly the arguments NvimGuiSession uses
// (nvimGuiArguments), shows neovim-qt's Shell widget, waits for the UI to
// attach, opens a file through the Lua glue, types a command line, and saves a
// screenshot. It checks that:
//   - Neovim starts and the Shell attaches (startup order: channel variable
//     before ui_attach, calls after attaching),
//   - qnvim_gui.lua is loaded (QNVimGui global, :Qtc) and reports 'bufenter',
//   - nvim-qt's GUI shim is loaded (:GuiFont exists),
//   - the grid is drawn (screenshot is not blank).
//
//   QT_QPA_PLATFORM=offscreen ./qnvim_gui_smoke <qnvim_gui.lua> <nvim_gui_shim.vim> \
//       <file to open> <screenshot.png> [nvim]

#include "nvimguiargs.h"

#include <gui/shell.h>
#include <msgpackrequest.h>
#include <neovimconnector.h>

#include <QApplication>
#include <QImage>
#include <QTimer>

#include <cstdio>

using namespace NeovimQt;

namespace {
int failures = 0;
void check(bool ok, const char *what) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        ++failures;
}
} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false); // the Shell closes when Neovim quits
    if (argc < 5) {
        std::fprintf(stderr, "usage: %s <lua> <shim> <file> <png> [nvim]\n", argv[0]);
        return 2;
    }
    const QString lua = QString::fromLocal8Bit(argv[1]);
    const QString shim = QString::fromLocal8Bit(argv[2]);
    const QString file = QString::fromLocal8Bit(argv[3]);
    const QString png = QString::fromLocal8Bit(argv[4]);
    const QString exe = argc > 5 ? QString::fromLocal8Bit(argv[5]) : QStringLiteral("nvim");

    const QStringList args = QNVim::Gui::nvimGuiArguments(lua, shim, {}, {QStringLiteral("--clean")});
    NeovimConnector *nvim = NeovimConnector::spawn(args, exe);

    bool gotBufEnter = false;
    bool attached = false;

    QObject::connect(nvim, &NeovimConnector::ready, [&] {
        nvim->api2()->nvim_set_var("qnvim_gui_channel", QVariant::fromValue<quint64>(nvim->channel()));
        QObject::connect(nvim->api0(), &NeovimApi0::neovimNotification,
                         [&](const QByteArray &name, const QVariantList &a) {
                             if (name == "QNVimGui" && !a.isEmpty() && a.at(0).toByteArray() == "bufenter"
                                 && a.value(1).toString().endsWith(QStringLiteral("smoke.cpp")))
                                 gotBufEnter = true;
                         });
    });
    QObject::connect(nvim, &NeovimConnector::error, [&] {
        std::fprintf(stderr, "neovim error: %s\n", qPrintable(nvim->errorString()));
        app.exit(3);
    });

    Shell shell(nvim);
    shell.setGuiFont(QStringLiteral("Monospace:h11"), true);
    shell.resize(900, 560);

    QObject::connect(&shell, &Shell::neovimAttachmentChanged, [&](bool ok) {
        if (!ok || attached)
            return;
        attached = true;
        // Like NvimGuiSession: calls into the glue only after attaching.
        nvim->api2()->nvim_call_function(
            "luaeval", {QStringLiteral("QNVimGui.open(_A[1], _A[2], _A[3], _A[4])"),
                        QVariantList{file, 3, 4, false}});
        auto guiFont = nvim->api2()->nvim_eval("exists(':GuiFont')");
        QObject::connect(guiFont, &MsgpackRequest::finished,
                         [](quint32, quint64, const QVariant &v) {
                             check(v.toInt() == 2, "nvim-qt shim loaded (:GuiFont)");
                         });
        auto qtc = nvim->api2()->nvim_eval("exists(':Qtc') && luaeval('type(QNVimGui)') ==# 'table'");
        QObject::connect(qtc, &MsgpackRequest::finished, [](quint32, quint64, const QVariant &v) {
            check(v.toInt() == 1, "qnvim_gui.lua loaded (:Qtc, QNVimGui)");
        });
        // Something on the command line, drawn by Neovim into the grid.
        QTimer::singleShot(400, [&] { nvim->api2()->nvim_input(":echo 'qnvim gui'"); });
        QTimer::singleShot(1500, [&] {
            const QImage img = shell.grab().toImage();
            img.save(png);
            // Not blank: more than one distinct colour.
            QSet<QRgb> colors;
            for (int y = 0; y < img.height(); y += 7)
                for (int x = 0; x < img.width(); x += 7)
                    colors.insert(img.pixel(x, y));
            check(colors.size() > 2, "grid drawn (screenshot has content)");
            check(gotBufEnter, "bufenter notification for the opened file");
            check(shell.rows() > 10 && shell.columns() > 40, "grid sized to the widget");
            // A completion menu, drawn by Neovim into the grid (no ext_popupmenu).
            nvim->api2()->nvim_input("<Esc><Esc>Go    i<C-n>");
        });
        QTimer::singleShot(2500, [&] {
            const QString pumPng = QString(png).replace(QStringLiteral(".png"), QStringLiteral("-pum.png"));
            shell.grab().save(pumPng);
            auto pum = nvim->api2()->nvim_eval("pumvisible()");
            QObject::connect(pum, &MsgpackRequest::finished, [&](quint32, quint64, const QVariant &v) {
                check(v.toInt() == 1, "completion menu visible");
                nvim->api2()->nvim_command("qall!");
                QTimer::singleShot(300, &app, [&] { app.exit(failures ? 1 : 0); });
            });
        });
    });

    shell.show();
    QTimer::singleShot(15000, &app, [&] {
        check(attached, "UI attached within 15 s");
        app.exit(4);
    });
    const int rc = app.exec();
    std::printf("%s\n", rc == 0 ? "SMOKE OK" : "SMOKE FAILED");
    return rc;
}
