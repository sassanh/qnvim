cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0135 NEW)

project(QNVim)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(FetchNeovimQt)
include(FetchQtCreator)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)

find_package(QtCreator REQUIRED COMPONENTS Core)
find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets Network REQUIRED)
set(QtX Qt${QT_VERSION_MAJOR})

add_qtc_plugin(QNVim
  PLUGIN_DEPENDS
    QtCreator::Core
    QtCreator::TextEditor
    QtCreator::ProjectExplorer
  DEPENDS
    ${QtX}::Widgets
    QtCreator::ExtensionSystem
    QtCreator::Utils
    neovim-qt
    neovim-qt-gui
  SOURCES
    src/qnvimplugin.cpp
    src/numbers_column.cpp
    src/qnvimplugin.h
    src/numbers_column.h
    src/qnvim_global.h
    src/qnvimconstants.h
)