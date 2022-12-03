# SPDX-FileCopyrightText: 2022 Mikhail Zolotukhin <mail@gikari.com>
# SPDX-License-Identifier: MIT

add_qtc_plugin(QNVim
  PLUGIN_DEPENDS
    QtCreator::Core
    QtCreator::TextEditor
    QtCreator::ProjectExplorer
  DEPENDS
    Qt5::Widgets
    QtCreator::ExtensionSystem
    QtCreator::Utils
    neovim-qt
    neovim-qt-gui
  SOURCES
    log.cpp
    log.h
    numbers_column.cpp
    numbers_column.h
    qnvim_global.h
    qnvimconstants.h
    qnvimplugin.cpp
    qnvimplugin.h
)
