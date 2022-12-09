# SPDX-FileCopyrightText: 2022 Mikhail Zolotukhin <mail@gikari.com>
# SPDX-License-Identifier: MIT

include(FetchContent)

FetchContent_Declare(
  neovimqt
  GIT_REPOSITORY https://github.com/gikari/neovim-qt.git
  GIT_TAG c84f81712639140f910a1d38acb8edcb4509dc4d # qt6 branch
)

FetchContent_GetProperties(neovimqt)
if(NOT neovimqt_POPULATED)
  FetchContent_Populate(neovimqt)

  # We use this, instead of MakeAvailable,
  # so that neovimqt is not installed in CI artifacts
  add_subdirectory(${neovimqt_SOURCE_DIR} ${neovimqt_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()
