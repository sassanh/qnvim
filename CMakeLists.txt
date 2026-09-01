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

find_package(Qt6 REQUIRED COMPONENTS Widgets Network TaskTree)
find_package(QtCreator REQUIRED COMPONENTS Core)

add_subdirectory(src)
