ADD_DEFINITIONS("-std=gnu99")
ADD_DEFINITIONS("-g")
add_subdirectory(lib)
include_directories(lib)
add_executable(mget-bin main.c)

set_target_properties(mget-bin
  PROPERTIES OUTPUT_NAME mget)
target_link_libraries(mget-bin mget)
install(
  TARGETS mget-bin
  RUNTIME
  DESTINATION "/usr/bin")

