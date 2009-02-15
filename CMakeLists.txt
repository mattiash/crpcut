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

set(CRPCUT_LIB_MAJOR_VERSION "0")
set(CRPCUT_LIB_MINOR_VERSION "2")
set(CRPCUT_LIB_PATCH_VERSION "0")
set(CMAKE_CXX_FLAGS "-ggdb")
set(CRPCUT_LIB_VERSION "${CRPCUT_LIB_MAJOR_VERSION}.${CRPCUT_LIB_MINOR_VERSION}.${CRPCUT_LIB_PATCH_VERSION}")
set(CRPCUT_LIB_SOVERSION ${CRPCUT_LIB_MAJOR_VERSION})
set(SHAREDIR "share/crpcut-${CRPCUT_LIB_VERSION}")
file(GLOB LIB_SRCS
     src/clocks.cpp
     src/implementation.cpp
     src/comm.cpp
     src/policies.cpp
     src/crpcut.cpp
     src/xml.cpp
     src/posix_encapsulation.cpp
)
if (WITH_GOOGLE_MOCK)
   if (NOT GOOGLE_MOCK_DIR)
      find_library(GOOGLE_MOCK_LIB "gmock")
      if (NOT GOOGLE_MOCK_LIB)
         message(FATAL_ERROR
                 "Can't find google mock, try -DGOOGLE_MOCK_DIR=<path>")
      endif (NOT GOOGLE_MOCK_LIB)
      set(GMOCK_LIBS "-lgmock")
   else (NOT GOOGLE_MOCK_DIR)
      include_directories(${GOOGLE_MOCK_DIR}/include)
      set(GMOCK_LIBS "-Wl,-rpath,${GOOGLE_MOCK_DIR}/lib -L${GOOGLE_MOCK_DIR}/lib -lgmock")
   endif (NOT GOOGLE_MOCK_DIR)
   file(GLOB GMOCK_LIB_SRCS src/gmock.cpp)
   set(SELFTEST_PARAM ${SELFTEST_PARAM} gmock)
endif (WITH_GOOGLE_MOCK)

include_directories(include)

if (NOT CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX /usr)
endif (NOT CMAKE_INSTALL_PREFIX)

# Check for variadic macros


file(WRITE ${CMAKE_BINARY_DIR}/CMakeTmp/variadic_macro.cpp
  "#define APA(...) main(__VA_ARGS__)\nint APA(int argc, char *argv[]) {}\n")
try_compile(HAVE_VARIADIC_MACROS ${CMAKE_BINARY_DIR}
  ${CMAKE_BINARY_DIR}/CMakeTmp/variadic_macro.cpp)
if (NOT HAVE_VARIADIC_MACROS)
  message(FATAL_ERROR "${CMAKE_CXX_COMPILER} doesn't support variadic macros")
endif(NOT HAVE_VARIADIC_MACROS)

file(WRITE ${CMAKE_BINARY_DIR}/CMakeTmp/exit.cpp
  "#include <cstdlib>\nmain() { std::_Exit(0); }\n")
try_compile(HAVE__EXIT ${CMAKE_BINARY_DIR}
  ${CMAKE_BINARY_DIR}/CMakeTmp/exit.cpp)
if (NOT HAVE__EXIT)
  message(FATAL_ERROR "${CMAKE_CXX_COMPILER} doesn't support std::_Exit()")
endif(NOT HAVE__EXIT)

# Check for decltype

file(WRITE ${CMAKE_BINARY_DIR}/CMakeTmp/decltype.cpp
  "template <typename T> T* func(T);\n"
  "typedef decltype(func(3.14)) apa;\n"
  "int main() {}\n")

try_compile(HAVE_DECLTYPE ${CMAKE_BINARY_DIR}
  ${CMAKE_BINARY_DIR}/CMakeTmp/decltype.cpp)
if(NOT HAVE_DECLTYPE)
  file(WRITE ${CMAKE_BINARY_DIR}/CMakeTmp/typeof.cpp
    "template <typename T> T* func(T);\n"
    "typedef typeof(func(3.14)) apa;\n"
    "int main() {}\n")
  try_compile(HAVE_DECLTYPE ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/CMakeTmp/typeof.cpp)
  if(NOT HAVE_DECLTYPE)
    message(
      FATAL_ERROR
      "${CMAKE_CXX_COMPILER} doesn't support decltype or typeof")
  else(NOT HAVE_DECLTYPE)
    set(USING_TYPEOF 1)
    add_definitions(-Ddecltype=typeof)
  endif(NOT HAVE_DECLTYPE)
endif(NOT HAVE_DECLTYPE)

# check for TR1

file(WRITE ${CMAKE_BINARY_DIR}/CMakeTmp/tr1.cpp
  "#include <tr1/type_traits>\n"
  "const bool v = std::tr1::is_reference<int>::type::value;\n"
  "int main() { return v; }\n")
try_compile(HAVE_TR1 ${CMAKE_BINARY_DIR}
  ${CMAKE_BINARY_DIR}/CMakeTmp/tr1.cpp)
if(NOT HAVE_TR1)
  message(FATAL_ERROR "${CMAKE_CXX_COMPILER} doesn't have std/tr1")
endif(NOT HAVE_TR1)
# check for clock_gettime()

set(EXTRA_LIBS "${EXTRA_LIBS} -ldl")
check_function_exists("clock_gettime()" HAVE_CLOCK_GETTIME)
if(HAVE_CLOCK_GETTIME)
  add_definitions(-DHAVE_CLOCK_GETTIME=libc)
