# cmake configuration used for Zephyr
cmake_minimum_required(VERSION 3.1)

if (CONFIG_LOBARO_COAP)

    find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

    file(GLOB_RECURSE SOURCE_FILES ${CMAKE_CURRENT_LIST_DIR}/../src/*.h ${CMAKE_CURRENT_LIST_DIR}/../src/*.c)

    zephyr_include_directories(${CMAKE_CURRENT_LIST_DIR}/../src)

    zephyr_library_named(lobaro_coap)

    zephyr_library_sources(${SOURCE_FILES})

endif()
