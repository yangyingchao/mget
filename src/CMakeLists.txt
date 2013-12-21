ADD_DEFINITIONS("-std=gnu99")
ADD_DEFINITIONS("-g")
add_subdirectory(lib)
include_directories(lib)
add_executable(tmget main.c)
target_link_libraries(tmget mget)
install(
  TARGETS tmget
  RUNTIME
  DESTINATION "bin")

# uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

