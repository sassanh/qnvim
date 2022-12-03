# SPDX-FileCopyrightText: 2022 Mikhail Zolotukhin <mail@gikari.com>
# SPDX-License-Identifier: MIT

set(qt_version "$ENV{QT_VERSION}")

string(REGEX MATCH "^[0-9]+" qt_version_major "${qt_version}")
string(REPLACE "." "" qt_version_dotless "${qt_version}")
if ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Windows")
  set(url_os "windows_x86")
  set(qt_package_arch_suffix "win64_msvc2019_64")
  set(qt_dir_prefix "${qt_version}/msvc2019_64")
  set(qt_package_suffix "-Windows-Windows_10-MSVC2019-Windows-Windows_10-X86_64")
elseif ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Linux")
  set(url_os "linux_x64")
  set(qt_package_arch_suffix "gcc_64")
  set(qt_dir_prefix "${qt_version}/gcc_64")
  if("${qt_version_major}" STREQUAL "5")
    set(qt_package_suffix "-Linux-RHEL_7_6-GCC-Linux-RHEL_7_6-X86_64")
  else()
    set(qt_package_suffix "-Linux-RHEL_8_2-GCC-Linux-RHEL_8_2-X86_64")
  endif()
elseif ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Darwin")
  set(url_os "mac_x64")
  set(qt_package_arch_suffix "clang_64")
  if("${qt_version_major}" STREQUAL "5")
    set(qt_dir_prefix "${qt_version}/clang_64")
    set(qt_package_suffix "-MacOS-MacOS_10_13-Clang-MacOS-MacOS_10_13-X86_64")
  else()
    set(qt_dir_prefix "${qt_version}/macos")
    set(qt_package_suffix "-MacOS-MacOS_11_00-Clang-MacOS-MacOS_11_00-X86_64-ARM64")
  endif()
endif()

set(qt_base_url "https://download.qt.io/online/qtsdkrepository/${url_os}/desktop/qt${qt_version_major}_${qt_version_dotless}")
file(DOWNLOAD "${qt_base_url}/Updates.xml" ./Updates.xml SHOW_PROGRESS)

file(READ ./Updates.xml updates_xml)
string(REGEX MATCH "<Name>qt.qt${qt_version_major}.*<Version>([0-9+-.]+)</Version>" updates_xml_output "${updates_xml}")
set(qt_package_version ${CMAKE_MATCH_1})

file(MAKE_DIRECTORY qt)

# Save the path for other steps
file(TO_CMAKE_PATH "$ENV{GITHUB_WORKSPACE}/qt/${qt_dir_prefix}" qt_dir)
file(APPEND $ENV{GITHUB_OUTPUT} "qt_dir=${qt_dir}")

message("Downloading Qt to ${qt_dir}")
function(downloadAndExtract url archive)
  message("Downloading ${url}")
  file(DOWNLOAD "${url}" ./${archive} SHOW_PROGRESS)
  execute_process(COMMAND ${CMAKE_COMMAND} -E tar xvf ../${archive} WORKING_DIRECTORY qt)
endfunction()

foreach(package qtbase qtdeclarative qtsvg qttools)
  downloadAndExtract(
    "${qt_base_url}/qt.qt${qt_version_major}.${qt_version_dotless}.${qt_package_arch_suffix}/${qt_package_version}${package}${qt_package_suffix}.7z"
    ${package}.7z
  )
endforeach()

if("${qt_version_major}" STREQUAL "6")
  foreach(package qt5compat qtshadertools)
    downloadAndExtract(
      "${qt_base_url}/qt.qt6.${qt_version_dotless}.${package}.${qt_package_arch_suffix}/${qt_package_version}${package}${qt_package_suffix}.7z"
      ${package}.7z
    )
  endforeach()
endif()

# uic depends on libicu56.so
if ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Linux")
  downloadAndExtract(
    "${qt_base_url}/qt.qt${qt_version_major}.${qt_version_dotless}.${qt_package_arch_suffix}/${qt_package_version}icu-linux-Rhel7.2-x64.7z"
    icu.7z
  )
endif()
