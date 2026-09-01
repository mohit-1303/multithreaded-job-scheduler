cmake_minimum_required(VERSION 3.10)
project(TaskFlow)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
include_directories(include)

add_executable(taskflow
    src/main.cpp
    src/Job.cpp
    src/Scheduler.cpp
)
target_link_libraries(taskflow PRIVATE Threads::Threads)

add_executable(taskflow_tests
    tests/test_job.cpp
    src/Job.cpp
)
target_link_libraries(taskflow_tests PRIVATE Threads::Threads)
