cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(altruist_zephyr_bootstrap)

if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/include)
  zephyr_include_directories(include)
endif()

target_sources(app PRIVATE src/main.c)

if(CONFIG_ALTRUIST_WIFI_MANAGER)
  target_sources(app PRIVATE
    src/net/wifi_manager.c
    src/provisioning/provisioning.c
  )

  if(CONFIG_ALTRUIST_WIFI_CREDENTIALS_SECURE_STORAGE)
    target_sources(app PRIVATE src/config/wifi_credentials_storage.c)
  endif()
endif()
