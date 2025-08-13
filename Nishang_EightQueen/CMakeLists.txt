cmake_minimum_required(VERSION 3.15)

include(proj.conf)

find_package(bouffalo_sdk REQUIRED HINTS $ENV{BL_SDK_BASE})

# User
sdk_add_compile_definitions(-DCONFIG_CLI_CMD_ENABLE)
#sdk_add_compile_definitions(-DBL616_DHCP_DEBUG)

target_sources(app PRIVATE
                   ./http/src/auto_connect.c
                   ./http/src/main.c
                   )

sdk_add_include_directories(./http/include)

target_sources(app PRIVATE
                   ./rtos/src/aligned_malloc.c
                   )

sdk_add_include_directories(./rtos/include)


target_sources(app PRIVATE
                   ./game/src/main.c
                   )

sdk_add_include_directories(./game/include)



target_sources(app PRIVATE
                   ./ir/src/main.c
                   ./ir/src/i2c.c
                   )

sdk_add_include_directories(./ir/include)


sdk_add_include_directories(./audio/include)

target_sources(app PRIVATE
                   ./audio/src/main.c
                   )

sdk_add_include_directories(./audio_es8388/include)

target_sources(app PRIVATE
                   ./audio_es8388/src/es8388_driver.c
                   )

target_sources(app PRIVATE
                   ./websocket/src/main.c
                   ./websocket/src/base64_encode.c
                   ./websocket/src/base64_decode.c
                   ./websocket/src/sha1.c
                   )

sdk_add_include_directories(./websocket/include)


target_sources(app PRIVATE
                   ./stt/src/main.c
                   )

sdk_add_include_directories(./stt/include)

sdk_set_main_file(main.c)

project(nishang)
