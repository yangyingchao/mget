ADD_DEFINITIONS("-std=gnu99")
ADD_DEFINITIONS("-g")

set(VERSION_MAJOR 0)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)

configure_file(mget_config.h.in mget_config.h)
add_library(mget SHARED libmget.c metadata.c fileutils.c
  http.c connection.c cmget_http.c
  timeutil.c  data_utlis.c netutils.c)
target_link_libraries(mget m curl)

install(
  TARGETS mget
  LIBRARY
  DESTINATION "/usr/lib")

#TODO: install header files....

add_executable(ctest ctest.c)
target_link_libraries(ctest curl mget)
