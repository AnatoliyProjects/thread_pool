# Header-only C++ implementation of thread pool.
# Adaptation of Python concurrent.futures.ThreadPoolExecutor.
# SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
# SPDX-License-Identifier: MIT

# Gtest CMake file.

cmake_minimum_required(VERSION 3.29)
project(thread_pool_gtest)

add_executable(thread_pool_gtest ${CMAKE_CURRENT_LIST_DIR}/thread_pool_test.cpp)
target_compile_features(thread_pool_gtest PUBLIC cxx_std_20)
target_include_directories(thread_pool_gtest PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../include)

enable_testing()

if(NOT DEFINED GTEST_DIR)
    set(GTEST_DIR "${CMAKE_CURRENT_LIST_DIR}/../../googletest")
    message(WARNING "-D GTEST_DIR is not set.\nDefault: '${GTEST_DIR}'")
endif()

if (NOT EXISTS ${GTEST_DIR})
    set(MSG "gtest is not found at: '${GTEST_DIR}'.\n"
            "Use -D GTEST_DIR=... to specify the appropriate dir.")
    message(FATAL_ERROR ${MSG})
endif()

set(BUILD_GMOCK OFF)
set(INSTALL_GTEST OFF)
add_subdirectory(${GTEST_DIR} ${CMAKE_CURRENT_BINARY_DIR}/cmake-build-gtest)
target_link_libraries(thread_pool_gtest PUBLIC GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(thread_pool_gtest)
