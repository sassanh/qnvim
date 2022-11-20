include(FetchContent)

find_package(Git QUIET)

file(GLOB NEOVIM_QT_PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/patches/*.patch")

FetchContent_Declare(
  neovimqt
  GIT_REPOSITORY https://github.com/equalsraf/neovim-qt.git
  GIT_TAG 3f05de82ecb5c1a24a7572081ae59e419eb059b8 # 0.2.17
  PATCH_COMMAND ${GIT_EXECUTABLE} restore . && ${GIT_EXECUTABLE} apply ${NEOVIM_QT_PATCHES}
)

FetchContent_GetProperties(neovimqt)
if(NOT neovimqt_POPULATED)
  FetchContent_Populate(neovimqt)

  set(ENABLE_TESTS OFF CACHE INTERNAL "Turn off tests")

  # We use this, instead of MakeAvailable,
  # so that neovimqt is not installed in CI artifacts
  add_subdirectory(${neovimqt_SOURCE_DIR} ${neovimqt_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()
