ADD_DEFINITIONS("-std=gnu99")
ADD_DEFINITIONS("-g")
add_library(mget SHARED metadata.c fileutils.c)
target_link_libraries(mget m)
