cmake_minimum_required(VERSION 3.22)

if (POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

project(QNVim)

option(FETCH_QTC "Download Qt Creator development files automatically" ON)

add_subdirectory(external)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options("/permissive-")
endif()

find_package(QtCreator REQUIRED COMPONENTS Core)
find_package(Qt5 COMPONENTS Widgets Network REQUIRED)

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
    src/qnvimplugin.cpp
    src/numbers_column.cpp
    src/qnvimplugin.h
    src/numbers_column.h
    src/qnvim_global.h
    src/qnvimconstants.h
)
