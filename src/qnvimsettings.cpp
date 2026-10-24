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

    editorModeAspect.setSettingsKey("EditorMode");
    editorModeAspect.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::RadioButtons);
    editorModeAspect.addOption(
        {tr("Neovim GUI in the editor area"),
         tr("Neovim draws its whole screen in Qt Creator's editor area, like nvim-qt: all "
            "plugins, windows, menus, the command line and messages appear exactly as in "
            "Neovim."),
         QStringLiteral("gui")});
    editorModeAspect.addOption(
        {tr("Classic (Qt Creator draws the text)"),
         tr("Neovim edits in the background; Qt Creator's own text editors show the text."),
         QStringLiteral("classic")});
    editorModeAspect.setDefaultValue(0);
    editorModeAspect.setUseDataAsSavedValue();

    nvimExecutable.setSettingsKey("Gui/NvimExecutable");
    nvimExecutable.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    nvimExecutable.setLabelText(tr("Neovim executable:"));
    nvimExecutable.setDefaultValue(QStringLiteral("nvim"));
    nvimExecutable.setToolTip(tr("Name in PATH or absolute path of the nvim binary."));

    extraArguments.setSettingsKey("Gui/ExtraArguments");
    extraArguments.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    extraArguments.setLabelText(tr("Additional arguments:"));
    extraArguments.setPlaceHolderText(tr("e.g. -u ~/.config/nvim-qtc/init.lua"));

    takeOverTextFiles.setSettingsKey("Gui/TakeOverTextFiles");
    takeOverTextFiles.setLabelText(tr("Open all text files in Neovim"));
    takeOverTextFiles.setToolTip(
        tr("Makes Neovim the default editor of every MIME type derived from text/plain. "
           "Other editors stay available through \"Open With\"."));
    takeOverTextFiles.setDefaultValue(true);

    excludedMimeTypes.setSettingsKey("Gui/ExcludedMimeTypes");
    excludedMimeTypes.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    excludedMimeTypes.setLabelText(tr("Keep Qt Creator's editor for:"));
    excludedMimeTypes.setDefaultValue(
        QStringLiteral("application/x-designer, application/scxml+xml, image/svg+xml"));
    excludedMimeTypes.setToolTip(tr("Comma-separated MIME types (and their sub-types) that keep "
                                    "their Qt Creator editor, e.g. Qt Designer forms."));

    passthroughKeys.setSettingsKey("Gui/PassthroughKeys");
    passthroughKeys.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    passthroughKeys.setLabelText(tr("Keys for Qt Creator:"));
    passthroughKeys.setDefaultValue(QStringLiteral(
        "Ctrl+1, Ctrl+2, Ctrl+3, Ctrl+4, Ctrl+5, Ctrl+6, Ctrl+7, "
        "Alt+0, Alt+1, Alt+2, Alt+3, Alt+4, Alt+5, Alt+6, Alt+7, Alt+8, Alt+9"));
    passthroughKeys.setToolTip(
        tr("While Neovim has the focus every key goes to Neovim, except these shortcuts, "
           "which keep their Qt Creator meaning (comma-separated, Qt notation). Everything "
           "else is reachable from Neovim with :Qtc <action>."));

    useEditorFont.setSettingsKey("Gui/UseEditorFont");
    useEditorFont.setLabelText(tr("Start with Qt Creator's text editor font"));
    useEditorFont.setToolTip(tr("Neovim's :GuiFont (e.g. in ginit.vim) still overrides it."));
    useEditorFont.setDefaultValue(true);

    followProjectDirectory.setSettingsKey("Gui/FollowProjectDirectory");
    followProjectDirectory.setLabelText(
        tr("Change Neovim's directory to the startup project's directory"));
    followProjectDirectory.setDefaultValue(true);

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
            tr("Changing the mode or the GUI options takes effect after restarting Qt Creator "
               "(the classic options: after toggling QNVim off and on)."));
        restartNote->setWordWrap(true);

        return Column{
            Group{title(tr("Mode")), Column{editorModeAspect}},
            Group{title(tr("Neovim GUI")),
                  Column{
                      Form{nvimExecutable, br,
                           extraArguments, br,
                           passthroughKeys, br,
                           excludedMimeTypes, br},
                      takeOverTextFiles,
                      useEditorFont,
                      followProjectDirectory,
                  }},
            Group{title(tr("Classic: command line, messages and completion menu")),
                  Column{widgetsProviderAspect}},
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

EditorMode QNVimSettings::editorMode() const {
    return editorModeAspect.itemValue().toString() == QLatin1String("classic")
               ? EditorMode::Classic
               : EditorMode::NeovimGui;
}

QNVimSettings &settings() {
    static QNVimSettings theSettings;
    return theSettings;
}

} // namespace Internal
} // namespace QNVim
