#! /bin/bash

cmake -D CMAKE_C_COMPILER=clang         \
      -D CMAKE_CXX_COMPILER=clang++     \
      -D APORT_RADIX_MODE=OFF           \
      -G "Ninja"                        \
      -B build                          \
 && cd build                            \
 && ninja -k 1                          \
 && cd ..                               \
 && ./bin/test.exe                      \
 && cmake -D CMAKE_C_COMPILER=clang     \
	  -D CMAKE_CXX_COMPILER=clang++ \
	  -D APORT_RADIX_MODE=ON        \
	  -G "Ninja"                    \
	  -B build                      \
 && cd build                            \
 && ninja -k 1                          \
 && cd ..                               \
 && ./bin/test.exe