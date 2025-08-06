cmake_minimum_required(VERSION 3.15)
include(proj.conf)
find_package(bouffalo_sdk REQUIRED HINTS $ENV{BL_SDK_BASE})

# User
sdk_add_compile_definitions(-DCONFIG_CLI_CMD_ENABLE)
sdk_add_compile_definitions(-DBL_VERIFY)
#sdk_add_compile_definitions(-DBL616_DHCP_DEBUG)

file(GLOB AUTO_CONNECT_SOURCES ./modules/auto_connect/src/*.c)
file(GLOB HTTPS_SOURCES ./modules/https/src/*.c)
file(GLOB CLI_CMDS_SOURCES ./modules/cli_cmds/src/*.c)
file(GLOB COMMON_UTILS_SOURCES ./modules/common/src/*.c)
file(GLOB REQUESTS_SOURCES ./modules/requests/src/*.c) 
file(GLOB OPENAPI_SOURCES ./modules/openapi/src/*.c)

target_sources(
    app PRIVATE
    ${AUTO_CONNECT_SOURCES}
    ${HTTPS_SOURCES}
    ${CLI_CMDS_SOURCES}
    ${COMMON_UTILS_SOURCES}
    ${REQUESTS_SOURCES}
    ${OPENAPI_SOURCES}
)

sdk_add_include_directories(
    .
    modules/auto_connect/include
    modules/https/include
    modules/cli_cmds/include
    modules/common/include
    modules/requests/include
    modules/openapi/include
)

sdk_set_main_file(./src/main.c)

project(openapi)
