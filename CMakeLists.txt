project (mget)
cmake_minimum_required(VERSION 2.6)

option (DLOG
  "Enable debug..."
  ON)

option (MSOCK
  "Try use raw sock instead of libcurl.."
  ON)


if(DLOG)
ADD_DEFINITIONS("-DDEBUG")
endif(DLOG)

if(MSOCK)
  set(TRY_MSOCK 1)
endif(MSOCK)


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


add_subdirectory(src)

