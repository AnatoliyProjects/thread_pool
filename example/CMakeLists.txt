# Header-only C++ implementation of thread pool.
# Adaptation of Python concurrent.futures.ThreadPoolExecutor.
# SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
# SPDX-License-Identifier: MIT

# Example CMake file.

cmake_minimum_required(VERSION 3.29)
project(thread_pool_example)

add_executable(thread_pool_example ${CMAKE_CURRENT_LIST_DIR}/main.cpp)
target_compile_features(thread_pool_example PUBLIC cxx_std_20)
target_include_directories(thread_pool_example PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../include)