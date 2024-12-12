# Header-only C++ implementation of thread pool.
# Adaptation of Python concurrent.futures.ThreadPoolExecutor.
# SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
# SPDX-License-Identifier: MIT

# Project CMake file.

cmake_minimum_required(VERSION 3.29)
project(thread_pool)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/example)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/test)