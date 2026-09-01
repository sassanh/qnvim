# SPDX-FileCopyrightText: 2022 Mikhail Zolotukhin <mail@gikari.com>
# SPDX-License-Identifier: MIT

set(QTC_EXT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../external/qtcreator")
set(QTC_DIR "${QTC_EXT_DIR}/dist-${CMAKE_HOST_SYSTEM_NAME}-$ENV{QT_CREATOR_VERSION}")

# Resolve Qt directory: allow QT_DIR from install-qt-action (QT_ROOT_DIR) or legacy DownloadQt
set(_qt_dir "$ENV{QT_DIR}")
if (NOT _qt_dir OR NOT EXISTS "${_qt_dir}")
  if (DEFINED ENV{Qt6_DIR})
    get_filename_component(_qt_dir "$ENV{Qt6_DIR}/../../.." ABSOLUTE)
  elseif (DEFINED ENV{QT_ROOT_DIR})
    set(_qt_dir "$ENV{QT_ROOT_DIR}")
  endif()
endif()
if (NOT EXISTS "${_qt_dir}")
  message(FATAL_ERROR "Qt directory not found: ${_qt_dir} (QT_DIR=$ENV{QT_DIR} Qt6_DIR=$ENV{Qt6_DIR} QT_ROOT_DIR=$ENV{QT_ROOT_DIR})")
endif()
set(ENV{QT_DIR} "${_qt_dir}")
message(STATUS "Using Qt at: $ENV{QT_DIR}")
message(STATUS "Using Qt Creator at: ${QTC_DIR}")

set(build_plugin_py "scripts/build_plugin.py")
foreach(dir "share/qtcreator/scripts" "Qt Creator.app/Contents/Resources/scripts" "Contents/Resources/scripts" "Qt Creator.sdk/share/qtcreator/scripts")
  if(EXISTS "${QTC_DIR}/${dir}/build_plugin.py")
    set(build_plugin_py "${dir}/build_plugin.py")
    break()
  endif()
endforeach()

execute_process(
  COMMAND python3
    -u
    "${QTC_DIR}/${build_plugin_py}"
    --name "$ENV{PLUGIN_NAME}-$ENV{QT_CREATOR_VERSION}-$ENV{ARTIFACT_SUFFIX}"
    --src .
    --build build
    --qt-path "$ENV{QT_DIR}"
    --qtc-path "${QTC_DIR}"
    --output-path "$ENV{GITHUB_WORKSPACE}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE output
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_STRIP_TRAILING_WHITESPACE
)
if (NOT result EQUAL 0)
  string(REGEX MATCH "FAILED:.*$" error_message "${output}")
  string(REPLACE "\n" "%0A" error_message "${error_message}")
  message("::error::${error_message}")
  message(STATUS "Full output:\n${output}")
  message(FATAL_ERROR "Build failed")
endif()
