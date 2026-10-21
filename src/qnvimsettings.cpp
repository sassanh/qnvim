#include "qnvimsettings.h"

#include <utils/layoutbuilder.h>

#include <QCoreApplication>
#include <QLabel>

namespace QNVim {
namespace Internal {

namespace {

QString tr(const char *text) {
    return QCoreApplication::translate("QNVim", text);
}

} // namespace

QNVimSettings::QNVimSettings() {
    setSettingsGroup("QNVim");
    setAutoApply(false);

    widgetsProviderAspect.setSettingsKey("WidgetsProvider");
    widgetsProviderAspect.setDisplayStyle(
        Utils::SelectionAspect::DisplayStyle::RadioButtons);
    widgetsProviderAspect.addOption(
        {tr("Rendered by QNVim"),
         tr("QNVim renders Neovim's command line, messages and completion menu."),
         QStringLiteral("qnvim")});
    widgetsProviderAspect.addOption(
        {tr("Rendered by Neovim plugins"),
         tr("Let Neovim plugins such as noice.nvim render the command line, messages and "
            "completion menu."),
         QStringLiteral("plugins")});
    widgetsProviderAspect.setDefaultValue(0);
    widgetsProviderAspect.setUseDataAsSavedValue();

    setLayouter([this] {
        using namespace Layouting;

        auto restartNote = new QLabel(
            tr("Changes take effect after QNVim restarts (toggle it off and on)."));
        restartNote->setWordWrap(true);

        return Column{
            tr("Command line, messages and completion menu:"),
            widgetsProviderAspect,
            restartNote,
            st,
        };
    });

    readSettings();
}

WidgetsProvider QNVimSettings::widgetsProvider() const {
    return widgetsProviderAspect.itemValue().toString() == QLatin1String("plugins")
               ? WidgetsProvider::NeovimPlugins
               : WidgetsProvider::QNVim;
}

QNVimSettings &settings() {
    static QNVimSettings theSettings;
    return theSettings;
}

} // namespace Internal
} // namespace QNVim
