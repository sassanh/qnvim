# SPDX-FileCopyrightText: 2022 Mikhail Zolotukhin <mail@gikari.com>
# SPDX-License-Identifier: MIT

if ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Linux")
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
