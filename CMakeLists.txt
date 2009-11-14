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
set(CRPCUT_LIB_MINOR_VERSION "8")
set(CRPCUT_LIB_PATCH_VERSION "2")
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "-O")
set(CMAKE_CXX_FLAGS_MAINT "-ggdb -Wall -Wextra -Wcast-qual -Wconversion -Wshadow -Wunused-parameter -Wwrite-strings -pedantic -Wno-variadic-macros -D_GLIBCXX_DEBUG=1")
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
     src/output.cpp
     src/heap.cpp
)
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

# Check for decltype and typeof


include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-std=c++0x HAVE_CXX0X)
file(WRITE ${CMAKE_BINARY_DIR}/CMakeTmp/typeof.cpp
  "template <typename T> T* func(T);\n"
  "typedef typeof(func(3.14)) apa;\n"
  "int main() {}\n")
try_compile(HAVE_TYPEOF ${CMAKE_BINARY_DIR}
  ${CMAKE_BINARY_DIR}/CMakeTmp/typeof.cpp)
if (NOT HAVE_TYPEOF)
  if (NOT HAVE_CXX0X)
    mesage(FATAL_ERROR
      "${CMAKE_CXX_COMPILER} supports neither decltype nor typeof")
  endif (NOT HAVE_CXX0X)
endif (NOT HAVE_TYPEOF)

# check for TR1

file(WRITE ${CMAKE_BINARY_DIR}/CMakeTmp/tr1.cpp
  "#include <tr1/type_traits>\n"
  "const bool v = std::tr1::is_reference<int>::type::value;\n"
  "int main() { return v; }\n")
try_compile(HAVE_TR1 ${CMAKE_BINARY_DIR}
  ${CMAKE_BINARY_DIR}/CMakeTmp/tr1.cpp)
if(NOT HAVE_TR1)
  file(WRITE ${CMAKE_BINARY_DIR}/CMakeTmp/boost_tr1.cpp
    "#include <boost/tr1/type_traits.hpp>\n"
    "const bool v = std::tr1::is_reference<int>::type::value;\n"
    "int main() { return v; }\n")
  try_compile(HAVE_BOOST_TR1 ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/CMakeTmp/boost_tr1.cpp)
  if (NOT HAVE_BOOST_TR1)
    message(FATAL_ERROR "${CMAKE_CXX_COMPILER} doesn't have std/tr1 nor boost/tr1")
  endif (NOT HAVE_BOOST_TR1)
endif(NOT HAVE_TR1)

# check for valgrind

file(WRITE ${CMAKE_BINARY_DIR}/CMakeTmp/valgrind.cpp
  "#include <valgrind/valgrind.h>\n"
  "int main(int argc, char *argv[])\n"
  "{\n"
  "  VALGRIND_MALLOCLIKE_BLOCK(argv, 4, 0, 0);\n"
  "  return 0;\n"
  "}\n")
try_compile(HAVE_VALGRIND ${CMAKE_BINARY_DIR}
  ${CMAKE_BINARY_DIR}/CMakeTmp/valgrind.cpp)

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

# check google-mock
if (WITH_GOOGLE_MOCK)
  if (NOT GOOGLE_MOCK_DIR)
    find_library(GOOGLE_MOCK_LIB "gmock")
    if (NOT GOOGLE_MOCK_LIB)
      message(FATAL_ERROR
        "Can't find google mock, try to add -DGOOGLE_MOCK_DIR=<path> to the command line")
    endif (NOT GOOGLE_MOCK_LIB)
    set(GMOCK_LIBS "-lgmock -lgtest")
  else (NOT GOOGLE_MOCK_DIR)
    include_directories(AFTER ${GOOGLE_MOCK_DIR}/include)
    set(GMOCK_LIBS "-Wl,-rpath,${GOOGLE_MOCK_DIR}/lib -L${GOOGLE_MOCK_DIR}/lib -lgmock -lgtest")
  endif (NOT GOOGLE_MOCK_DIR)
  file(GLOB GMOCK_LIB_SRCS src/gmock.cpp)
  set(SELFTEST_PARAM ${SELFTEST_PARAM} gmock)

  file(WRITE ${CMAKE_BINARY_DIR}/CMakeTmp/gmock.cpp
    "#include <gmock/gmock.h>\nclass C : public ::testing::EmptyTestEventListener {};\nint main() {\n  C obj;\n  ::testing::UnitTest::GetInstance()->listeners().Append(&obj);\n}\n")
  try_compile(HAVE_GMOCK ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/CMakeTmp/gmock.cpp
    CMAKE_FLAGS -DINCLUDE_DIRECTORIES=${GOOGLE_MOCK_DIR}/include
    -DLINK_DIRECTORIES=${GOOGLE_MOCK_DIR}/lib
    -DLINK_LIBRARIES=gtest)
  if (NOT HAVE_GMOCK)
    message(FATAL_ERROR "Unknown version of Google Mock")
  endif (NOT HAVE_GMOCK)
endif (WITH_GOOGLE_MOCK)

include_directories(include)

add_library(crpcut SHARED ${LIB_SRCS})
set_target_properties(crpcut PROPERTIES
 VERSION ${CRPCUT_LIB_VERSION}
 SOVERSION ${CRPCUT_LIB_SOVERSION}
 LINK_FLAGS "${EXTRA_LIBS}")

if(GMOCK_LIBS)
  file(GLOB GMOCK_TEST_SRCS   test-src/gmock.cpp)
