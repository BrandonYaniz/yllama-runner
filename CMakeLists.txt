cmake_minimum_required(VERSION 3.16)

project(
  yllama-runner
  VERSION 2026.5.31
  LANGUAGES CXX
)

include(GNUInstallDirs)

find_package(Threads REQUIRED)

option(YLLAMA_BUILD_TESTS "Build yllama-runner tests" ON)
option(YLLAMA_ENABLE_LLAMA_SMOKE_TEST "Enable opt-in llama.cpp backend smoke test" OFF)
option(YLLAMA_ENABLE_LLAMA_LOGS "Keep llama.cpp diagnostics on stderr" OFF)
set(YLLAMA_RELEASE_VERSION
    "2026.05.31-beta.1"
    CACHE STRING "Public yllama-runner release version")
set(YLLAMA_LLAMA_CPP_INCLUDE_DIR
    ""
    CACHE PATH "Path to llama.cpp public headers")
set(YLLAMA_LLAMA_CPP_EXTRA_INCLUDE_DIRS
    ""
    CACHE STRING "Semicolon-separated extra include paths required by llama.cpp")
set(YLLAMA_LLAMA_CPP_LIBRARIES
    ""
    CACHE STRING "Semicolon-separated llama.cpp library paths or linker names")
set(YLLAMA_LLAMA_SMOKE_MODEL_PATH
    ""
    CACHE FILEPATH "GGUF model path for the opt-in llama.cpp smoke test")
set(YLLAMA_LLAMA_SMOKE_LIBRARY_PATH
    ""
    CACHE PATH "Runtime library path for the opt-in llama.cpp smoke test")

if(NOT YLLAMA_LLAMA_CPP_INCLUDE_DIR)
  message(FATAL_ERROR
    "YLLAMA_LLAMA_CPP_INCLUDE_DIR is required")
endif()

if(NOT EXISTS "${YLLAMA_LLAMA_CPP_INCLUDE_DIR}/llama.h")
  message(FATAL_ERROR
    "YLLAMA_LLAMA_CPP_INCLUDE_DIR must contain llama.h")
endif()

if(NOT YLLAMA_LLAMA_CPP_LIBRARIES)
  message(FATAL_ERROR
    "YLLAMA_LLAMA_CPP_LIBRARIES is required")
endif()

add_library(llama_cpp_external INTERFACE)
target_include_directories(llama_cpp_external INTERFACE
  "${YLLAMA_LLAMA_CPP_INCLUDE_DIR}"
  ${YLLAMA_LLAMA_CPP_EXTRA_INCLUDE_DIRS}
)
target_link_libraries(llama_cpp_external INTERFACE
  ${YLLAMA_LLAMA_CPP_LIBRARIES}
)

add_library(yllama_protocol
  src/backend.cpp
  src/jsonl.cpp
  src/llama_backend.cpp
  src/prompt.cpp
  src/protocol.cpp
  src/runner.cpp
)

