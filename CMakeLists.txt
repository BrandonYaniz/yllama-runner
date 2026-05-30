cmake_minimum_required(VERSION 3.16)

project(
  yllama-runner
  VERSION 0.1.0
  LANGUAGES CXX
)

include(GNUInstallDirs)

option(YLLAMA_BUILD_TESTS "Build yllama-runner tests" ON)
option(YLLAMA_ENABLE_LLAMA_BACKEND "Build with llama.cpp backend support" OFF)
set(YLLAMA_LLAMA_CPP_INCLUDE_DIR
    ""
    CACHE PATH "Path to llama.cpp public headers")
set(YLLAMA_LLAMA_CPP_LIBRARIES
    ""
    CACHE STRING "Semicolon-separated llama.cpp library paths or linker names")

if(YLLAMA_ENABLE_LLAMA_BACKEND)
  if(NOT YLLAMA_LLAMA_CPP_INCLUDE_DIR)
    message(FATAL_ERROR
      "YLLAMA_ENABLE_LLAMA_BACKEND requires YLLAMA_LLAMA_CPP_INCLUDE_DIR")
  endif()

  if(NOT EXISTS "${YLLAMA_LLAMA_CPP_INCLUDE_DIR}/llama.h")
    message(FATAL_ERROR
      "YLLAMA_LLAMA_CPP_INCLUDE_DIR must contain llama.h")
  endif()

  if(NOT YLLAMA_LLAMA_CPP_LIBRARIES)
    message(FATAL_ERROR
      "YLLAMA_ENABLE_LLAMA_BACKEND requires YLLAMA_LLAMA_CPP_LIBRARIES")
  endif()

  add_library(llama_cpp_external INTERFACE)
  target_include_directories(llama_cpp_external INTERFACE
    "${YLLAMA_LLAMA_CPP_INCLUDE_DIR}"
  )
  target_link_libraries(llama_cpp_external INTERFACE
    ${YLLAMA_LLAMA_CPP_LIBRARIES}
  )
endif()

add_library(yllama_protocol
  src/backend.cpp
  src/jsonl.cpp
  src/prompt.cpp
  src/protocol.cpp
  src/runner.cpp
)

target_include_directories(yllama_protocol PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(yllama_protocol PUBLIC cxx_std_17)

if(YLLAMA_ENABLE_LLAMA_BACKEND)
  target_compile_definitions(yllama_protocol PRIVATE YLLAMA_ENABLE_LLAMA_BACKEND=1)
  target_link_libraries(yllama_protocol PRIVATE llama_cpp_external)
endif()

add_executable(yllama-runner
  src/main.cpp
)

target_link_libraries(yllama-runner PRIVATE yllama_protocol)

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
    PASS_REGULAR_EXPRESSION "yllama-runner 0.1.0"
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
endif()
