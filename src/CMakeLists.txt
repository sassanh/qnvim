# GUI mode resources: qnvim's Lua glue and nvim-qt's GUI shim (:GuiFont, ...),
# taken from the neovim-qt sources so it always matches the Shell widget.
set(_shim "${QNVIM_NEOVIMQT_SOURCE_DIR}/src/gui/runtime/plugin/nvim_gui_shim.vim")
if (NOT EXISTS "${_shim}")
  message(FATAL_ERROR "neovim-qt GUI shim not found at ${_shim}")
endif()
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/qnvim_gui.qrc" CONTENT
"<RCC>
    <qresource prefix=\"/qnvim/gui\">
        <file alias=\"qnvim_gui.lua\">${CMAKE_CURRENT_SOURCE_DIR}/gui/qnvim_gui.lua</file>
        <file alias=\"nvim_gui_shim.vim\">${_shim}</file>
    </qresource>
</RCC>
")
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/qnvim_gui.qrc"
  PROPERTIES AUTORCC_OPTIONS "--compress-algo;zlib")

add_qtc_plugin(QNVim
  PLUGIN_DEPENDS
    QtCreator::Core
    QtCreator::TextEditor
    QtCreator::ProjectExplorer
  DEPENDS
    Qt::Widgets
    QtCreator::ExtensionSystem
    QtCreator::Utils
    neovim-qt
    neovim-qt-gui
  SOURCES
    log.cpp
    log.h
    numbers_column.cpp
    numbers_column.h
    qnvim.qrc
    qnvim_global.h
    qnvimconstants.h
    qnvimplugin.cpp
    qnvimplugin.h
    qnvimcore.cpp
    qnvimcore.h
    qnvimfloat.cpp
    qnvimfloat.h
    qnvimsettings.cpp
    qnvimsettings.h
    qnvimsettingspage.cpp
    qnvimsettingspage.h
    cmdline.cpp
    cmdline.h
    gui/nvimguiargs.cpp
    gui/nvimguiargs.h
    gui/nvimguiconstants.h
    gui/nvimguieditor.cpp
    gui/nvimguieditor.h
    gui/nvimguisession.cpp
    gui/nvimguisession.h
    ${CMAKE_CURRENT_BINARY_DIR}/qnvim_gui.qrc
    textediteventfilter.cpp
    textediteventfilter.h
    textpos.cpp
    textpos.h
)

# Compress the resource with zlib instead of zstd: rcc's zstd resources refer to
# qt_resourceFeatureZstd, which some QtCore versions do not export (the Qt 6.11
# of the test Qt Creator, for one), and the plugin would fail to load there.
set_source_files_properties(qnvim.qrc PROPERTIES AUTORCC_OPTIONS "--compress-algo;zlib")
