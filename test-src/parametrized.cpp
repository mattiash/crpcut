/*
 * Copyright 2009 Bjorn Fahller <bjorn@fahller.se>
 * All rights reserved

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include <crpcut.hpp>

TESTSUITE(parametrized)
{
  class ptest
  {
  protected:
    template <typename T1, typename T2>
    void a_test(T1 p1, T2 p2)
    {
      ASSERT_LT(p1, p2);
    }
  };

  TEST(should_succeed_assert_lt_int_double, ptest)
  {
    a_test(3, 3.1415);
  }

  TEST(should_succeed_assert_lt_int_char, ptest)
  {
    a_test(32, 'A');
  }

  TEST(should_succeed_assert_lt_char_array_string, ptest)
  {
    a_test("apa", std::string("katt"));
  }

  TEST(should_fail_assert_lt_int_double, ptest)
  {
    a_test(4, 3.1415);
  }

  TEST(should_fail_assert_lt_int_char, ptest)
  {
    a_test(800, 'A');
  }

  TEST(should_fail_assert_lt_char_array_string, ptest)
  {
    a_test("orm", std::string("katt"));
  }
}
