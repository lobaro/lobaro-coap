cmake_minimum_required(VERSION 3.6)
project(lobaro_coap)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

set(SOURCE_FILES
    interface/debug/coap_debug.c
    interface/debug/coap_debug.h
    interface/mem/coap_mem.c
    interface/mem/coap_mem.h
    interface/network/net_Endpoint.c
    interface/network/net_Endpoint.h
    interface/network/net_Packet.c
    interface/network/net_Packet.h
    interface/network/net_Socket.c
    interface/network/net_Socket.h
    interface/coap_interface.h
    option-types/coap_option_blockwise.c
    option-types/coap_option_blockwise.h
    option-types/coap_option_cf.c
    option-types/coap_option_cf.h
    option-types/coap_option_ETag.c
    option-types/coap_option_ETag.h
    option-types/coap_option_observe.c
    option-types/coap_option_observe.h
    option-types/coap_option_uri.c
    option-types/coap_option_uri.h
    coap.h
    coap_interaction.c
    coap_interaction.h
    coap_main.c
    coap_main.h
    coap_message.c
    coap_message.h
    coap_options.c
    coap_options.h
    coap_resource.c
    coap_resource.h liblobaro_coap.h liblobaro_coap.c)

add_library(lobaro_coap ${SOURCE_FILES})
set_property(TARGET lobaro_coap PROPERTY C_STANDARD 99)