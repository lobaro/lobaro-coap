cmake_minimum_required(VERSION 3.1)

###### Compile library ######
project(LobaroCoapLib)

file(GLOB_RECURSE LIBRARY_FILES ${CMAKE_CURRENT_LIST_DIR}/../src/*.h ${CMAKE_CURRENT_LIST_DIR}/../src/*.c)
add_library(${PROJECT_NAME} ${LIBRARY_FILES})
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_LIST_DIR}/../src)

###### Compile test executable ######
project(LobaroCoapTests)

## Prepare cmake for testing
enable_testing()
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

## Prepare gtest
set(GTEST_DIR $ENV{GTEST_DIR} CACHE PATH "")
set(GTEST_FILES ${GTEST_DIR}/src/gtest-all.cc ${GTEST_DIR}/src/gtest_main.cc)
include_directories( ${GTEST_DIR} ${GTEST_DIR}/include)
add_definitions(-DGTEST_HAS_PTHREAD=0)

## Prepare for coverage measuring
if(${COVERAGE})
	set(CMAKE_CXX_FLAGS "-g -O0 -fprofile-arcs -ftest-coverage")
endif()

# All cpp files in this directory are considered testcase files.
file(GLOB_RECURSE TESTS_FILES ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

## Create executable, set c++11 standard, link to library
add_executable(${PROJECT_NAME} ${TESTS_FILES} ${GTEST_FILES})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_range_for)
target_link_libraries(${PROJECT_NAME} LobaroCoapLib)

## We actually do not use the cmake test system (we would add each test case with add_test)
## but the gtest framework. We therefore add only "one" test case here, which internally
## performs many gtest testcases.
add_test(${PROJECT_NAME} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME})
