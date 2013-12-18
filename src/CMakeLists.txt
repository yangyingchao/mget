ADD_DEFINITIONS("-std=gnu99")
add_executable(mget main.c metadata.c fileutils.c)
target_link_libraries(mget m)

