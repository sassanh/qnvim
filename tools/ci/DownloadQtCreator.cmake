string(REGEX MATCH "([0-9]+.[0-9]+).[0-9]+" outvar "$ENV{QT_CREATOR_VERSION}")

set(qtc_base_url "https://download.qt.io/official_releases/qtcreator/${CMAKE_MATCH_1}/$ENV{QT_CREATOR_VERSION}/installer_source")
set(qtc_snapshot "$ENV{QT_CREATOR_SNAPSHOT}")
if (qtc_snapshot)
  set(qtc_base_url "https://download.qt.io/snapshots/qtcreator/${CMAKE_MATCH_1}/$ENV{QT_CREATOR_VERSION}/installer_source/${qtc_snapshot}")
endif()

if ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Windows")
  set(qtc_platform "windows_x64")
elseif ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Linux")
  set(qtc_platform "linux_x64")
elseif ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Darwin")
  set(qtc_platform "mac_x64")
endif()

file(TO_CMAKE_PATH "$ENV{GITHUB_WORKSPACE}/qtcreator" qtc_dir)
# Save the path for other steps
file(APPEND $ENV{GITHUB_OUTPUT} "qtc_dir=${qtc_dir}")

file(MAKE_DIRECTORY qtcreator)

message("Downloading Qt Creator from ${qtc_base_url}/${qtc_platform}")

foreach(package qtcreator qtcreator_dev)
  file(DOWNLOAD
    "${qtc_base_url}/${qtc_platform}/${package}.7z" ./${package}.7z SHOW_PROGRESS)
  execute_process(COMMAND
    ${CMAKE_COMMAND} -E tar xvf ../${package}.7z WORKING_DIRECTORY qtcreator)
endforeach()