endif(GMOCK_LIBS)


file(GLOB TEST_SRCS
     test-src/unitt.cpp
     test-src/asserts_and_depends.cpp
     test-src/output.cpp
     test-src/timeouts.cpp
     test-src/death_by_exit.cpp
     test-src/death_by_signal.cpp
     test-src/death_by_exception.cpp
     test-src/parametrized.cpp
     test-src/wrapped.cpp
     test-src/ext_parameters.cpp
     test-src/predicates.cpp
     test-src/regex.cpp
     test-src/fp.cpp
     test-src/collate.cpp
     test-src/suite_deps1.cpp
     test-src/suite_deps2.cpp
     test-src/heap.cpp
     test-src/bad_forks.cpp
     ${GMOCK_TEST_SRCS}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FEATURE_SELECTION}")
add_executable(testprog EXCLUDE_FROM_ALL ${TEST_SRCS})
target_link_libraries(testprog crpcut)
if (GMOCK_LIBS)
  target_link_libraries(testprog "${GMOCK_LIBS}")
endif (GMOCK_LIBS)

add_custom_target(can_dump_core
        ulimit -c | egrep -q "\\(Unlimited\\)\\|\\[1-9\\]" && touch can_dump_core)
find_program(XSLTPROC "xsltproc")
find_program(SED "sed")
file(GLOB HTML_CHUNK_XSL
  "/usr/share/xml/docbook/stylesheet/docbook-xsl-ns/html/chunk.xsl")
if (NOT HTML_CHUNK_XSL)
  file(GLOB HTML_CHUNK_XSL
    "/usr/share/sgml/docbook/xsl-ns-stylesheets/html/chunk.xsl")
endif(NOT HTML_CHUNK_XSL)
find_program(GDB "gdb")
if (NOT XSLTPROC)
   message("xsltproc is not found - Can't build documentation")
else (NOT XSLTPROC)
  if (NOT HTML_CHUNK_XSL)
    message("docbook-xsl-ns not found, can't build docs")
  else (NOT HTML_CHUNK_XSL)
    if (NOT GDB)
      message("gdb not found, can't build docs")
    else(NOT GDB)
      if (NOT SED)
        message("sed not found, can't build docs")
      endif(NOT SED)
    endif (NOT GDB)
  endif(NOT HTML_CHUNK_XSL)
endif(NOT XSLTPROC)

