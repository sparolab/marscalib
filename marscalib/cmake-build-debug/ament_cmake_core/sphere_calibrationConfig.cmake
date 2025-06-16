# generated from ament/cmake/core/templates/nameConfig.cmake.in

# prevent multiple inclusion
if(_sphere_calibration_CONFIG_INCLUDED)
  # ensure to keep the found flag the same
  if(NOT DEFINED sphere_calibration_FOUND)
    # explicitly set it to FALSE, otherwise CMake will set it to TRUE
    set(sphere_calibration_FOUND FALSE)
  elseif(NOT sphere_calibration_FOUND)
    # use separate condition to avoid uninitialized variable warning
    set(sphere_calibration_FOUND FALSE)
  endif()
  return()
endif()
set(_sphere_calibration_CONFIG_INCLUDED TRUE)

# output package information
if(NOT sphere_calibration_FIND_QUIETLY)
  message(STATUS "Found sphere_calibration: 0.0.0 (${sphere_calibration_DIR})")
endif()

# warn when using a deprecated package
if(NOT "" STREQUAL "")
  set(_msg "Package 'sphere_calibration' is deprecated")
  # append custom deprecation text if available
  if(NOT "" STREQUAL "TRUE")
    set(_msg "${_msg} ()")
  endif()
  # optionally quiet the deprecation message
  if(NOT ${sphere_calibration_DEPRECATED_QUIET})
    message(DEPRECATION "${_msg}")
  endif()
endif()

# flag package as ament-based to distinguish it after being find_package()-ed
set(sphere_calibration_FOUND_AMENT_PACKAGE TRUE)

# include all config extra files
set(_extras "ament_cmake_export_libraries-extras.cmake")
foreach(_extra ${_extras})
  include("${sphere_calibration_DIR}/${_extra}")
endforeach()
