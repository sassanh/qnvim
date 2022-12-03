# SPDX-FileCopyrightText: 2022 Mikhail Zolotukhin <mail@gikari.com>
# SPDX-License-Identifier: MIT

set(QTC_EXT_DIR "${CMAKE_CURRENT_LIST_DIR}/../external/qtcreator")

# Fetch Qt Creator Version
include("${QTC_EXT_DIR}/version.cmake")

# Notify CI about Qt Creator version
file(APPEND $ENV{GITHUB_OUTPUT} "qtc_ver=${QT_CREATOR_VERSION}")

string(REGEX MATCH "([0-9]+.[0-9]+).[0-9]+" outvar "${QT_CREATOR_VERSION}")

set(qtc_base_url "https://download.qt.io/official_releases/qtcreator/\
${CMAKE_MATCH_1}/${QT_CREATOR_VERSION}/installer_source")

if (QT_CREATOR_SNAPSHOT)
  set(qtc_base_url "https://download.qt.io/snapshots/qtcreator/\
${CMAKE_MATCH_1}/${QT_CREATOR_VERSION}/installer_source/${QT_CREATOR_SNAPSHOT}")
endif()

if ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Windows")
  set(qtc_platform "windows_x64")
elseif ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Linux")
  set(qtc_platform "linux_x64")
elseif ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Darwin")
  set(qtc_platform "mac_x64")
endif()

set(QTC_DIST_DIR "${QTC_EXT_DIR}/dist-${CMAKE_HOST_SYSTEM_NAME}-${QT_CREATOR_VERSION}")

file(MAKE_DIRECTORY "${QTC_DIST_DIR}")

message(STATUS "Downloading Qt Creator from ${qtc_base_url}/${qtc_platform}...")

foreach(package qtcreator qtcreator_dev)
  message(STATUS "Downloading ${package}...")
  file(DOWNLOAD
      "${qtc_base_url}/${qtc_platform}/${package}.7z"
      "${CMAKE_CURRENT_BINARY_DIR}/${package}.7z"
  )
  message(STATUS "Extracting ${package}...")
  execute_process(
      COMMAND ${CMAKE_COMMAND} -E tar xf "${CMAKE_CURRENT_BINARY_DIR}/${package}.7z"
      WORKING_DIRECTORY "${QTC_DIST_DIR}"
  )
endforeach()
