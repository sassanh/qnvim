cmake_minimum_required(VERSION 3.22)

if (POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

# Allow FetchContent dependencies that declare old CMake minimum versions (e.g. msgpackc)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

project(QNVim)

option(FETCH_QTC "Download Qt Creator development files automatically" ON)
option(QNVIM_BUILD_TESTS "Build the GUI mode smoke test (tests/gui)" OFF)
set(QNVIM_TEST_NVIM "nvim" CACHE STRING "Neovim used by the GUI smoke test")

add_subdirectory(external)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options("/permissive-")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
find_package(Qt6TaskTree QUIET)
if (NOT TARGET Qt6::TaskTree)
  # CI Qt 6.8 lacks QtTaskTree (added in 6.11). Provide full headers from Qt 6.11 for compilation.
  # The library is header-only for our usage (kitaspect.h only needs types); no linking required.
  file(COPY ${CMAKE_SOURCE_DIR}/external/dummy_tasktree/QtTaskTree DESTINATION ${CMAKE_BINARY_DIR}/dummy_tasktree)
  add_library(Qt6::TaskTree INTERFACE IMPORTED)
  target_include_directories(Qt6::TaskTree INTERFACE ${CMAKE_BINARY_DIR}/dummy_tasktree)
endif()
find_package(QtCreator REQUIRED COMPONENTS Core)

if (APPLE)
  # On macOS FindCups/FindZLIB report <SDK>/usr/include as the package include directory.
  # Qt links Cups::Cups (through Qt6::PrintSupport), so it reaches every Qt Widgets consumer
  # as a -isystem flag. CMake normally omits directories the compiler searches implicitly,
  # but a build directory reused after switching Xcode keeps the previous SDK in
  # CUPS_INCLUDE_DIR/ZLIB_INCLUDE_DIR; that path is not implicit to the active compiler and
  # then comes before libc++'s headers, so <cmath> includes the C <math.h> and fails to find
  # libc++'s <math.h>. Strip SDK include directories from those imported targets. A fresh
  # configure (or deleting the build directory) re-finds the packages and avoids the mismatch;
  # cached CUPS_LIBRARIES/ZLIB_LIBRARY_RELEASE may still point into the old SDK until then.
  # The imported targets are directory-scoped, so this block must stay at top level, after
  # find_package(QtCreator) and before add_subdirectory(src).
  foreach(_pkg_target IN ITEMS Cups::Cups ZLIB::ZLIB)
    if (TARGET ${_pkg_target})
      get_target_property(_pkg_dirs ${_pkg_target} INTERFACE_INCLUDE_DIRECTORIES)
      if (_pkg_dirs)
        foreach(_pkg_dir IN LISTS _pkg_dirs)
          if (_pkg_dir MATCHES "/MacOSX[^/]*\\.sdk/usr/include/?$")
            list(REMOVE_ITEM _pkg_dirs "${_pkg_dir}")
          endif()
        endforeach()
        set_target_properties(${_pkg_target} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${_pkg_dirs}")
      endif()
      unset(_pkg_dirs)
    endif()
  endforeach()
  unset(_pkg_target)
  unset(_pkg_dir)
endif()

add_subdirectory(src)

if (QNVIM_BUILD_TESTS)
  add_subdirectory(tests/gui)
endif()