if(XSLTPROC AND HTML_CHUNK_XSL AND GDB AND SED)
  file(GLOB DOC_SRC_LIB "doc-src")
  file(GLOB REFMAN_SOURCES "${DOC_SRC_LIB}/refman.docbook")
  add_custom_target(
    stripped-src
    test -d stripped-src || mkdir stripped-src
    )
  add_custom_target(
    doc-bin
    test -d doc-bin || mkdir doc-bin
    )
  add_custom_target(
    sample-output
    test -d sample-output || mkdir sample-output
    )
  add_custom_target(
    html
    test -d html || mkdir html
    )

  add_custom_target(
    refman
    test -d html/refman || mkdir html/refman
    DEPENDS html
    )

  set(STRIP_SOURCES sed "0,/^$/D\\;s/\\(.*\\)/     \\1/g")

  #
  # all_tests
  #
  file(GLOB ALL_TESTS "${DOC_SRC_LIB}/samples/all_tests.cpp")
  add_custom_target(
    all_tests.cpp
    ${STRIP_SOURCES} < ${ALL_TESTS} > all_tests.cpp
    DEPENDS stripped-src ${ALL_TESTS}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(all_tests EXCLUDE_FROM_ALL ${ALL_TESTS})
  target_link_libraries(all_tests crpcut)
  add_custom_target(
    all_tests.out
    all_tests | sed "s/\\(.*\\)/     \\1/g" > all_tests.out
    DEPENDS all_tests sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # exit_any_code
  #
  file(GLOB EXIT_ANY_CODE "${DOC_SRC_LIB}/samples/exit_any_code.cpp")
  add_custom_target(
    exit_any_code.cpp
    ${STRIP_SOURCES} < ${EXIT_ANY_CODE} > exit_any_code.cpp
    DEPENDS stripped-src ${EXIT_ANY_CODE}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(exit_any_code EXCLUDE_FROM_ALL ${EXIT_ANY_CODE})
  target_link_libraries(exit_any_code crpcut)
  add_custom_target(
    exit_any_code.out
    exit_any_code | sed "s/\\(.*\\)/     \\1/g" > exit_any_code.out
    DEPENDS exit_any_code sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # assert_eq_fails
  #
  file(GLOB ASSERT_EQ_FAILS "${DOC_SRC_LIB}/samples/assert_eq_fails.cpp")
  file(GLOB DEBUG_CMD "${DOC_SRC_LIB}/samples/debug_cmd")
  add_custom_target(
    assert_eq_fails.cpp
    ${STRIP_SOURCES} < ${ASSERT_EQ_FAILS} > assert_eq_fails.cpp
    DEPENDS stripped-src ${ASSERT_EQ_FAILS}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(assert_eq_fails EXCLUDE_FROM_ALL ${ASSERT_EQ_FAILS})
  set_target_properties(assert_eq_fails PROPERTIES COMPILE_FLAGS "-ggdb")
  target_link_libraries(assert_eq_fails crpcut)
  add_custom_target(
    assert_eq_fails.out
    assert_eq_fails | sed "s/\\(.*\\)/     \\1/g" > assert_eq_fails.out
    DEPENDS assert_eq_fails sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)
  add_custom_target(
    debug_assert_eq_fails.out
    ${GDB} ../test/assert_eq_fails  < ${DEBUG_CMD} | sed "1,1s/^.*\\(GNU.*\\)/bash > gdb .\\/test\\/assert_eq_fails\\n\\1/g;s/\\((gdb)[^(]*\\)(gdb) \\(.*\\)$/\\1\\n\\2/g;s/^\\(..\\)\\((gdb).*\\)/\\1\\n\\2/g;s/\\[answered.*//g"  > debug_assert_eq_fails.out
    WORKING_DIRECTORY sample-output
    VERBATIM)



  #
  # assert_ge_fails
  #
  file(GLOB ASSERT_GE_FAILS "${DOC_SRC_LIB}/samples/assert_ge_fails.cpp")
  add_custom_target(
    assert_ge_fails.cpp
    ${STRIP_SOURCES} < ${ASSERT_GE_FAILS} > assert_ge_fails.cpp
    DEPENDS stripped-src ${ASSERT_GE_FAILS}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(assert_ge_fails EXCLUDE_FROM_ALL ${ASSERT_GE_FAILS})
  target_link_libraries(assert_ge_fails crpcut)
  add_custom_target(
    assert_ge_fails.out
    assert_ge_fails | sed "s/\\(.*\\)/     \\1/g" > assert_ge_fails.out
    DEPENDS assert_ge_fails sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # assert_le_fails
  #
  file(GLOB ASSERT_LE_FAILS "${DOC_SRC_LIB}/samples/assert_le_fails.cpp")
  add_custom_target(
    assert_le_fails.cpp
    ${STRIP_SOURCES} < ${ASSERT_LE_FAILS} > assert_le_fails.cpp
    DEPENDS stripped-src ${ASSERT_LE_FAILS}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(assert_le_fails EXCLUDE_FROM_ALL ${ASSERT_LE_FAILS})
  target_link_libraries(assert_le_fails crpcut)
  add_custom_target(
    assert_le_fails.out
    assert_le_fails | sed "s/\\(.*\\)/     \\1/g" > assert_le_fails.out
    DEPENDS assert_le_fails sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # assert_lt_fails
  #
  file(GLOB ASSERT_LT_FAILS "${DOC_SRC_LIB}/samples/assert_lt_fails.cpp")
  add_custom_target(
    assert_lt_fails.cpp
    ${STRIP_SOURCES} < ${ASSERT_LT_FAILS} > assert_lt_fails.cpp
    DEPENDS stripped-src ${ASSERT_LT_FAILS}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(assert_lt_fails EXCLUDE_FROM_ALL ${ASSERT_LT_FAILS})
  target_link_libraries(assert_lt_fails crpcut)
  add_custom_target(
    assert_lt_fails.out
    assert_lt_fails | sed "s/\\(.*\\)/     \\1/g" > assert_lt_fails.out
    DEPENDS assert_lt_fails sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # assert_gt_fails
  #
  file(GLOB ASSERT_GT_FAILS "${DOC_SRC_LIB}/samples/assert_gt_fails.cpp")
  add_custom_target(
    assert_gt_fails.cpp
    ${STRIP_SOURCES} < ${ASSERT_GT_FAILS} > assert_gt_fails.cpp
    DEPENDS stripped-src ${ASSERT_GT_FAILS}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(assert_gt_fails EXCLUDE_FROM_ALL ${ASSERT_GT_FAILS})
  target_link_libraries(assert_gt_fails crpcut)
  add_custom_target(
    assert_gt_fails.out
    assert_gt_fails | sed "s/\\(.*\\)/     \\1/g" > assert_gt_fails.out
    DEPENDS assert_gt_fails sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # assert_ne_fails
  #
  file(GLOB ASSERT_NE_FAILS "${DOC_SRC_LIB}/samples/assert_ne_fails.cpp")
  add_custom_target(
    assert_ne_fails.cpp
    ${STRIP_SOURCES} < ${ASSERT_NE_FAILS} > assert_ne_fails.cpp
    DEPENDS stripped-src ${ASSERT_NE_FAILS}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(assert_ne_fails EXCLUDE_FROM_ALL ${ASSERT_NE_FAILS})
  target_link_libraries(assert_ne_fails crpcut)
  add_custom_target(
    assert_ne_fails.out
    assert_ne_fails | sed "s/\\(.*\\)/     \\1/g" > assert_ne_fails.out
    DEPENDS assert_ne_fails sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # assert_false_fails
  #
  file(GLOB ASSERT_FALSE_FAILS "${DOC_SRC_LIB}/samples/assert_false_fails.cpp")
  add_custom_target(
    assert_false_fails.cpp
    ${STRIP_SOURCES} < ${ASSERT_FALSE_FAILS} > assert_false_fails.cpp
    DEPENDS stripped-src ${ASSERT_FALSE_FAILS}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(assert_false_fails EXCLUDE_FROM_ALL ${ASSERT_FALSE_FAILS})
  target_link_libraries(assert_false_fails crpcut)
  add_custom_target(
    assert_false_fails.out
    assert_false_fails | sed "s/\\(.*\\)/     \\1/g" > assert_false_fails.out
    DEPENDS assert_false_fails sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)


  #
  # assert_true_fails
  #
  file(GLOB ASSERT_TRUE_FAILS "${DOC_SRC_LIB}/samples/assert_true_fails.cpp")
  add_custom_target(
    assert_true_fails.cpp
    ${STRIP_SOURCES} < ${ASSERT_TRUE_FAILS} > assert_true_fails.cpp
    DEPENDS stripped-src ${ASSERT_TRUE_FAILS}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(assert_true_fails EXCLUDE_FROM_ALL ${ASSERT_TRUE_FAILS})
  target_link_libraries(assert_true_fails crpcut)
  add_custom_target(
    assert_true_fails.out
    assert_true_fails | sed "s/\\(.*\\)/     \\1/g" > assert_true_fails.out
    DEPENDS assert_true_fails sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # assert_no_throw_fails
  #
  file(GLOB ASSERT_NO_THROW_FAILS "${DOC_SRC_LIB}/samples/assert_no_throw_fails.cpp")
  add_custom_target(
    assert_no_throw_fails.cpp
    ${STRIP_SOURCES} < ${ASSERT_NO_THROW_FAILS} > assert_no_throw_fails.cpp
    DEPENDS stripped-src ${ASSERT_NO_THROW_FAILS}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(assert_no_throw_fails EXCLUDE_FROM_ALL ${ASSERT_NO_THROW_FAILS})
  target_link_libraries(assert_no_throw_fails crpcut)
  add_custom_target(
    assert_no_throw_fails.out
    assert_no_throw_fails | sed "s/\\(.*\\)/     \\1/g" > assert_no_throw_fails.out
    DEPENDS assert_no_throw_fails sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # assert_throw_fails
  #
  file(GLOB ASSERT_THROW_FAILS "${DOC_SRC_LIB}/samples/assert_throw_fails.cpp")
  add_custom_target(
    assert_throw_fails.cpp
    ${STRIP_SOURCES} < ${ASSERT_THROW_FAILS} > assert_throw_fails.cpp
    DEPENDS stripped-src ${ASSERT_THROW_FAILS}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(assert_throw_fails EXCLUDE_FROM_ALL ${ASSERT_THROW_FAILS})
  target_link_libraries(assert_throw_fails crpcut)
  add_custom_target(
    assert_throw_fails.out
    assert_throw_fails | sed "s/\\(.*\\)/     \\1/g" > assert_throw_fails.out
    DEPENDS assert_throw_fails sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)


  #
  # assert_pred_fails
  #
  file(GLOB ASSERT_PRED_FAILS "${DOC_SRC_LIB}/samples/assert_pred_fails.cpp")
  add_custom_target(
    assert_pred_fails.cpp
    ${STRIP_SOURCES} < ${ASSERT_PRED_FAILS} > assert_pred_fails.cpp
    DEPENDS stripped-src ${ASSERT_PRED_FAILS}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(assert_pred_fails EXCLUDE_FROM_ALL ${ASSERT_PRED_FAILS})
  target_link_libraries(assert_pred_fails crpcut)
  add_custom_target(
    assert_pred_fails.out
    assert_pred_fails | sed "s/\\(.*\\)/     \\1/g" > assert_pred_fails.out
    DEPENDS assert_pred_fails sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # wrap_func
  #
  file(GLOB WRAP_FUNC "${DOC_SRC_LIB}/samples/wrap_func.cpp")
  add_custom_target(
    wrap_func.cpp
    ${STRIP_SOURCES} < ${WRAP_FUNC} > wrap_func.cpp
    DEPENDS stripped-src ${WRAP_FUNC}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(wrap_func EXCLUDE_FROM_ALL ${WRAP_FUNC})
  target_link_libraries(wrap_func crpcut)
  add_custom_target(
    wrap_func.out
    wrap_func | sed "s/\\(.*\\)/     \\1/g" > wrap_func.out
    DEPENDS wrap_func sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # wrap_v_func
  #
  file(GLOB WRAP_V_FUNC "${DOC_SRC_LIB}/samples/wrap_v_func.cpp")
  add_custom_target(
    wrap_v_func.cpp
    ${STRIP_SOURCES} < ${WRAP_V_FUNC} > wrap_v_func.cpp
    DEPENDS stripped-src ${WRAP_V_FUNC}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(wrap_v_func EXCLUDE_FROM_ALL ${WRAP_V_FUNC})
  target_link_libraries(wrap_v_func crpcut)
  add_custom_target(
    wrap_v_func.out
    wrap_v_func | sed "s/\\(.*\\)/     \\1/g" > wrap_v_func.out
    DEPENDS wrap_v_func sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # wrapped
  #
  file(GLOB WRAPPED "${DOC_SRC_LIB}/samples/wrapped.cpp")
  add_custom_target(
    wrapped.cpp
    ${STRIP_SOURCES} < ${WRAPPED} > wrapped.cpp
    DEPENDS stripped-src ${WRAPPED}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(wrapped EXCLUDE_FROM_ALL ${WRAPPED})
  target_link_libraries(wrapped crpcut)
  add_custom_target(
    wrapped.out
    wrapped | sed "s/\\(.*\\)/     \\1/g" > wrapped.out
    DEPENDS wrapped sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # collate
  #
  file(GLOB COLLATE "${DOC_SRC_LIB}/samples/collate.cpp")
  add_custom_target(
    collate.cpp
    ${STRIP_SOURCES} < ${COLLATE} > collate.cpp
    DEPENDS stripped-src ${COLLATE}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(collate EXCLUDE_FROM_ALL ${COLLATE})
  target_link_libraries(collate crpcut)
  add_custom_target(
    collate.out
    collate | sed "s/\\(.*\\)/     \\1/g" > collate.out
    DEPENDS collate sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # collate_case
  #
  file(GLOB COLLATE_CASE "${DOC_SRC_LIB}/samples/collate_case.cpp")
  add_custom_target(
    collate_case.cpp
    ${STRIP_SOURCES} < ${COLLATE_CASE} > collate_case.cpp
    DEPENDS stripped-src ${COLLATE_CASE}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(collate_case EXCLUDE_FROM_ALL ${COLLATE_CASE})
  target_link_libraries(collate_case crpcut)
  add_custom_target(
    collate_case.out
    collate_case | sed "s/\\(.*\\)/     \\1/g" > collate_case.out
    DEPENDS collate_case sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # deadline_cpu
  #
  file(GLOB DEADLINE_CPU "${DOC_SRC_LIB}/samples/deadline_cpu.cpp")
  add_custom_target(
    deadline_cpu.cpp
    ${STRIP_SOURCES} < ${DEADLINE_CPU} > deadline_cpu.cpp
    DEPENDS stripped-src ${DEADLINE_CPU}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(deadline_cpu EXCLUDE_FROM_ALL ${DEADLINE_CPU})
  target_link_libraries(deadline_cpu crpcut)
  add_custom_target(
    deadline_cpu.out
    deadline_cpu | sed "s/\\(.*\\)/     \\1/g" > deadline_cpu.out
    DEPENDS deadline_cpu sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # deadline_realtime
  #
  file(GLOB DEADLINE_REALTIME "${DOC_SRC_LIB}/samples/deadline_realtime.cpp")
  add_custom_target(
    deadline_realtime.cpp
    ${STRIP_SOURCES} < ${DEADLINE_REALTIME} > deadline_realtime.cpp
    DEPENDS stripped-src ${DEADLINE_REALTIME}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(deadline_realtime EXCLUDE_FROM_ALL ${DEADLINE_REALTIME})
  target_link_libraries(deadline_realtime crpcut)
  add_custom_target(
    deadline_realtime.out
    deadline_realtime | sed "s/\\(.*\\)/     \\1/g" > deadline_realtime.out
    DEPENDS deadline_realtime sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # depends_on
  #
  file(GLOB DEPENDS_ON "${DOC_SRC_LIB}/samples/depends_on.cpp")
  add_custom_target(
    depends_on.cpp
    ${STRIP_SOURCES} < ${DEPENDS_ON} > depends_on.cpp
    DEPENDS stripped-src ${DEPENDS_ON}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(depends_on EXCLUDE_FROM_ALL ${DEPENDS_ON})
  target_link_libraries(depends_on crpcut)
  add_custom_target(
    depends_on.out
    depends_on | sed "s/\\(.*\\)/     \\1/g" > depends_on.out
    DEPENDS depends_on sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # expect_exception
  #
  file(GLOB EXPECT_EXCEPTION "${DOC_SRC_LIB}/samples/expect_exception.cpp")
  add_custom_target(
    expect_exception.cpp
    ${STRIP_SOURCES} < ${EXPECT_EXCEPTION} > expect_exception.cpp
    DEPENDS stripped-src ${EXPECT_EXCEPTION}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(expect_exception EXCLUDE_FROM_ALL ${EXPECT_EXCEPTION})
  target_link_libraries(expect_exception crpcut)
  add_custom_target(
    expect_exception.out
    expect_exception | sed "s/\\(.*\\)/     \\1/g" > expect_exception.out
    DEPENDS expect_exception sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # expect_exit
  #
  file(GLOB EXPECT_EXIT "${DOC_SRC_LIB}/samples/expect_exit.cpp")
  add_custom_target(
    expect_exit.cpp
    ${STRIP_SOURCES} < ${EXPECT_EXIT} > expect_exit.cpp
    DEPENDS stripped-src ${EXPECT_EXIT}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(expect_exit EXCLUDE_FROM_ALL ${EXPECT_EXIT})
  target_link_libraries(expect_exit crpcut)
  add_custom_target(
    expect_exit.out
    expect_exit | sed "s/\\(.*\\)/     \\1/g" > expect_exit.out
    DEPENDS expect_exit sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # expect_signal_death
  #
  file(GLOB EXPECT_SIGNAL_DEATH "${DOC_SRC_LIB}/samples/expect_signal_death.cpp")
  add_custom_target(
    expect_signal_death.cpp
    ${STRIP_SOURCES} < ${EXPECT_SIGNAL_DEATH} > expect_signal_death.cpp
    DEPENDS stripped-src ${EXPECT_SIGNAL_DEATH}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(expect_signal_death EXCLUDE_FROM_ALL ${EXPECT_SIGNAL_DEATH})
  target_link_libraries(expect_signal_death crpcut)
  add_custom_target(
    expect_signal_death.out
    expect_signal_death | sed "s/\\(.*\\)/     \\1/g" > expect_signal_death.out
    DEPENDS expect_signal_death sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)


  #
  # fail
  #
  file(GLOB FAIL "${DOC_SRC_LIB}/samples/fail.cpp")
  add_custom_target(
    fail.cpp
    ${STRIP_SOURCES} < ${FAIL} > fail.cpp
    DEPENDS stripped-src ${FAIL}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(fail EXCLUDE_FROM_ALL ${FAIL})
  target_link_libraries(fail crpcut)
  add_custom_target(
    fail.out
    fail | sed "s/\\(.*\\)/     \\1/g" > fail.out
    DEPENDS fail sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # info
  #
  file(GLOB INFO "${DOC_SRC_LIB}/samples/info.cpp")
  add_custom_target(
    info.cpp
    ${STRIP_SOURCES} < ${INFO} > info.cpp
    DEPENDS stripped-src ${INFO}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(info EXCLUDE_FROM_ALL ${INFO})
  target_link_libraries(info crpcut)
  add_custom_target(
    info.out
    info | sed "s/\\(.*\\)/     \\1/g" > info.out
    DEPENDS info sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # no_core
  #
  file(GLOB NO_CORE "${DOC_SRC_LIB}/samples/no_core.cpp")
  add_custom_target(
    no_core.cpp
    ${STRIP_SOURCES} < ${NO_CORE} > no_core.cpp
    DEPENDS stripped-src ${NO_CORE}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(no_core EXCLUDE_FROM_ALL ${NO_CORE})
  target_link_libraries(no_core crpcut)
  add_custom_target(
    no_core.out
    no_core | sed "s/\\(.*\\)/     \\1/g" > no_core.out
    DEPENDS no_core sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # infinite_loop
  #
  file(GLOB INFINITE_LOOP "${DOC_SRC_LIB}/samples/infinite_loop.cpp")
  add_custom_target(
    infinite_loop.cpp
    ${STRIP_SOURCES} < ${INFINITE_LOOP} > infinite_loop.cpp
    DEPENDS stripped-src ${INFINITE_LOOP}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(infinite_loop EXCLUDE_FROM_ALL ${INFINITE_LOOP})
  target_link_libraries(infinite_loop crpcut)
  add_custom_target(
    infinite_loop.out
    infinite_loop | sed "s/\\(.*\\)/     \\1/g" > infinite_loop.out
    DEPENDS infinite_loop sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # file_fail
  #
  file(GLOB FILE_FAIL "${DOC_SRC_LIB}/samples/file_fail.cpp")
  add_custom_target(
    file_fail.cpp
    ${STRIP_SOURCES} < ${FILE_FAIL} > file_fail.cpp
    DEPENDS stripped-src ${FILE_FAIL}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(file_fail EXCLUDE_FROM_ALL ${FILE_FAIL})
  target_link_libraries(file_fail crpcut)
  add_custom_target(
    file_fail.out
    file_fail | sed "s/\\(.*\\)/     \\1/g" > file_fail.out
    DEPENDS file_fail sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # ambiguous_fix
  #
  file(GLOB AMBIGUOUS_FIX "${DOC_SRC_LIB}/samples/ambiguous_fix.cpp")
  add_custom_target(
    ambiguous_fix.cpp
    ${STRIP_SOURCES} < ${AMBIGUOUS_FIX} > ambiguous_fix.cpp
    DEPENDS stripped-src ${AMBIGUOUS_FIX}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(ambiguous_fix EXCLUDE_FROM_ALL ${AMBIGUOUS_FIX})
  target_link_libraries(ambiguous_fix crpcut)
  add_custom_target(
    ambiguous_fix.out
    ambiguous_fix | sed "s/\\(.*\\)/     \\1/g" > ambiguous_fix.out
    DEPENDS ambiguous_fix sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # simple_fix
  #
  file(GLOB SIMPLE_FIX "${DOC_SRC_LIB}/samples/simple_fix.cpp")
  add_custom_target(
    simple_fix.cpp
    ${STRIP_SOURCES} < ${SIMPLE_FIX} > simple_fix.cpp
    DEPENDS stripped-src ${SIMPLE_FIX}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(simple_fix EXCLUDE_FROM_ALL ${SIMPLE_FIX})
  target_link_libraries(simple_fix crpcut)
  add_custom_target(
    simple_fix.out
    simple_fix | sed "s/\\(.*\\)/     \\1/g" > simple_fix.out
    DEPENDS simple_fix sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # file_fix
  #
  file(GLOB FILE_FIX "${DOC_SRC_LIB}/samples/file_fix.cpp")
  add_custom_target(
    file_fix.cpp
    ${STRIP_SOURCES} < ${FILE_FIX} > file_fix.cpp
    DEPENDS stripped-src ${FILE_FIX}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(file_fix EXCLUDE_FROM_ALL ${FILE_FIX})
  target_link_libraries(file_fix crpcut)
  add_custom_target(
    file_fix.out
    file_fix | sed "s/\\(.*\\)/     \\1/g" > file_fix.out
    DEPENDS file_fix sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # abs_diff
  #
  file(GLOB ABS_DIFF "${DOC_SRC_LIB}/samples/abs_diff.cpp")
  add_custom_target(
    abs_diff.cpp
    ${STRIP_SOURCES} < ${ABS_DIFF} > abs_diff.cpp
    DEPENDS stripped-src ${ABS_DIFF}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(abs_diff EXCLUDE_FROM_ALL ${ABS_DIFF})
  target_link_libraries(abs_diff crpcut)
  add_custom_target(
    abs_diff.out
    abs_diff | sed "s/\\(.*\\)/     \\1/g" > abs_diff.out
    DEPENDS abs_diff sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)


  #
  # relative_diff
  #
  file(GLOB RELATIVE_DIFF "${DOC_SRC_LIB}/samples/relative_diff.cpp")
  add_custom_target(
    relative_diff.cpp
    ${STRIP_SOURCES} < ${RELATIVE_DIFF} > relative_diff.cpp
    DEPENDS stripped-src ${RELATIVE_DIFF}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(relative_diff EXCLUDE_FROM_ALL ${RELATIVE_DIFF})
  target_link_libraries(relative_diff crpcut)
  add_custom_target(
    relative_diff.out
    relative_diff | sed "s/\\(.*\\)/     \\1/g" > relative_diff.out
    DEPENDS relative_diff sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # ulps_diff
  #
  file(GLOB ULPS_DIFF "${DOC_SRC_LIB}/samples/ulps_diff.cpp")
  add_custom_target(
    ulps_diff.cpp
    ${STRIP_SOURCES} < ${ULPS_DIFF} > ulps_diff.cpp
    DEPENDS stripped-src ${ULPS_DIFF}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(ulps_diff EXCLUDE_FROM_ALL ${ULPS_DIFF})
  target_link_libraries(ulps_diff crpcut)
  add_custom_target(
    ulps_diff.out
    ulps_diff | sed "s/\\(.*\\)/     \\1/g" > ulps_diff.out
    DEPENDS ulps_diff sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # regex
  #
  file(GLOB REGEX "${DOC_SRC_LIB}/samples/regex.cpp")
  add_custom_target(
    regex.cpp
    ${STRIP_SOURCES} < ${REGEX} > regex.cpp
    DEPENDS stripped-src ${REGEX}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(regex EXCLUDE_FROM_ALL ${REGEX})
  target_link_libraries(regex crpcut)
  add_custom_target(
    regex.out
    regex | sed "s/\\(.*\\)/     \\1/g" > regex.out
    DEPENDS regex sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # simple_testsuite
  #
  file(GLOB SIMPLE_TESTSUITE "${DOC_SRC_LIB}/samples/simple_testsuite.cpp")
  add_custom_target(
    simple_testsuite.cpp
    ${STRIP_SOURCES} < ${SIMPLE_TESTSUITE} > simple_testsuite.cpp
    DEPENDS stripped-src ${SIMPLE_TESTSUITE}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(simple_testsuite EXCLUDE_FROM_ALL ${SIMPLE_TESTSUITE})
  target_link_libraries(simple_testsuite crpcut)
  add_custom_target(
    simple_testsuite.out
    simple_testsuite | sed "s/\\(.*\\)/     \\1/g" > simple_testsuite.out
    DEPENDS simple_testsuite sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # report_example
  #
  file(GLOB REPORT_EXAMPLE "${DOC_SRC_LIB}/samples/report_example.cpp")
  add_custom_target(
    report_example.cpp
    ${STRIP_SOURCES} < ${REPORT_EXAMPLE} > report_example.cpp
    DEPENDS stripped-src ${REPORT_EXAMPLE}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(report_example EXCLUDE_FROM_ALL ${REPORT_EXAMPLE})
  target_link_libraries(report_example crpcut)
  set_target_properties(report_example PROPERTIES COMPILE_FLAGS "-ggdb")
  add_custom_target(
    report_example_human.out
    report_example -v | sed "s/\\(.*\\)/     \\1/g" > report_example_human.out
    DEPENDS report_example sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)
  add_custom_target(
    report_example_xml.out
    report_example -v -x | sed "s/\\(.*\\)/     \\1/g" > report_example_xml.out
    DEPENDS report_example sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  #
  # simple_depend
  #
  file(GLOB SUITE_DEPEND "${DOC_SRC_LIB}/samples/suite_depend.cpp")
  file(GLOB SUITE_DEFINE "${DOC_SRC_LIB}/samples/suite_define.cpp")
  add_custom_target(
    suite_depend.cpp
    ${STRIP_SOURCES} < ${SUITE_DEPEND} > suite_depend.cpp
    DEPENDS stripped-src ${SUITE_DEPEND}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_custom_target(
    suite_define.cpp
    ${STRIP_SOURCES} < ${SUITE_DEFINE} > suite_define.cpp
    DEPENDS stripped-src ${SUITE_DEFINE}
    WORKING_DIRECTORY stripped-src
    VERBATIM)
  add_executable(suite_depend EXCLUDE_FROM_ALL
    ${SUITE_DEPEND} ${SUITE_DEFINE})
  target_link_libraries(suite_depend crpcut)
  add_custom_target(
    suite_depend.out
    suite_depend | sed "s/\\(.*\\)/     \\1/g" > suite_depend.out
    DEPENDS suite_depend sample-output
    WORKING_DIRECTORY sample-output
    VERBATIM)

  add_custom_target(
    doc-src
    test -L doc-src || ln -s ${DOC_SRC_LIB} doc-src
    )

  #
  # refman
  #
  add_custom_target(
    refman_html
    ${XSLTPROC}
    --stringparam chunk.first.sections 1
    --stringparam navig.graphics 1
    --stringparam navig.graphics.extension .png
    --stringparam navig.graphics.path ../images/
    --stringparam admon.graphics 1
    --stringparam admon.graphics.extension .png
    --stringparam admon.graphics.path ../images/
    --stringparam admon.graphic.width 57
    --stringparam html.stylesheet ../crpcut-doc.css
    --xinclude --stringparam use.extensions 1
    --stringparam textinsert.extension 1
    ${HTML_CHUNK_XSL} - < ${REFMAN_SOURCES}
    DEPENDS html/refman
    stripped-src/all_tests.cpp sample-output/all_tests.out
    stripped-src/exit_any_code.cpp sample-output/exit_any_code.out
    stripped-src/assert_ne_fails.cpp sample-output/assert_ne_fails.out
    stripped-src/assert_le_fails.cpp sample-output/assert_le_fails.out
    stripped-src/assert_lt_fails.cpp sample-output/assert_lt_fails.out
    stripped-src/assert_eq_fails.cpp sample-output/assert_eq_fails.out
    stripped-src/assert_ge_fails.cpp sample-output/assert_ge_fails.out
    sample-output/debug_assert_eq_fails.out
    stripped-src/assert_gt_fails.cpp sample-output/assert_gt_fails.out
    stripped-src/assert_false_fails.cpp sample-output/assert_false_fails.out
    stripped-src/assert_true_fails.cpp sample-output/assert_true_fails.out
    stripped-src/assert_no_throw_fails.cpp sample-output/assert_no_throw_fails.out
    stripped-src/assert_throw_fails.cpp sample-output/assert_throw_fails.out
    stripped-src/assert_pred_fails.cpp sample-output/assert_pred_fails.out
    stripped-src/wrap_func.cpp sample-output/wrap_func.out
    stripped-src/wrap_v_func.cpp sample-output/wrap_v_func.out
    stripped-src/wrapped.cpp sample-output/wrapped.out
    stripped-src/collate.cpp sample-output/collate.out
    stripped-src/collate_case.cpp sample-output/collate_case.out
    stripped-src/deadline_cpu.cpp sample-output/deadline_cpu.out
    stripped-src/deadline_realtime.cpp sample-output/deadline_realtime.out
    stripped-src/depends_on.cpp sample-output/depends_on.out
    stripped-src/expect_exception.cpp sample-output/expect_exception.out
    stripped-src/expect_exit.cpp sample-output/expect_exit.out
    stripped-src/expect_signal_death.cpp sample-output/expect_signal_death.out
    stripped-src/fail.cpp sample-output/fail.out
    stripped-src/info.cpp sample-output/info.out
    stripped-src/no_core.cpp sample-output/no_core.out
    stripped-src/abs_diff.cpp sample-output/abs_diff.out
    stripped-src/relative_diff.cpp sample-output/relative_diff.out
    stripped-src/ulps_diff.cpp sample-output/ulps_diff.out
    stripped-src/regex.cpp sample-output/regex.out
    stripped-src/simple_testsuite.cpp sample-output/simple_testsuite.out
    stripped-src/report_example.cpp sample-output/report_example_human.out
    sample-output/report_example_xml.out
    stripped-src/simple_fix.cpp sample-output/simple_fix.out
    stripped-src/file_fix.cpp sample-output/file_fix.out
    stripped-src/ambiguous_fix.cpp sample-output/ambiguous_fix.out
    stripped-src/file_fail.cpp sample-output/file_fail.out
    stripped-src/infinite_loop.cpp sample-output/infinite_loop.out
    stripped-src/suite_depend.cpp sample-output/suite_depend.out
    stripped-src/suite_define.cpp
    can_dump_core
    ${REFMAN_SOURCES} ${DOC_SRC_LIB}/crpcut-doc.css
    WORKING_DIRECTORY html/refman)

  install(
    DIRECTORY ${CMAKE_BINARY_DIR}/html
    DESTINATION "${SHAREDIR}/doc"
    )
  install(
    DIRECTORY ${DOC_SRC_LIB}/icons/
    DESTINATION "${SHAREDIR}/doc/html/images"
    FILES_MATCHING PATTERN "*.png"
    )
  install(
    FILES ${DOC_SRC_LIB}/crpcut-doc.css
    DESTINATION "${SHAREDIR}/doc/html"
    )
endif(XSLTPROC AND HTML_CHUNK_XSL AND GDB AND SED)

find_program(RUBY "ruby")
if(RUBY)
  add_custom_target(selftest ${RUBY} ${CMAKE_SOURCE_DIR}/selftest.rb ${SELFTEST_PARAM} DEPENDS testprog VERBATIM)
else(RUBY)
  message("ruby is not found - selftest target not available")
endif(RUBY)
find_program(AWK "awk")
find_program(BASH "bash")
find_program(XMLLINT "xmllint")
if (AWK AND BASH AND XMLLINT)
   add_custom_target(sanitycheck ${BASH} ${CMAKE_SOURCE_DIR}/sanitycheck.sh ${AWK} ${CMAKE_SOURCE_DIR} ${SELFTEST_PARAM} DEPENDS testprog VERBATIM)
endif (AWK AND BASH AND XMLLINT)
set(LIBRARY_OUTPUT_PATH lib)
set(EXECUTABLE_OUTPUT_PATH test)

if(HAVE_CXX0X AND NOT HAVE_TYPEOF)
  message("*** IMPORTANT *** You must compile your tests using -std=c++0x")
  add_definitions(-std=c++0x)
endif(HAVE_CXX0X AND NOT HAVE_TYPEOF)
if (HAVE_VALGRIND)
  message("*** compiling with support for valgrind")
  add_definitions(-DHAVE_VALGRIND)
  set(CMAKE_CXX_FLAGS_MAINT "${CMAKE_CXX_FLAGS_MAINT} -Wno-long-long")
endif (HAVE_VALGRIND)

install(
  FILES include/crpcut.hpp
  DESTINATION include
)

install(TARGETS crpcut DESTINATION lib)

install(
  FILES ${TEST_SRCS}
  DESTINATION "${SHAREDIR}/example_source"
)
install(
  FILES crpcut.xsd FAQ TODO
  DESTINATION "${SHAREDIR}"
  )
if(XSLTPROC)
endif(XSLTPROC)
