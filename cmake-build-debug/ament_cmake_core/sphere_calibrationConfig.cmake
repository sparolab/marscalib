# generated from ament/cmake/core/templates/nameConfig.cmake.in

# prevent multiple inclusion
if(_marscalib_CONFIG_INCLUDED)
  # ensure to keep the found flag the same
  if(NOT DEFINED marscalib_FOUND)
    # explicitly set it to FALSE, otherwise CMake will set it to TRUE
    set(marscalib_FOUND FALSE)
  elseif(NOT marscalib_FOUND)
    # use separate condition to avoid uninitialized variable warning
    set(marscalib_FOUND FALSE)
  endif()
  return()
endif()
set(_marscalib_CONFIG_INCLUDED TRUE)

# output package information
if(NOT marscalib_FIND_QUIETLY)
  message(STATUS "Found marscalib: 0.0.0 (${marscalib_DIR})")
endif()

# warn when using a deprecated package
if(NOT "" STREQUAL "")
  set(_msg "Package 'marscalib' is deprecated")
  # append custom deprecation text if available
  if(NOT "" STREQUAL "TRUE")
    set(_msg "${_msg} ()")
  endif()
  # optionally quiet the deprecation message
  if(NOT ${marscalib_DEPRECATED_QUIET})
    message(DEPRECATION "${_msg}")
  endif()
endif()

# flag package as ament-based to distinguish it after being find_package()-ed
set(marscalib_FOUND_AMENT_PACKAGE TRUE)

# include all config extra files
set(_extras "ament_cmake_export_libraries-extras.cmake")
foreach(_extra ${_extras})
  include("${marscalib_DIR}/${_extra}")
endforeach()
