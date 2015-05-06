add_definitions(-std=gnu99 -Wall -D_GNU_SOURCE)

set(VERSION_MAJOR 1)
set(VERSION_MINOR 6)
set(VERSION_PATCH 5)

find_package(OpenSSL)
if (OPENSSL_FOUND)
  file(GLOB  SSL_SOURCES "plugin/ssl/openssl/*.c")
  include_directories(${OPENSSL_INCLUDE_DIR})
  set(SSL_LIBS ${OPENSSL_LIBRARIES})
  set(SSL_SUPPORT 1)
else ()
  find_package (GnuTLS)
  if (GNUTLS_FOUND)
    include_directories(${GNUTLS_INCLUDE_DIR})
    file(GLOB  SSL_SOURCES "plugin/ssl/gnutls/*.c")
    set(SSL_LIBS  ${GNUTLS_LIBRARIES})
    set(SSL_SUPPORT 1)
  endif (GNUTLS_FOUND)
endif ()

if (APPLE)
  set(USE_FCNTL 1)
endif (APPLE)

configure_file(mget_config.h.in mget_config.h)

file(GLOB SOURCES "*.c")
file(GLOB_RECURSE PROTOCOLS "protocols/*.c")
list(APPEND SOURCES ${PROTOCOLS})

add_custom_target(scan_protocols
  COMMAND python protocol_scanner.py)

add_library(mget SHARED ${SOURCES} ${SSL_SOURCES})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_dependencies(mget scan_protocols)

set(LIBS m)
if(APPLE)
  # Place holder..
else(APPLE)
  list(APPEND LIBS rt)
endif(APPLE)

target_link_libraries(mget ${LIBS})

install(
  TARGETS mget
  LIBRARY
  DESTINATION "lib")

file(GLOB PUB_HEADERS "*mget*.h")
install(
  FILES ${PUB_HEADERS}
  DESTINATION "include/libmget/"
  )

target_link_libraries (mget ${SSL_LIBS} "pthread")
