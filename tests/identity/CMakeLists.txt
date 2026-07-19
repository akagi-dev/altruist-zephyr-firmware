cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(altruist_identity)

target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../app/include
)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/crypto/identity.c
  src/main.c
)
