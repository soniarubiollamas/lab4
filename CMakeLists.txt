cmake_minimum_required(VERSION 3.0)

project(smallpt_thread_pool)

add_executable(smallpt_thread_pool smallpt_thread_pool.cpp)
target_include_directories(smallpt_thread_pool
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
