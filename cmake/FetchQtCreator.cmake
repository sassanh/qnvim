include(FetchContent)

## Download Qt Creator binaries and development files,
## so that we can link to libraries and include headers

set(BASE_URL "https://github.com/qt-creator/qt-creator/releases/download")

# NOTE: Don't forget to change filenames and checksums below when changing the version
set(QTC_VER "4.15.2")

if (MINGW) # Windows MinGW
    set(QTC_FILENAME "qtcreator-Windows-MinGW-1029797214.7z")
    set(QTC_DEV_FILENAME "qtcreator-Windows-MinGW-1029797214_dev.7z")
    set(QTC_MD5 "a65b60036f1f777d7564ae6eef41e67a")
    set(QTC_DEV_MD5 "453ca164956623209e3889fd0bdc565a")
elseif(MSVC) # Windows MSVC
    set(QTC_FILENAME "qtcreator-Windows-MSVC-1029797214.7z")
    set(QTC_DEV_FILENAME "qtcreator-Windows-MSVC-1029797214_dev.7z")
    set(QTC_MD5 "55a3c8e6f3ad6c665e8219a4df1d702e")
    set(QTC_DEV_MD5 "fdaa3f29ea9929e4f923b264586da2ee")
elseif(APPLE) # macOS
    set(QTC_FILENAME "qtcreator-macOS-1029797214.7z")
    set(QTC_DEV_FILENAME "qtcreator-macOS-1029797214_dev.7z")
    set(QTC_MD5 "47b92ed4daab6cc336806d0a758f1dc4")
    set(QTC_DEV_MD5 "bb59dd9810c9919b742fc495a72038e3")
elseif(UNIX) # Linux
    set(QTC_FILENAME "qtcreator-Linux-1029797214.7z")
    set(QTC_DEV_FILENAME "qtcreator-Linux-1029797214_dev.7z")
    set(QTC_MD5 "06327245c64266f4de6f4fbe9ceb5af9")
    set(QTC_DEV_MD5 "0a3aa1ef573a28dacd83b14d0f22596f")
endif()

set(QTC_URL "${BASE_URL}/v${QTC_VER}/${QTC_FILENAME}")
set(QTC_DEV_URL "${BASE_URL}/v${QTC_VER}/${QTC_DEV_FILENAME}")

FetchContent_Declare(
  qtcreator
  URL "${QTC_URL}"
  URL_HASH MD5=${QTC_MD5}
  DOWNLOAD_NO_EXTRACT TRUE
  DOWNLOAD_NAME "qtc.7z"
)

FetchContent_GetProperties(qtcreator)
if(NOT qtcreator_POPULATED)
  FetchContent_Populate(qtcreator)

  # NOTE: We need to extract the content by hand to preserve the directory structure
  file(ARCHIVE_EXTRACT INPUT "${qtcreator_SOURCE_DIR}/qtc.7z" DESTINATION "${qtcreator_BINARY_DIR}")

  # Add dev files into the same directory
  file(DOWNLOAD "${QTC_DEV_URL}" "${qtcreator_SOURCE_DIR}/qtc_dev.7z" EXPECTED_MD5 ${QTC_DEV_MD5})
  file(ARCHIVE_EXTRACT INPUT "${qtcreator_SOURCE_DIR}/qtc_dev.7z" DESTINATION "${qtcreator_BINARY_DIR}")

  # Make CMake's find_package find the QtCreator files where they are stored
  if (APPLE)
    list(APPEND CMAKE_PREFIX_PATH "${qtcreator_BINARY_DIR}/Qt Creator.app/Contents/Resources")
  endif()

  list(APPEND CMAKE_PREFIX_PATH "${qtcreator_BINARY_DIR}")
endif()
