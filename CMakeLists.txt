cmake_minimum_required(VERSION 3.16)

project(
  yllama-runner
  LANGUAGES CXX
)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" YLLAMA_RELEASE_VERSION
  LIMIT_COUNT 1)
if(NOT YLLAMA_RELEASE_VERSION MATCHES "^[0-9][0-9]\\.[0-9][0-9]\\.[0-9][0-9]\\.[0-9][0-9](-Release)?$")
  message(FATAL_ERROR "VERSION must use YY.MM.DD.NN or YY.MM.DD.NN-Release")
endif()

include(GNUInstallDirs)

find_package(Threads REQUIRED)

option(YLLAMA_BUILD_TESTS "Build yllama-runner tests" ON)
option(YLLAMA_ENABLE_LLAMA_SMOKE_TEST "Enable opt-in llama.cpp backend smoke test" OFF)
option(YLLAMA_ENABLE_LLAMA_LOGS "Keep llama.cpp diagnostics on stderr" OFF)
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

set(YLLAMA_LLAMA_CPP_REVISION "0821c5fcfd729af70037bc1e9e60769d42c081ba" CACHE STRING "Pinned llama.cpp commit")
if(YLLAMA_LLAMA_CPP_INCLUDE_DIR)
  set(YLLAMA_RELEASE_ARTIFACT OFF)
  if(NOT EXISTS "${YLLAMA_LLAMA_CPP_INCLUDE_DIR}/llama.h" OR NOT YLLAMA_LLAMA_CPP_LIBRARIES)
    message(FATAL_ERROR "External llama.cpp requires valid include dir and libraries")
  endif()
  add_library(llama_cpp_external INTERFACE)
  target_include_directories(llama_cpp_external INTERFACE "${YLLAMA_LLAMA_CPP_INCLUDE_DIR}" ${YLLAMA_LLAMA_CPP_EXTRA_INCLUDE_DIRS})
  target_link_libraries(llama_cpp_external INTERFACE ${YLLAMA_LLAMA_CPP_LIBRARIES})
