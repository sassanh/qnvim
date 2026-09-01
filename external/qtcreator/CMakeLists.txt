# SPDX-FileCopyrightText: 2022 Mikhail Zolotukhin <mail@gikari.com>
# SPDX-License-Identifier: MIT

include("version.cmake")

set(QTC_DIST_PATH
    "${PROJECT_SOURCE_DIR}/external/qtcreator/dist-${CMAKE_SYSTEM_NAME}-${QT_CREATOR_VERSION}")
# Check for successful extraction — handle both old (19.x) and new (20.x) layouts where dev files may be in Qt Creator.app or Qt Creator.sdk
set(_qtc_config_markers
  "${QTC_DIST_PATH}/Qt Creator.app/Contents/Resources/lib/cmake/QtCreator/QtCreatorConfig.cmake"
  "${QTC_DIST_PATH}/lib/cmake/QtCreator/QtCreatorConfig.cmake"
  "${QTC_DIST_PATH}/Qt Creator.sdk/lib/cmake/QtCreator/QtCreatorConfig.cmake"
  "${QTC_DIST_PATH}/Qt Creator.sdk/Contents/Resources/lib/cmake/QtCreator/QtCreatorConfig.cmake"
)
set(_qtc_config_found FALSE)
foreach(_m IN LISTS _qtc_config_markers)
  if (EXISTS "${_m}")
    set(_qtc_config_found TRUE)
    break()
  endif()
endforeach()
if (NOT _qtc_config_found)
    set(qtc_local_archives_arg "")
    if (DEFINED QTC_LOCAL_ARCHIVES_DIR)
        set(qtc_local_archives_arg "-DQTC_LOCAL_ARCHIVES_DIR=${QTC_LOCAL_ARCHIVES_DIR}")
    endif()
    execute_process(COMMAND
        ${CMAKE_COMMAND} ${qtc_local_archives_arg}
        -P "${PROJECT_SOURCE_DIR}/tools/DownloadQtCreator.cmake"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
endif()

if (APPLE)
    list(APPEND CMAKE_PREFIX_PATH "${QTC_DIST_PATH}/Qt Creator.app/Contents/Resources")
    list(APPEND CMAKE_PREFIX_PATH "${QTC_DIST_PATH}/Qt Creator.sdk")
    list(APPEND CMAKE_PREFIX_PATH "${QTC_DIST_PATH}/Qt Creator.sdk/Contents/Resources")
endif()
if (EXISTS "${QTC_DIST_PATH}/lib/cmake/QtCreator")
    list(APPEND CMAKE_PREFIX_PATH "${QTC_DIST_PATH}")
else()
    # Fallback: add the dist root anyway so any layout is searched
    list(APPEND CMAKE_PREFIX_PATH "${QTC_DIST_PATH}")
endif()

set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
