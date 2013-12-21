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
