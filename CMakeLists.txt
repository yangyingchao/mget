project (mget)
cmake_minimum_required(VERSION 2.6)

option (DLOG
  "Enable debug..."
  ON)

option (MSOCK
  "Try use raw sock instead of libcurl.."
  ON)


if(DLOG)
ADD_DEFINITIONS("-DDEBUG")
endif(DLOG)

if(MSOCK)
  set(TRY_MSOCK 1)
endif(MSOCK)


add_subdirectory(src)

