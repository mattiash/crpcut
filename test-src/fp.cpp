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
#include <limits>

template <typename T>
struct epsilon
{
  T eps;
  epsilon()
    : eps(std::numeric_limits<T>::epsilon())
  {
  }
};

TESTSUITE(fp)
{
  TESTSUITE(abs)
  {
    TEST(should_succeed_add_epsilon_float, epsilon<float>)
    {
      float p1 = 0.5;
      float p2 = 0.5 + eps;
      ASSERT_PRED(crpcut::match<crpcut::abs_diff>(eps), p1, p2);
    }
    TEST(should_succeed_sub_epsilon_float, epsilon<float>)
    {
      float p1 = 0.5;
      float p2 = 0.5 - eps;
      ASSERT_PRED(crpcut::match<crpcut::abs_diff>(eps), p1, p2);
    }
    TEST(should_fail_add_2epsilon_float, epsilon<float>)
    {
      float p1 = 0.5;
      float p2 = 0.5 + 2*eps;
      ASSERT_PRED(crpcut::match<crpcut::abs_diff>(eps), p1, p2);
    }
    TEST(should_fail_sub_2epsilon_float, epsilon<float>)
    {
      float p1 = 0.5;
      float p2 = 0.5 - 2*eps;
      ASSERT_PRED(crpcut::match<crpcut::abs_diff>(eps), p1, p2);
    }

    TEST(should_succeed_add_epsilon_double, epsilon<double>)
    {
      double p1 = 0.5;
      double p2 = 0.5 + eps;
      ASSERT_PRED(crpcut::match<crpcut::abs_diff>(eps), p1, p2);
    }
    TEST(should_succeed_sub_epsilon_double, epsilon<double>)
    {
      double p1 = 0.5;
      double p2 = 0.5 - eps;
      ASSERT_PRED(crpcut::match<crpcut::abs_diff>(eps), p1, p2);
    }
    TEST(should_fail_add_2epsilon_double, epsilon<double>)
    {
      double p1 = 0.5;
      double p2 = 0.5 + 2*eps;
      ASSERT_PRED(crpcut::match<crpcut::abs_diff>(eps), p1, p2);
    }
    TEST(should_fail_sub_2epsilon_double, epsilon<double>)
    {
      double p1 = 0.5;
      double p2 = 0.5 - 2*eps;
      ASSERT_PRED(crpcut::match<crpcut::abs_diff>(eps), p1, p2);
    }


    TEST(should_succeed_add_epsilon_long_double, epsilon<long double>)
    {
      long double p1 = 0.5;
      long double p2 = 0.5 + eps;
      ASSERT_PRED(crpcut::match<crpcut::abs_diff>(eps), p1, p2);
    }
    TEST(should_succeed_sub_epsilon_long_double, epsilon<long double>)
    {
      long double p1 = 0.5;
      long double p2 = 0.5 - eps;
      ASSERT_PRED(crpcut::match<crpcut::abs_diff>(eps), p1, p2);
    }
    TEST(should_fail_add_2epsilon_long_double, epsilon<long double>)
    {
      long double p1 = 0.5;
      long double p2 = 0.5 + 2*eps;
      ASSERT_PRED(crpcut::match<crpcut::abs_diff>(eps), p1, p2);
    }
    TEST(should_fail_sub_2epsilon_long_double, epsilon<long double>)
    {
      long double p1 = 0.5;
      long double p2 = 0.5 - 2*eps;
      ASSERT_PRED(crpcut::match<crpcut::abs_diff>(eps), p1, p2);
    }

  }
  TESTSUITE(relative)
  {
    TEST(should_fail_relative_epsilon_float, epsilon<float>)
    {
      float p1 = 0.5;
      float p2 = 0.5 + eps;
      ASSERT_PRED(crpcut::match<crpcut::relative_diff>(eps), p1, p2);
    }
    TEST(should_succeed_relative_epsilon_float, epsilon<float>)
    {
      float p1 = 1.0;
      float p2 = 1.0 + eps;
      ASSERT_PRED(crpcut::match<crpcut::relative_diff>(eps), p1, p2);
    }

    TEST(should_fail_relative_epsilon_double, epsilon<double>)
    {
      double p1 = 0.5;
      double p2 = 0.5 + eps;
      ASSERT_PRED(crpcut::match<crpcut::relative_diff>(eps), p1, p2);
    }
    TEST(should_succeed_relative_epsilon_double, epsilon<double>)
    {
      double p1 = 1.0;
      double p2 = 1.0 + eps;
      ASSERT_PRED(crpcut::match<crpcut::relative_diff>(eps), p1, p2);
    }

    TEST(should_fail_relative_epsilon_long_double, epsilon<long double>)
    {
      long double p1 = 0.5;
      long double p2 = 0.5 + eps;
      ASSERT_PRED(crpcut::match<crpcut::relative_diff>(eps), p1, p2);
    }
    TEST(should_succeed_relative_epsilon_long_double, epsilon<long double>)
    {
      long double p1 = 1.0;
      long double p2 = 1.0 + eps;
      ASSERT_PRED(crpcut::match<crpcut::relative_diff>(eps), p1, p2);
    }

  }
}
