cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(altruist_zephyr_firmware)

target_include_directories(app PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

target_sources(app PRIVATE src/main.c)

if(CONFIG_ALTRUIST_CONFIG)
  target_sources(app PRIVATE
    src/config/config_defaults.c
    src/config/config_store.c
  )
endif()
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/src/crypto/CMakeLists.txt)
  add_subdirectory(src/crypto)
endif()