else(HAVE_CLOCK_GETTIME)
  check_library_exists("rt" "clock_gettime" "" RT_HAS_CLOCK_GETTIME)
  if(RT_HAS_CLOCK_GETTIME)
    set(EXTRA_LIBS "${EXTRA_LIBS} -lrt")
    set(HAVE_CLOCK_GETTIME 1)
    add_definitions(-DHAVE_CLOCK_GETTIME=librt)
  endif(RT_HAS_CLOCK_GETTIME)
endif(HAVE_CLOCK_GETTIME)
if(HAVE_CLOCK_GETTIME)
  set(MAY_HAVE_MONO 1)
  set(MAY_HAVE_CPU 1)
  set(HAVE_CLOCK 1)
endif(HAVE_CLOCK_GETTIME)

# check for mach_absolute_time()

check_function_exists("mach_absolute_time" HAVE_MACH_ABSOLUTE_TIME)
if(HAVE_MACH_ABSOLUTE_TIME)
  set(MAY_HAVE_MONO 1)
  set(HAVE_CLOCK 1)
  add_definitions(-DHAVE_MACH_ABSOLUTE_TIME)
endif(HAVE_MACH_ABSOLUTE_TIME)


# check for getitimer()

check_function_exists("getitimer" HAVE_ITIMER)
if(HAVE_ITIMER)
  add_definitions(-DHAVE_ITIMER)
  set(MAY_HAVE_CPU 1)
  set(HAVE_CLOCK 1)
endif(HAVE_ITIMER)

# check for gettimeofday()

check_function_exists("gettimeofday" HAVE_GETTIMEOFDAY)
if(HAVE_GETTIMEOFDAY)
  add_definitions(-DHAVE_GETTIMEOFDAY)
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
  add_definitions(-DHAVE_EPOLL)
endif(HAVE_EPOLL)


include_directories(include)

add_library(crpcut SHARED ${LIB_SRCS})
set_target_properties(crpcut PROPERTIES
 VERSION ${CRPCUT_LIB_VERSION}
 SOVERSION ${CRPCUT_LIB_SOVERSION}
 LINK_FLAGS "${EXTRA_LIBS}")

if(GMOCK_LIBS)
  add_library(crpcut_gmock SHARED ${GMOCK_LIB_SRCS})
  set_target_properties(crpcut_gmock PROPERTIES
    VERSION ${CRPCUT_LIB_VERSION}
    SOVERSION ${CRPCUT_LIB_SOVERSION}
    LINK_FLAGS "${GMOCK_LIBS}")
  file(GLOB GMOCK_TEST_SRCS   test-src/gmock.cpp)
endif(GMOCK_LIBS)

file(GLOB TEST_SRCS
     test-src/asserts_and_depends.cpp
     test-src/output.cpp
     test-src/timeouts.cpp
     test-src/death_by_exit.cpp
     test-src/death_by_signal.cpp
     test-src/death_by_exception.cpp
     test-src/parametrized.cpp
     test-src/unitt.cpp
     test-src/wrapped.cpp
     ${GMOCK_TEST_SRCS}
)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FEATURE_SELECTION}")
add_executable(testprog ${TEST_SRCS})
target_link_libraries(testprog crpcut)
if (GMOCK_LIBS)
  target_link_libraries(testprog crpcut_gmock "${GMOCK_LIBS}")
endif (GMOCK_LIBS)

find_program(XSLTPROC "xsltproc")
if(XSLTPROC)
  file(GLOB DOC_SOURCES  "doc-src/*.xml")
  file(GLOB XSL_TEMPLATE "doc-src/doc2html.xsl")
  add_custom_target(
    html bash -c "test -d html || mkdir html"
    VERBATIM)
  add_custom_target(
    doc.html ALL
    ${XSLTPROC} ${XSL_TEMPLATE} ${DOC_SOURCES} > doc.html
    DEPENDS html ${XSL_TEMPLATE} ${DOC_SOURCES}
    WORKING_DIRECTORY html)
else(XSLTPROC)
  message("xsltproc is not found - Can't build documentation")
endif(XSLTPROC)

find_program(RUBY "ruby")
if(RUBY)
  add_custom_target(selftest ${RUBY} ${CMAKE_SOURCE_DIR}/selftest.rb ${SELFTEST_PARAM} VERBATIM)
else(RUBY)
  message("ruby is not found - selftest target not available")
endif(RUBY)
find_program(AWK "awk")
find_program(BASH "bash")
find_program(XMLLINT "xmllint")
if (AWK AND BASH AND XMLLINT)
   add_custom_target(sanitycheck ${BASH} ${CMAKE_SOURCE_DIR}/sanitycheck.sh ${AWK} ${CMAKE_SOURCE_DIR} ${SELFTEST_PARAM} VERBATIM)
endif (AWK AND BASH AND XMLLINT)
set(LIBRARY_OUTPUT_PATH lib)
set(EXECUTABLE_OUTPUT_PATH test)

if(USING_TYPEOF)
  message("*** IMPORTANT *** Compile your tests using -Ddecltype=typeof")
endif(USING_TYPEOF)

install(
  FILES include/crpcut.hpp
  DESTINATION include
)

install(TARGETS crpcut DESTINATION lib)

if(GMOCK_LIBS)
  install(TARGETS crpcut_gmock DESTINATION lib)
endif(GMOCK_LIBS)
install(
  FILES ${TEST_SRCS}
  DESTINATION "${SHAREDIR}/example_source"
)
install(
  FILES crpcut.xsd FAQ TODO
  DESTINATION "${SHAREDIR}"
  )
if(XSLTPROC)
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}//html/doc.html
    DESTINATION "${SHAREDIR}/html"
    )
endif(XSLTPROC)
