# SPDX-FileCopyrightText: 2022 Mikhail Zolotukhin <mail@gikari.com>
# SPDX-License-Identifier: MIT

include(FetchContent)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.28")
  # EXCLUDE_FROM_ALL prevents neovimqt from being installed in CI artifacts
  FetchContent_Declare(
    neovimqt
    GIT_REPOSITORY https://github.com/gikari/neovim-qt.git
    GIT_TAG c84f81712639140f910a1d38acb8edcb4509dc4d # qt6 branch
    EXCLUDE_FROM_ALL
  )
  FetchContent_MakeAvailable(neovimqt)
else()
  FetchContent_Declare(
    neovimqt
    GIT_REPOSITORY https://github.com/gikari/neovim-qt.git
    GIT_TAG c84f81712639140f910a1d38acb8edcb4509dc4d # qt6 branch
  )
  FetchContent_GetProperties(neovimqt)
  if(NOT neovimqt_POPULATED)
    cmake_policy(SET CMP0169 OLD)
    FetchContent_Populate(neovimqt)
    # Use add_subdirectory with EXCLUDE_FROM_ALL so neovimqt is not installed in CI artifacts
    add_subdirectory(${neovimqt_SOURCE_DIR} ${neovimqt_BINARY_DIR} EXCLUDE_FROM_ALL)
  endif()
endif()

# Qt Creator sets QT_NO_FOREACH which disables the Qt foreach macro, but
# neovim-qt's msgpackiodevice.h uses it in a template. Patch it to use a
# range-based for loop instead.
set(_msgpackio "${neovimqt_SOURCE_DIR}/src/msgpackiodevice.h")
if(EXISTS "${_msgpackio}")
  file(READ "${_msgpackio}" _content)
  if(_content MATCHES "foreach\\(const T& elem, list\\)")
    string(REPLACE
      "foreach(const T& elem, list) {"
      "for (const auto& elem : list) {"
      _content "${_content}")
    file(WRITE "${_msgpackio}" "${_content}")
  endif()
endif()
