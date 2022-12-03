include("version.cmake")

set(QTC_DIST_PATH
    "${PROJECT_SOURCE_DIR}/external/qtcreator/dist-${CMAKE_SYSTEM_NAME}-${QT_CREATOR_VERSION}")
if (NOT EXISTS "${QTC_DIST_PATH}")
    execute_process(COMMAND
        ${CMAKE_COMMAND} -P "${PROJECT_SOURCE_DIR}/tools/DownloadQtCreator.cmake"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
endif()

if (APPLE)
    list(APPEND CMAKE_PREFIX_PATH "${QTC_DIST_PATH}/Qt Creator.app/Contents/Resources")
endif()

list(APPEND CMAKE_PREFIX_PATH "${QTC_DIST_PATH}")

set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
