cmake_minimum_required(VERSION 2.8)


find_library(dyn_array_lib dyn_array)

# Locate GTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})


#set(CMAKE_CXX_FLAGS "-std=c++0x -Wall -Wextra -Wshadow -Werror -g")
set(CMAKE_C_FLAGS "-std=c99 -Wall -Wextra -Wshadow -Werror -g")

# Link runTests with what we want to test and the GTest and pthread library
add_executable(project_test test/tests.cpp)
add_executable(analysis src/analysis.c src/process_scheduling.c)
target_link_libraries(analysis ${dyn_array_lib} pthread)
target_link_libraries(project_test ${dyn_array_lib}
${GTEST_LIBRARIES} pthread)

enable_testing()
add_test(NAME    project_test 
         COMMAND project_test)


