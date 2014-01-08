ADD_DEFINITIONS("-std=gnu99")
ADD_DEFINITIONS("-g")

set(VERSION_MAJOR 0)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)

find_package (GnuTLS)
if (GNUTLS_FOUND)
  set(HAVE_GNUTLS 1)
endif (GNUTLS_FOUND)

include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckLibraryExists)

CHECK_INCLUDE_FILES(sys/epoll.h HAVE_EPOLL)
if(NOT HAVE_EPOLL)
  message(STATUS "epoll not found, trying to use select system call")
  CHECK_INCLUDE_FILES(sys/select.h HAVE_SELECT)
  if(NOT HAVE_SELECT)
    message(FATAL_ERROR "ERROR: requires select system call!")
  endif(NOT HAVE_SELECT)
endif(NOT HAVE_EPOLL)

configure_file(mget_config.h.in mget_config.h)
add_library(mget SHARED libmget.c metadata.c fileutils.c
  http.c connection.c  timeutil.c  data_utlis.c netutils.c gnutls.c log.c)

target_link_libraries(mget m)
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
