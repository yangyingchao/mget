add_subdirectory(lib)
include_directories(lib)

add_definitions(-std=gnu99 -Wall)

file(GLOB  SRC "*.c")
add_executable(mget-bin ${SRC})

set_target_properties(mget-bin
  PROPERTIES OUTPUT_NAME mget)

target_link_libraries(mget-bin mget)

install(
  TARGETS mget-bin
  RUNTIME
  DESTINATION "bin")

