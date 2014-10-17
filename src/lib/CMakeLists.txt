ADD_DEFINITIONS("-std=gnu99")
ADD_DEFINITIONS("-g")

set(VERSION_MAJOR 1)
set(VERSION_MINOR 1)
set(VERSION_PATCH 5)

find_package (GnuTLS)
if (GNUTLS_FOUND)
  set(HAVE_GNUTLS 1)
endif (GNUTLS_FOUND)

include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckLibraryExists)

configure_file(mget_config.h.in mget_config.h)

file(GLOB SOURCES "*.c")
file(GLOB_RECURSE PROTOCOLS "protocols/*.c")
list(APPEND SOURCES ${PROTOCOLS})
if(GNUTLS_FOUND)
  file(GLOB SSL_SOURCES "plugin/ssl/*.c")
  foreach(item ${SSL_SOURCES})
    list(APPEND SOURCES ${item})
  endforeach()
endif(GNUTLS_FOUND)

add_custom_target(scan_protocols
    COMMAND python protocol_scanner.py)

add_library(mget SHARED ${SOURCES})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_dependencies(mget scan_protocols)

target_link_libraries(mget m)
install(
  TARGETS mget
  LIBRARY
  DESTINATION "lib")

file(GLOB PUB_HEADERS "*mget*.h")
install(
  FILES ${PUB_HEADERS}
  DESTINATION "include/libmget/"
  )

if (GNUTLS_FOUND)
  include_directories(${GNUTLS_INCLUDE_DIR})
  target_link_libraries (mget ${GNUTLS_LIBRARIES})
else (GNUTLS_FOUND)
 target_link_libraries (mget "pthread")
endif (GNUTLS_FOUND)
