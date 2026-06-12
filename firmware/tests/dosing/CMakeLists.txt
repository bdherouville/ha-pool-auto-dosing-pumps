cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(dosing_test)

target_sources(app PRIVATE
	dosing_test.c
	pump_pwm_mock.c
	../../src/dosing.c
)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
