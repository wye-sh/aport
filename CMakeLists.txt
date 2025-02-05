################################################################################
# Author:    Alexander Hållenius                                               #
# Copyright: 2025-                                                             #
# File:      /CMakeLists.txt                                                   #
################################################################################

set(BUILD_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build)
set(BINARY_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(LIBRARY_DIRECTORY ${BINARY_DIRECTORY})
set(INCLUDE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include)

################################################################################
##  project  ###################################################################
################################################################################

cmake_minimum_required(VERSION 3.27)
project(APORT VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${LIBRARY_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_DIRECTORY})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BINARY_DIRECTORY})

################################################################################
##  target  ####################################################################
################################################################################

if(${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
  # We are running from this project, meaning it is debug
  add_definitions(-DLX_DEBUG_INFO)
endif()
add_library(aport INTERFACE)
target_include_directories(aport INTERFACE ${INCLUDE_DIRECTORY})

################################################################################
##  options  ###################################################################
################################################################################

option(APORT_RADIX_MODE "When retrieving from tree, will use radix mode." OFF)
if (APORT_RADIX_MODE)
  target_compile_definitions(aport
    INTERFACE
      APORT_RADIX_MODE)
endif()

################################################################################
##  testing  ###################################################################
################################################################################

if(${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
  # We are running from this project, meaning it is debug
  add_subdirectory(test)
endif()
