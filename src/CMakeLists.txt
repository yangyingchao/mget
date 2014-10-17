ADD_DEFINITIONS("-std=gnu99")
ADD_DEFINITIONS("-g")
add_subdirectory(lib)
include_directories(lib)
add_executable(mget-bin main.c)

set_target_properties(mget-bin
  PROPERTIES OUTPUT_NAME mget)

if(APPLE)
target_link_libraries(mget-bin mget)
else(APPLE)
target_link_libraries(mget-bin mget rt)
endif(APPLE)

install(
  TARGETS mget-bin
  RUNTIME
  DESTINATION "bin")

