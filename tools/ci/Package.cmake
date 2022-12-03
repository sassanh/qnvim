set(QTC_EXT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../external/qtcreator")
set(QTC_DIR "${QTC_EXT_DIR}/dist-${CMAKE_HOST_SYSTEM_NAME}-$ENV{QT_CREATOR_VERSION}")

set(build_plugin_py "scripts/build_plugin.py")
foreach(dir "share/qtcreator/scripts" "Qt Creator.app/Contents/Resources/scripts" "Contents/Resources/scripts")
  if(EXISTS "${QTC_DIR}/${dir}/build_plugin.py")
    set(build_plugin_py "${dir}/build_plugin.py")
    break()
  endif()
endforeach()

execute_process(
  COMMAND python
    -u
    "${QTC_DIR}/${build_plugin_py}"
    --name "$ENV{PLUGIN_NAME}-$ENV{QT_CREATOR_VERSION}-$ENV{ARTIFACT_SUFFIX}"
    --src .
    --build build
    --qt-path "$ENV{QT_DIR}"
    --qtc-path "${QTC_DIR}"
    --output-path "$ENV{GITHUB_WORKSPACE}"
  RESULT_VARIABLE result
)
if (NOT result EQUAL 0)
  string(REGEX MATCH "FAILED:.*$" error_message "${output}")
  string(REPLACE "\n" "%0A" error_message "${error_message}")
  message("::error::${error_message}")
  message(FATAL_ERROR "Build failed")
endif()
