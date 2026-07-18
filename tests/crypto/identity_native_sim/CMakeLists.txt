cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(altruist_identity_native_sim)

target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../app/include
)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../app/src/crypto/identity.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../app/src/crypto/monocypher/monocypher.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../app/src/crypto/monocypher/monocypher-ed25519.c
  src/main.c
)
