cmake_minimum_required(VERSION 3.15)

include(proj.conf)

find_package(bouffalo_sdk REQUIRED HINTS $ENV{BL_SDK_BASE})

sdk_add_compile_definitions(-DCONFIG_CLI_CMD_ENABLE)

sdk_add_include_directories(./hall/include)

target_sources(app PRIVATE
                   ./hall/src/main.c
                   )

sdk_add_include_directories(./http/include)

target_sources(app PRIVATE
                   ./http/src/main.c
                   ./http/src/auto_connect.c
                   )

sdk_add_include_directories(./lcd/include)

target_sources(app PRIVATE
                   ./lcd/src/main.c
                   )

sdk_add_include_directories(./rtos/include)

target_sources(app PRIVATE
                   ./rtos/src/main.c
                   )


sdk_add_include_directories(./audio/include)

target_sources(app PRIVATE
                   ./audio/src/main.c
                   )

sdk_add_include_directories(./audio_es8388/include)

target_sources(app PRIVATE
                   ./audio_es8388/src/es8388_driver.c
                   )

sdk_add_include_directories(./websocket/include)

target_sources(app PRIVATE
                   ./websocket/src/main.c
                   ./websocket/src/base64_encode.c
                   ./websocket/src/base64_decode.c
                   ./websocket/src/sha1.c
                   )

sdk_set_main_file(main.c)

project(nishang)
