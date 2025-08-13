cmake_minimum_required(VERSION 3.15)

include(proj.conf)

find_package(bouffalo_sdk REQUIRED HINTS $ENV{BL_SDK_BASE})

target_sources(app PRIVATE
                   ./i2c/src/i2c.c
                   )

sdk_add_include_directories(./i2c/include)

sdk_set_main_file(main.c)

project(pwm_config_channel)
