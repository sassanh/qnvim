set(runner_os "$ENV{RUNNER_OS}")

if ("${runner_os}" STREQUAL "Linux")
  execute_process(
    COMMAND sudo apt update
  )
  execute_process(
    COMMAND sudo apt install libgl1-mesa-dev
    RESULT_VARIABLE result
  )
  if (NOT result EQUAL 0)
    message(FATAL_ERROR "Failed to install dependencies")
  endif()
endif()
