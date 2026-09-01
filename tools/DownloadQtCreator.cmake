# SPDX-FileCopyrightText: 2022 Mikhail Zolotukhin <mail@gikari.com>
# SPDX-License-Identifier: MIT

set(QTC_EXT_DIR "${CMAKE_CURRENT_LIST_DIR}/../external/qtcreator")

# Fetch Qt Creator Version
include("${QTC_EXT_DIR}/version.cmake")

# Notify CI about Qt Creator version (only when running in GitHub Actions)
if (DEFINED ENV{GITHUB_OUTPUT} AND NOT "$ENV{GITHUB_OUTPUT}" STREQUAL "")
  file(APPEND "$ENV{GITHUB_OUTPUT}" "qtc_ver=${QT_CREATOR_VERSION}\n")
endif()

string(REGEX MATCH "([0-9]+.[0-9]+).[0-9]+" outvar "${QT_CREATOR_VERSION}")

set(qtc_base_url "https://download.qt.io/official_releases/qtcreator/${CMAKE_MATCH_1}/${QT_CREATOR_VERSION}/installer_source")

if (QT_CREATOR_SNAPSHOT)
  set(qtc_base_url "https://download.qt.io/snapshots/qtcreator/${CMAKE_MATCH_1}/${QT_CREATOR_VERSION}/installer_source/${QT_CREATOR_SNAPSHOT}")
endif()

if ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Windows")
  if (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
    set(qtc_platform "windows_arm64")
  else()
    set(qtc_platform "windows_x64")
  endif()
elseif ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Linux")
  if (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
    set(qtc_platform "linux_arm64")
  else()
    set(qtc_platform "linux_x64")
  endif()
elseif ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Darwin")
  # Qt Creator 20 still uses mac_x64 (universal binary) for both Intel and Apple Silicon
  set(qtc_platform "mac_x64")
endif()

set(QTC_DIST_DIR "${QTC_EXT_DIR}/dist-${CMAKE_HOST_SYSTEM_NAME}-${QT_CREATOR_VERSION}")

file(MAKE_DIRECTORY "${QTC_DIST_DIR}")

foreach(package qtcreator qtcreator_dev)
  set(archive_dest "${CMAKE_CURRENT_BINARY_DIR}/${package}.7z")

  # Use a local archive if QTC_LOCAL_ARCHIVES_DIR is set and the file exists there
  if (DEFINED QTC_LOCAL_ARCHIVES_DIR AND EXISTS "${QTC_LOCAL_ARCHIVES_DIR}/${package}.7z")
    message(STATUS "Using local archive: ${QTC_LOCAL_ARCHIVES_DIR}/${package}.7z")
    file(COPY_FILE "${QTC_LOCAL_ARCHIVES_DIR}/${package}.7z" "${archive_dest}")
  else()
    # Resume partial download if present — file(DOWNLOAD) always overwrites, so use curl -C - when available
    set(_resume_supported FALSE)
    if (EXISTS "${archive_dest}")
      file(SIZE "${archive_dest}" _existing_size)
      if (_existing_size GREATER 1024)
        find_program(_curl_cmd curl)
        if (_curl_cmd)
          message(STATUS "Resuming ${package} from ${_existing_size} bytes ...")
          execute_process(
            COMMAND "${_curl_cmd}" -L -C - --progress-bar -o "${archive_dest}" "${qtc_base_url}/${qtc_platform}/${package}.7z"
            RESULT_VARIABLE _curl_result
          )
          if (_curl_result EQUAL 0)
            set(_resume_supported TRUE)
          else()
            message(STATUS "Resume failed (curl exit ${_curl_result}), falling back to fresh download")
            file(REMOVE "${archive_dest}")
          endif()
        endif()
      endif()
    endif()
    if (NOT _resume_supported)
      message(STATUS "Downloading ${package} from ${qtc_base_url}/${qtc_platform}/${package}.7z ...")
      file(DOWNLOAD
          "${qtc_base_url}/${qtc_platform}/${package}.7z"
          "${archive_dest}"
          SHOW_PROGRESS
          STATUS download_status
          LOG download_log
      )
      list(GET download_status 0 download_error_code)
      list(GET download_status 1 download_error_msg)
      if (download_error_code)
          message(FATAL_ERROR "Download failed for ${package}: ${download_error_msg}\n${download_log}")
      endif()
    endif()
    file(SIZE "${archive_dest}" package_size)
    if (package_size LESS 1024)
        message(FATAL_ERROR "Downloaded ${package}.7z is too small (${package_size} bytes) -- URL likely 404:\n${qtc_base_url}/${qtc_platform}/${package}.7z")
    endif()
  endif()

  message(STATUS "Extracting ${package}...")
  execute_process(
      COMMAND ${CMAKE_COMMAND} -E tar xf "${archive_dest}"
      WORKING_DIRECTORY "${QTC_DIST_DIR}"
      RESULT_VARIABLE extract_result
  )
  if (extract_result)
      message(FATAL_ERROR "Extraction failed for ${package}: ${extract_result}")
  endif()
endforeach()