else()
  set(YLLAMA_RELEASE_ARTIFACT ON)
  include(FetchContent)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static release dependencies" FORCE)
  set(LLAMA_BUILD_COMMON OFF CACHE BOOL "" FORCE)
  set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
  set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(llama_cpp GIT_REPOSITORY https://github.com/ggml-org/llama.cpp.git GIT_TAG ${YLLAMA_LLAMA_CPP_REVISION} GIT_SHALLOW FALSE)
  FetchContent_MakeAvailable(llama_cpp)
  add_library(llama_cpp_external ALIAS llama)
endif()

add_library(yllama_core
  src/backend.cpp
  src/frame.cpp
  src/llama_backend.cpp
  src/runner.cpp
  src/stream_filter.cpp
)

target_include_directories(yllama_core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(yllama_core PUBLIC cxx_std_17)
target_link_libraries(yllama_core PRIVATE llama_cpp_external Threads::Threads)

if(YLLAMA_ENABLE_LLAMA_LOGS)
  target_compile_definitions(yllama_core PRIVATE YLLAMA_ENABLE_LLAMA_LOGS=1)
endif()

add_executable(yllama-runner
  src/main.cpp
)

target_link_libraries(yllama-runner PRIVATE yllama_core)
target_compile_definitions(yllama-runner PRIVATE
  YLLAMA_RELEASE_VERSION="${YLLAMA_RELEASE_VERSION}"
  YLLAMA_LLAMA_CPP_REVISION="${YLLAMA_LLAMA_CPP_REVISION}"
  YLLAMA_COMPUTE_BACKENDS="CPU$<$<BOOL:${GGML_METAL}>:,Metal>$<$<BOOL:${GGML_ACCELERATE}>:,Accelerate>"
  YLLAMA_TARGET_OS="${CMAKE_SYSTEM_NAME}"
  YLLAMA_TARGET_ARCH="${CMAKE_SYSTEM_PROCESSOR}"
)

if(MSVC)
  target_compile_options(yllama_core PRIVATE /W4)
  target_compile_options(yllama-runner PRIVATE /W4)
else()
  target_compile_options(yllama_core PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(yllama-runner PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS yllama-runner
  RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}"
)

if(APPLE)
  set_target_properties(yllama-runner PROPERTIES INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
elseif(UNIX)
  set_target_properties(yllama-runner PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()

if(YLLAMA_BUILD_TESTS)
  enable_testing()

  add_test(
    NAME yllama-runner-version
    COMMAND yllama-runner --version
  )

  add_test(NAME yllama-runner-build-info COMMAND yllama-runner --build-info)
  set_tests_properties(yllama-runner-build-info PROPERTIES
    PASS_REGULAR_EXPRESSION "llama.cpp-revision: ${YLLAMA_LLAMA_CPP_REVISION}")

  find_program(PYTHON3_FOR_CONTRACT_TESTS python3)
  if(NOT PYTHON3_FOR_CONTRACT_TESTS)
    message(FATAL_ERROR "Protocol contract tests require python3")
  endif()
  add_test(NAME release-version-contract
    COMMAND "${PYTHON3_FOR_CONTRACT_TESTS}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/release_version_test.py"
      "$<TARGET_FILE:yllama-runner>" "${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME qualification-manifest-contract
    COMMAND "${PYTHON3_FOR_CONTRACT_TESTS}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/qualification_manifest_test.py"
      "${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME qualification-artifact-contract
    COMMAND "${PYTHON3_FOR_CONTRACT_TESTS}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/qualification_artifact_test.py"
      "${CMAKE_CURRENT_SOURCE_DIR}")

  set_tests_properties(yllama-runner-version PROPERTIES
    PASS_REGULAR_EXPRESSION "yllama-runner ${YLLAMA_RELEASE_VERSION}"
  )

  add_executable(frame_test
    tests/frame_test.cpp
  )

  target_link_libraries(frame_test PRIVATE yllama_core)

  if(MSVC)
    target_compile_options(frame_test PRIVATE /W4)
  else()
    target_compile_options(frame_test PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  add_test(
    NAME frame-io
    COMMAND frame_test
  )

  add_executable(backend_test
    tests/backend_test.cpp
  )

  target_link_libraries(backend_test PRIVATE yllama_core)

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

  target_link_libraries(runner_test PRIVATE yllama_core)

  if(MSVC)
    target_compile_options(runner_test PRIVATE /W4)
  else()
    target_compile_options(runner_test PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  add_test(
    NAME runner-loop
    COMMAND runner_test
  )

  add_executable(runner_subprocess_fixture tests/runner_subprocess_fixture.cpp)
  target_link_libraries(runner_subprocess_fixture PRIVATE yllama_core)
  add_test(NAME protocol-subprocess
    COMMAND "${PYTHON3_FOR_CONTRACT_TESTS}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/protocol_subprocess_test.py"
      "$<TARGET_FILE:runner_subprocess_fixture>")
  set_tests_properties(protocol-subprocess PROPERTIES TIMEOUT 30)

  add_executable(stream_filter_test tests/stream_filter_test.cpp)
  target_link_libraries(stream_filter_test PRIVATE yllama_core)
  if(MSVC)
    target_compile_options(frame_test PRIVATE /UNDEBUG)
    target_compile_options(backend_test PRIVATE /UNDEBUG)
    target_compile_options(runner_test PRIVATE /UNDEBUG)
    target_compile_options(stream_filter_test PRIVATE /UNDEBUG)
  else()
    target_compile_options(frame_test PRIVATE -UNDEBUG)
    target_compile_options(backend_test PRIVATE -UNDEBUG)
    target_compile_options(runner_test PRIVATE -UNDEBUG)
    target_compile_options(stream_filter_test PRIVATE -UNDEBUG)
  endif()
  add_test(NAME utf8-stop-filter COMMAND stream_filter_test)

  configure_file(tests/install_relocation_test.sh.in
    "${CMAKE_CURRENT_BINARY_DIR}/install_relocation_test.sh" @ONLY)
  add_test(NAME install-relocation
    COMMAND sh "${CMAKE_CURRENT_BINARY_DIR}/install_relocation_test.sh"
            "$<TARGET_FILE:yllama-runner>")

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
    find_program(PYTHON3_EXECUTABLE python3)
    if(NOT PYTHON3_EXECUTABLE)
      message(FATAL_ERROR
        "YLLAMA_ENABLE_LLAMA_SMOKE_TEST requires python3")
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
    set_tests_properties(llama-backend-smoke PROPERTIES TIMEOUT 30)
    add_test(NAME install-relocation-smoke
      COMMAND "${SH_EXECUTABLE}"
              "${CMAKE_CURRENT_BINARY_DIR}/install_relocation_test.sh"
              "$<TARGET_FILE:yllama-runner>"
              "${YLLAMA_LLAMA_SMOKE_MODEL_PATH}")
    set_tests_properties(install-relocation-smoke PROPERTIES TIMEOUT 30)

    if(YLLAMA_LLAMA_SMOKE_LIBRARY_PATH)
      set_tests_properties(llama-backend-smoke PROPERTIES
        ENVIRONMENT
          "DYLD_LIBRARY_PATH=${YLLAMA_LLAMA_SMOKE_LIBRARY_PATH};LD_LIBRARY_PATH=${YLLAMA_LLAMA_SMOKE_LIBRARY_PATH}"
      )
    endif()
  endif()
endif()
