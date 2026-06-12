cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(piscine_pump4)

target_sources(app PRIVATE src/main.c src/pump_pwm.c src/dosing.c src/zb_pump.c)
