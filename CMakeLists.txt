cmake_minimum_required(VERSION 3.1)

project(lobaro_coap)


file(GLOB_RECURSE SOURCE_FILES ${CMAKE_CURRENT_LIST_DIR}/src/*.h ${CMAKE_CURRENT_LIST_DIR}/src/*.c)
add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES} src/liblobaro_coap.c)
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_LIST_DIR}/src)

set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 99)