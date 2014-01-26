ADD_DEFINITIONS("-std=gnu99")
ADD_DEFINITIONS("-g")

set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)
set(VERSION_PATCH 1)

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

file(GLOB SOURCES "*.c")
add_library(mget SHARED ${SOURCES})

target_link_libraries(mget m)
install(
  TARGETS mget
  LIBRARY
  DESTINATION "/usr/lib")

#TODO: install header files....

if (GNUTLS_FOUND)
  include_directories(${GNUTLS_INCLUDE_DIR})
  target_link_libraries (mget ${GNUTLS_LIBRARIES})
endif (GNUTLS_FOUND)
