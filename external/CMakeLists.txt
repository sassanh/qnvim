add_subdirectory(neovim-qt)

if (FETCH_QTC)
    add_subdirectory(qtcreator)
endif()

set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
