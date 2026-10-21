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
    textediteventfilter.cpp
    textediteventfilter.h
    textpos.cpp
    textpos.h
)

# Compress the resource with zlib instead of zstd: rcc's zstd resources refer to
# qt_resourceFeatureZstd, which some QtCore versions do not export (the Qt 6.11
# of the test Qt Creator, for one), and the plugin would fail to load there.
set_source_files_properties(qnvim.qrc PROPERTIES AUTORCC_OPTIONS "--compress-algo;zlib")
