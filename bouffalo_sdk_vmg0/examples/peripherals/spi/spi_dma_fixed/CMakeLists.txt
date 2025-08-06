cmake_minimum_required(VERSION 3.15)

include(proj.conf)

find_package(bouffalo_sdk REQUIRED HINTS $ENV{BL_SDK_BASE})

target_sources(app PRIVATE
                   ./spi/src/main.c
                   )

sdk_add_include_directories(./spi/include)

if(DEFINED ENV{SPI_MASTER_MODE})
    sdk_add_include_directories(./master)
else()
    sdk_add_include_directories(./slave)
endif()

sdk_set_main_file(main.c)

project(spi_dma)
