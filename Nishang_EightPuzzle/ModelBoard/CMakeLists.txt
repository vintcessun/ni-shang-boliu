cmake_minimum_required(VERSION 3.15)

include(proj.conf)

find_package(bouffalo_sdk REQUIRED HINTS $ENV{BL_SDK_BASE})

sdk_add_include_directories(.)
sdk_add_include_directories(./neural/include)
target_sources(app PRIVATE ./neural/src/neural_model.c)
target_sources(app PRIVATE ./neural/src/neural_predict.cc)
sdk_set_main_file(main.c)

target_sources(app PRIVATE
                   ./spi/src/main.c
                   )

sdk_add_include_directories(./spi/include)

project(cam_g0)
