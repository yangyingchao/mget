project (mget)
cmake_minimum_required(VERSION 2.6)

option (DEBUG
  "Debug or release.."
  OFF)

# uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_distclean.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_distclean.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

add_custom_target(distclean
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_distclean.cmake)

if (DEBUG)
  message(STATUS "Configuring mget for debug version...")
  set(CMAKE_BUILD_TYPE "Debug")
else()
    message(STATUS "Configuring mget for release version...")
  set(CMAKE_BUILD_TYPE "Release")
endif ()
add_subdirectory(src)

