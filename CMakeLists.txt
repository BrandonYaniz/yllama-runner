cmake_minimum_required(VERSION 3.16)

project(
  yllama-runner
  VERSION 0.1.0
  LANGUAGES CXX
)

include(GNUInstallDirs)

option(YLLAMA_BUILD_TESTS "Build yllama-runner tests" ON)
option(YLLAMA_ENABLE_LLAMA_BACKEND "Build with llama.cpp backend support" OFF)

add_executable(yllama-runner
  src/main.cpp
)

target_compile_features(yllama-runner PRIVATE cxx_std_17)

if(MSVC)
  target_compile_options(yllama-runner PRIVATE /W4)
else()
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
endif()
