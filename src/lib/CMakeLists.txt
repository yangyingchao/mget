ADD_DEFINITIONS("-std=gnu99")
ADD_DEFINITIONS("-g")

set(VERSION_MAJOR 0)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)

find_package (GnuTLS)
if (GNUTLS_FOUND)
  set(USE_GNUTLS 1)
endif (GNUTLS_FOUND)

configure_file(mget_config.h.in mget_config.h)
add_library(mget SHARED libmget.c metadata.c fileutils.c
  http.c connection.c cmget_http.c
  timeutil.c  data_utlis.c netutils.c gnutls.c log.c)
target_link_libraries(mget m curl)

install(
  TARGETS mget
  LIBRARY
  DESTINATION "/usr/lib")

#TODO: install header files....

add_executable(ctest ctest.c)
target_link_libraries(ctest curl mget)

if (GNUTLS_FOUND)
  include_directories(${GNUTLS_INCLUDE_DIR})
  target_link_libraries (mget ${GNUTLS_LIBRARIES})
endif (GNUTLS_FOUND)
