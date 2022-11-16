include(FetchContent)

FetchContent_Declare(
  neovimqt
  GIT_REPOSITORY https://github.com/equalsraf/neovim-qt.git
  GIT_TAG 3f05de82ecb5c1a24a7572081ae59e419eb059b8 # 0.2.17
)

set(ENABLE_TESTS OFF CACHE INTERNAL "Turn off tests")
FetchContent_MakeAvailable(neovimqt)

# FIXME: Ideally we don't need to do that.
# This should be correctly upstreamed to neovim-qt
target_include_directories(neovim-qt
  INTERFACE
    "${neovimqt_SOURCE_DIR}/src"
)
