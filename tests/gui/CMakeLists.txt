# GUI mode smoke test: neovim-qt's Shell + qnvim's Neovim arguments and Lua
# glue, without Qt Creator. Enable with -DQNVIM_BUILD_TESTS=ON.
add_executable(qnvim_gui_smoke
  shell_smoke.cpp
  ${PROJECT_SOURCE_DIR}/src/gui/nvimguiargs.cpp
)
target_include_directories(qnvim_gui_smoke PRIVATE ${PROJECT_SOURCE_DIR}/src/gui)
target_link_libraries(qnvim_gui_smoke PRIVATE Qt6::Widgets neovim-qt neovim-qt-gui)

set(_shim "${QNVIM_NEOVIMQT_SOURCE_DIR}/src/gui/runtime/plugin/nvim_gui_shim.vim")
add_custom_target(qnvim_gui_smoke_run
  COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen
          $<TARGET_FILE:qnvim_gui_smoke>
          ${PROJECT_SOURCE_DIR}/src/gui/qnvim_gui.lua
          ${_shim}
          ${CMAKE_CURRENT_SOURCE_DIR}/smoke.cpp
          ${CMAKE_CURRENT_BINARY_DIR}/smoke.png
          ${QNVIM_TEST_NVIM}
  DEPENDS qnvim_gui_smoke
  COMMENT "Running the GUI mode smoke test (screenshot: ${CMAKE_CURRENT_BINARY_DIR}/smoke.png)"
  VERBATIM)