target_include_directories(yllama_protocol PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(yllama_protocol PUBLIC cxx_std_17)
target_link_libraries(yllama_protocol PRIVATE llama_cpp_external Threads::Threads)

if(YLLAMA_ENABLE_LLAMA_LOGS)
  target_compile_definitions(yllama_protocol PRIVATE YLLAMA_ENABLE_LLAMA_LOGS=1)
endif()

add_executable(yllama-runner
  src/main.cpp
)

target_link_libraries(yllama-runner PRIVATE yllama_protocol)
target_compile_definitions(yllama-runner PRIVATE
  YLLAMA_RELEASE_VERSION="${YLLAMA_RELEASE_VERSION}"
)

if(MSVC)
  target_compile_options(yllama_protocol PRIVATE /W4)
  target_compile_options(yllama-runner PRIVATE /W4)
else()
  target_compile_options(yllama_protocol PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(yllama-runner PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS yllama-runner
  RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}"
)

if(YLLAMA_BUILD_TESTS)
  enable_testing()

  add_test(
    NAME yllama-runner-version
    COMMAND yllama-runner --version
  )

  set_tests_properties(yllama-runner-version PROPERTIES
    PASS_REGULAR_EXPRESSION "yllama-runner ${YLLAMA_RELEASE_VERSION}"
  )

  add_executable(protocol_test
    tests/protocol_test.cpp
  )

  target_link_libraries(protocol_test PRIVATE yllama_protocol)

  if(MSVC)
    target_compile_options(protocol_test PRIVATE /W4)
  else()
    target_compile_options(protocol_test PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  add_test(
    NAME protocol-events
    COMMAND protocol_test
  )

  add_executable(jsonl_test
    tests/jsonl_test.cpp
  )

  target_link_libraries(jsonl_test PRIVATE yllama_protocol)

  if(MSVC)
    target_compile_options(jsonl_test PRIVATE /W4)
  else()
    target_compile_options(jsonl_test PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  add_test(
    NAME jsonl-commands
    COMMAND jsonl_test
  )

  add_executable(prompt_test
    tests/prompt_test.cpp
  )

  target_link_libraries(prompt_test PRIVATE yllama_protocol)

  if(MSVC)
    target_compile_options(prompt_test PRIVATE /W4)
  else()
    target_compile_options(prompt_test PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  add_test(
    NAME prompt-rendering
    COMMAND prompt_test
  )

  add_executable(backend_test
    tests/backend_test.cpp
  )

  target_link_libraries(backend_test PRIVATE yllama_protocol)

  if(MSVC)
    target_compile_options(backend_test PRIVATE /W4)
  else()
    target_compile_options(backend_test PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  add_test(
    NAME backend-interface
    COMMAND backend_test
  )

  add_executable(runner_test
    tests/runner_test.cpp
  )

  target_link_libraries(runner_test PRIVATE yllama_protocol)

  if(MSVC)
    target_compile_options(runner_test PRIVATE /W4)
  else()
    target_compile_options(runner_test PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  add_test(
    NAME runner-loop
    COMMAND runner_test
  )

  if(YLLAMA_ENABLE_LLAMA_SMOKE_TEST)
    if(NOT YLLAMA_LLAMA_SMOKE_MODEL_PATH)
      message(FATAL_ERROR
        "YLLAMA_ENABLE_LLAMA_SMOKE_TEST requires YLLAMA_LLAMA_SMOKE_MODEL_PATH")
    endif()

    if(NOT EXISTS "${YLLAMA_LLAMA_SMOKE_MODEL_PATH}")
      message(FATAL_ERROR
        "YLLAMA_LLAMA_SMOKE_MODEL_PATH must point to an existing GGUF model")
    endif()

    find_program(SH_EXECUTABLE sh)
    if(NOT SH_EXECUTABLE)
      message(FATAL_ERROR
        "YLLAMA_ENABLE_LLAMA_SMOKE_TEST requires a POSIX sh executable")
    endif()
    configure_file(
      tests/llama_smoke_test.sh.in
      "${CMAKE_CURRENT_BINARY_DIR}/llama_smoke_test.sh"
      @ONLY
    )

    add_test(
      NAME llama-backend-smoke
      COMMAND "${SH_EXECUTABLE}"
              "${CMAKE_CURRENT_BINARY_DIR}/llama_smoke_test.sh"
              "$<TARGET_FILE:yllama-runner>"
              "${YLLAMA_LLAMA_SMOKE_MODEL_PATH}"
    )

    if(YLLAMA_LLAMA_SMOKE_LIBRARY_PATH)
      set_tests_properties(llama-backend-smoke PROPERTIES
        ENVIRONMENT
          "DYLD_LIBRARY_PATH=${YLLAMA_LLAMA_SMOKE_LIBRARY_PATH};LD_LIBRARY_PATH=${YLLAMA_LLAMA_SMOKE_LIBRARY_PATH}"
      )
    endif()
  endif()
endif()
