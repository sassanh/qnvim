# SPDX-FileCopyrightText: 2022 Mikhail Zolotukhin <mail@gikari.com>
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.22)

if (POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

# Allow FetchContent dependencies that declare old CMake minimum versions (e.g. msgpackc)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

project(QNVim)

option(FETCH_QTC "Download Qt Creator development files automatically" ON)

add_subdirectory(external)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options("/permissive-")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
find_package(Qt6TaskTree QUIET)
if (NOT TARGET Qt6::TaskTree)
  # CI Qt 6.8 lacks QtTaskTree (added in 6.11). Provide full headers from Qt 6.11 for compilation.
  # The library is header-only for our usage (kitaspect.h only needs types); no linking required.
  file(COPY ${CMAKE_SOURCE_DIR}/external/dummy_tasktree/QtTaskTree DESTINATION ${CMAKE_BINARY_DIR}/dummy_tasktree)
  add_library(Qt6::TaskTree INTERFACE IMPORTED)
  target_include_directories(Qt6::TaskTree INTERFACE ${CMAKE_BINARY_DIR}/dummy_tasktree)
endif()
find_package(QtCreator REQUIRED COMPONENTS Core)

add_subdirectory(src)
