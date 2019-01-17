# - Check for the presence of AIO
#
# The following variables are set when AIO is found:
#  HAVE_AIO       = Set to true, if all components of AIO
#                          have been found.
#  AIO_INCLUDES   = Include path for the header files of AIO
#  AIO_LIBRARIES  = Link these to use AIO

## -----------------------------------------------------------------------------
## Check for the header files

find_path (AIO_INCLUDES libaio.h
  PATHS /usr/local/include /usr/include ${CMAKE_EXTRA_INCLUDES}
  )

## -----------------------------------------------------------------------------
## Check for the library

find_library (AIO_LIBRARIES aio
  PATHS /usr/local/lib64 /usr/lib64 /lib64 ${CMAKE_EXTRA_LIBRARIES}
  )

## -----------------------------------------------------------------------------
## Actions taken when all components have been found

if (AIO_INCLUDES AND AIO_LIBRARIES)
  set (HAVE_AIO TRUE)
else (AIO_INCLUDES AND AIO_LIBRARIES)
  if (NOT AIO_FIND_QUIETLY)
    if (NOT AIO_INCLUDES)
      message (STATUS "Unable to find AIO header files!")
    endif (NOT AIO_INCLUDES)
    if (NOT AIO_LIBRARIES)
      message (STATUS "Unable to find AIO library files!")
    endif (NOT AIO_LIBRARIES)
  endif (NOT AIO_FIND_QUIETLY)
endif (AIO_INCLUDES AND AIO_LIBRARIES)

if (HAVE_AIO)
  if (NOT AIO_FIND_QUIETLY)
    message (STATUS "Found components for AIO")
    message (STATUS "AIO_INCLUDES = ${AIO_INCLUDES}")
    message (STATUS "AIO_LIBRARIES = ${AIO_LIBRARIES}")
  endif (NOT AIO_FIND_QUIETLY)
else (HAVE_AIO)
  if (AIO_FIND_REQUIRED)
    message (FATAL_ERROR "Could not find AIO!")
  endif (AIO_FIND_REQUIRED)
endif (HAVE_AIO)

mark_as_advanced (
  HAVE_AIO
  AIO_LIBRARIES
  AIO_INCLUDES
  )
  