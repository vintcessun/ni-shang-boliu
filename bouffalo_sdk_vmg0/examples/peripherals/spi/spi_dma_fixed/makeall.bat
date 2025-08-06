@echo off
REM 编译并烧录slave模式(COM12) 
make && make flash COMX=COM12

REM 编译并烧录master模式(COM11)
set SPI_MASTER_MODE=1
make && make flash COMX=COM11