cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(OPENTHREAD_SERVER)



target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/openthread_events)
target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/utils)
# target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/device_history)



target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/openthread_events/openthread_events.c)
# target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/device_history/device_history.c)


