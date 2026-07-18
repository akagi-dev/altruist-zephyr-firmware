cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(altruist_wifi_manager_native_sim)

zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../../app/include)

target_sources(app PRIVATE
	src/main.c
	../../../../app/src/net/wifi_manager.c
	../../../../app/src/provisioning/provisioning.c
)
