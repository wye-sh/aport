################################################################################
# Author:    Alexander Hållenius                                               #
# Copyright: 2025                                                              #
# File:      /test/CMakeLists.txt                                              #
################################################################################

################################################################################
##  project  ###################################################################
################################################################################

cmake_minimum_required(VERSION 3.27)
project(TEST VERSION 1.0.0 LANGUAGES CXX C)

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.8.0)
FetchContent_MakeAvailable(Catch2)

set_source_files_properties(tests.x PROPERTIES LANGUAGE CXX)
add_executable(test tests.x)
target_compile_features(test PRIVATE cxx_std_23)
target_compile_options(test PRIVATE -g)
target_link_libraries(test PRIVATE
  aport
  Catch2::Catch2WithMain)
