ADD_DEFINITIONS("-std=gnu99")
ADD_DEFINITIONS("-g")

set(VERSION_MAJOR 0)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)

configure_file(mget_config.h.in mget_config.h)
add_library(mget SHARED libmget.c metadata.c fileutils.c cmget_http.c
  timeutil.c  data_utlis.c)
target_link_libraries(mget m curl)

install(
  TARGETS mget
  LIBRARY
  DESTINATION "/usr/lib")

#TODO: install header files....

# - Try to find LibXml2
# Once done this will define
#  LIBXML2_FOUND - System has LibXml2
#  LIBXML2_INCLUDE_DIRS - The LibXml2 include directories
#  LIBXML2_LIBRARIES - The libraries needed to use LibXml2
#  LIBXML2_DEFINITIONS - Compiler switches required for using LibXml2

# find_package(PkgConfig)
# pkg_check_modules(PC_LIBXML QUIET libxml-2.0)
# set(LIBXML2_DEFINITIONS ${PC_LIBXML_CFLAGS_OTHER})

# find_path(LIBXML2_INCLUDE_DIR libxml/xpath.h
#           HINTS ${PC_LIBXML_INCLUDEDIR} ${PC_LIBXML_INCLUDE_DIRS}
#           PATH_SUFFIXES libxml2 )

# find_library(LIBXML2_LIBRARY NAMES xml2 libxml2
#              HINTS ${PC_LIBXML_LIBDIR} ${PC_LIBXML_LIBRARY_DIRS} )

# set(LIBXML2_LIBRARIES ${LIBXML2_LIBRARY} )
# set(LIBXML2_INCLUDE_DIRS ${LIBXML2_INCLUDE_DIR} )

# include(FindPackageHandleStandardArgs)
# # handle the QUIETLY and REQUIRED arguments and set LIBXML2_FOUND to TRUE
# # if all listed variables are TRUE
# find_package_handle_standard_args(LibXml2  DEFAULT_MSG
#                                   LIBXML2_LIBRARY LIBXML2_INCLUDE_DIR)

# mark_as_advanced(LIBXML2_INCLUDE_DIR LIBXML2_LIBRARY )

add_executable(ctest ctest.c)
target_link_libraries(ctest curl)
