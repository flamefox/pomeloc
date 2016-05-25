cmake_minimum_required(VERSION 2.8)

project(pomeloc)

set(Pomeloc_Library_SRCS
  include/pomeloc/pomeloc.h
  include/pomeloc/idl.h
  include/pomeloc/util.h
  include/pomeloc/json.hpp
  src/idl_parser.cpp
)

set(Pomeloc_Compiler_SRCS
  ${Pomeloc_Library_SRCS}
  src/idl_gen_general.cpp
  src/pomeloc.cpp
)

if(APPLE)
  set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++ -Wall -pedantic -Werror -Wextra")
elseif(CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -std=c++0x -Wall -pedantic -Werror -Wextra -Werror=shadow -Wunused-result -Werror=unused-result")
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -std=c++0x -stdlib=libc++ -Wall -pedantic -Werror -Wextra")
  set(CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS} -lc++abi")
endif()


include_directories(include)

add_executable(pomeloc ${Pomeloc_Compiler_SRCS})
