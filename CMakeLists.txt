if (DEFINED __INIT_LOBARO_BUILD)
    # cmake configuration used in lobaro's internal build configuration
    project(lobaro-coap)

    include(${CPATH}/src/github.com/lobaro/c-build/build.cmake)

    add_definitions(
            -DCOAP_MAX_RETRANSMIT=2 # need +1 timeout values
            -DCOAP_EXPLICIT_TIMEOUT0=20
            -DCOAP_EXPLICIT_TIMEOUT1=20
            -DCOAP_EXPLICIT_TIMEOUT2=20
    )

    cbuild_module("lobaro/lobaro-coap/test")

    # cbuild_dependency(_ github.com/lobaro/cpp-utils)
else()
    # general cmake configuration used outside of lobaro
    cmake_minimum_required(VERSION 3.1)

    project(lobaro_coap C)

    file(GLOB_RECURSE SOURCE_FILES ${CMAKE_CURRENT_LIST_DIR}/src/*.h ${CMAKE_CURRENT_LIST_DIR}/src/*.c)
    add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES} src/liblobaro_coap.c)
    target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_LIST_DIR}/src)

    set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 99)
endif()
