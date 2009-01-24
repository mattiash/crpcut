#  Copyright 2009 Bjorn Fahller <bjorn@fahller.se>
#  All rights reserved
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.

#  THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
#  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
#  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
#  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
#  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
#  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
#  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
#  SUCH DAMAGE.



cmake_minimum_required(VERSION 2.4)
if (COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)
project(crpcut)
include(CheckFunctionExists)
include(CheckLibraryExists)
aux_source_directory(src LIB_SRCS)
if (NOT CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX /usr)
endif (NOT CMAKE_INSTALL_PREFIX)

# Check for variadic macros


file(WRITE ${CMAKE_BINARY_DIR}/CMakeTmp/variadic_macro.cpp
  "#define APA(...) main(__VA_ARGS__)\nint APA(int argc, char *argv[]) {}")
try_compile(HAVE_VARIADIC_MACROS ${CMAKE_BINARY_DIR}
  ${CMAKE_BINARY_DIR}/CMakeTmp/variadic_macro.cpp)
if (NOT HAVE_VARIADIC_MACROS)
  message(FATAL_ERROR "Your compiler doesn't support variadic macros")
endif(NOT HAVE_VARIADIC_MACROS)

# Check for decltype

file(WRITE ${CMAKE_BINARY_DIR}/CMakeTmp/decltype.cpp
  "template <typename T> T* func(T); typedef decltype(func(3.14)) apa; int main() {} \n")
try_compile(HAVE_DECLTYPE ${CMAKE_BINARY_DIR}
  ${CMAKE_BINARY_DIR}/CMakeTmp/decltype.cpp)
if(NOT HAVE_DECLTYPE)
  file(WRITE ${CMAKE_BINARY_DIR}/CMakeTmp/typeof.cpp
    "template <typename T> T* func(T); typedef typeof(func(3.14)) apa; int main() {}\n")
  try_compile(HAVE_DECLTYPE ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/CMakeTmp/typeof.cpp)
  if(NOT HAVE_DECLTYPE)
    message(FATAL_ERROR "Your compiler doesn't support decltype or typeof")
  else(NOT HAVE_DECLTYPE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ddecltype=typeof")
    message("*** IMPORTANT *** Compile your tests using -Ddecltype=typeof")
  endif(NOT HAVE_DECLTYPE)
endif(NOT HAVE_DECLTYPE)

# check for TR1

file(WRITE ${CMAKE_BINARY_DIR}/CMakeTmp/tr1.cpp
  "#include <tr1/type_traits>\nconst bool v = std::tr1::is_reference<int>::type::value; int main() { return v; }\n")
try_compile(HAVE_TR1 ${CMAKE_BINARY_DIR}
  ${CMAKE_BINARY_DIR}/CMakeTmp/tr1.cpp)
if(NOT HAVE_TR1)
  message(FATAL_ERROR "Your compiler doesn't have std/tr1")
endif(NOT HAVE_TR1)
# check for clock_gettime()

check_function_exists("clock_gettime()" HAVE_CLOCK_GETTIME)
if(NOT HAVE_CLOCK_GETTIME)
  check_library_exists("rt" "clock_gettime" "" RT_HAS_CLOCK_GETTIME)
  if(RT_HAS_CLOCK_GETTIME)
    set(EXTRA_LIBS ${EXTRA_LIBS} -lrt)
    set(HAVE_CLOCK_GETTIME 1)
  endif(RT_HAS_CLOCK_GETTIME)
endif(NOT HAVE_CLOCK_GETTIME)
if(HAVE_CLOCK_GETTIME)
  set(MAY_HAVE_MONO 1)
  set(MAY_HAVE_CPU 1)
  set(HAVE_CLOCK 1)
  set(FEATURE_SELECTION "${FEATURE_SELECTION} -DHAVE_CLOCK_GETTIME")
endif(HAVE_CLOCK_GETTIME)

# check for mach_absolute_time()

check_function_exists("mach_absolute_time" HAVE_MACH_ABSOLUTE_TIME)
if(HAVE_MACH_ABSOLUTE_TIME)
  set(MAY_HAVE_MONO 1)
  set(HAVE_CLOCK 1)
  set(FEATURE_SELECTION "${FEATURE_SELECTION} -DHAVE_MACH_ABSOLUTE_TIME")
endif(HAVE_MACH_ABSOLUTE_TIME)


# check for getitimer()

check_function_exists("getitimer" HAVE_ITIMER)
if(HAVE_ITIMER)
  set(FEATURE_SELECTION "${FEATURE_SELECTION} -DHAVE_ITIMER")
  set(MAY_HAVE_CPU 1)
  set(HAVE_CLOCK 1)
endif(HAVE_ITIMER)

# check for gettimeofday()

check_function_exists("gettimeofday" HAVE_GETTIMEOFDAY)
if(HAVE_GETTIMEOFDAY)
  set(FEATURE_SELECTION "${FEATURE_SELECTION} -DHAVE_GETTIMEOFDAY")
  set(HAVE_CLOCK 1)
endif(HAVE_GETTIMEOFDAY)


if (NOT HAVE_CLOCK)
  message(FATAL_ERROR "Velly velly solly, but you have no clocks!")
endif (NOT HAVE_CLOCK)
if (NOT MAY_HAVE_CPU)
  message(FATAL_ERROR "Velly velly solly, but I can't measure CPU-time")
endif (NOT MAY_HAVE_CPU)
if (NOT MAY_HAVE_MONO)
  message("Monotonic time source unavailable - using realtime clocks!")
endif (NOT MAY_HAVE_MONO)

# check for epoll_*

check_function_exists("epoll_ctl" HAVE_EPOLL)
if(HAVE_EPOLL)
  set(FEATURE_SELECTION "${FEATURE_SELECTION} -DHAVE_EPOLL")
endif(HAVE_EPOLL)


include_directories(include)
add_library(crpcut STATIC ${LIB_SRCS})
set(TEST_SRCS test-src/unitt.cpp)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FEATURE_SELECTION}")
add_executable(testprog ${TEST_SRCS})
target_link_libraries(testprog crpcut ${EXTRA_LIBS})


find_program(XSLTPROC "xsltproc")
if(XSLTPROC)
  file(GLOB DOC_SOURCES  "doc-src/*.xml")
  file(GLOB XSL_TEMPLATE "doc-src/doc2html.xsl")
  add_custom_target(
    doc.html ALL
    COMMAND ${XSLTPROC} ${XSL_TEMPLATE} ${DOC_SOURCES} > doc.html
    DEPENDS ${XSL_TEMPLATE} ${DOC_SOURCES}
    WORKING_DIRECTORY html)
else(XSLTPROC)
  message("xsltproc is not found - Can't build documentation")
endif(XSLTPROC)


set(LIBRARY_OUTPUT_PATH lib)
set(EXECUTABLE_OUTPUT_PATH test)

install(
  FILES include/crpcut.hpp
  DESTINATION include
)

install(
  FILES lib/libcrpcut.a
  DESTINATION lib
)

if(XSLTPROC)
  install(
    FILES html/doc.html
    DESTINATION share/crpcut
    )
endif(XSLTPROC)
