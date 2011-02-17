/*
 * Copyright 2009-2011 Bjorn Fahller <bjorn@fahller.se>
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
      float p1 = 0.5f;
      float p2 = 0.5f + eps;
      ASSERT_PRED(crpcut::match<crpcut::abs_diff>(eps), p1, p2);
    }

    TEST(should_succeed_sub_epsilon_float, epsilon<float>)
    {
      float p1 = 0.5f;
      float p2 = 0.5f - eps;
      ASSERT_PRED(crpcut::match<crpcut::abs_diff>(eps), p1, p2);
    }

    TEST(should_fail_add_2epsilon_float, epsilon<float>)
    {
      float p1 = 0.5f;
      float p2 = 0.5f + 2*eps;
      ASSERT_PRED(crpcut::match<crpcut::abs_diff>(eps), p1, p2);
    }

    TEST(should_fail_sub_2epsilon_float, epsilon<float>)
    {
      float p1 = 0.5f;
      float p2 = 0.5f - 2*eps;
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
      long double p1 = 0.5l;
      long double p2 = 0.5l + eps;
      ASSERT_PRED(crpcut::match<crpcut::abs_diff>(eps), p1, p2);
    }

    TEST(should_succeed_sub_epsilon_long_double, epsilon<long double>)
    {
      long double p1 = 0.5l;
      long double p2 = 0.5l - eps;
      ASSERT_PRED(crpcut::match<crpcut::abs_diff>(eps), p1, p2);
    }

    TEST(should_fail_add_2epsilon_long_double, epsilon<long double>)
    {
      long double p1 = 0.5l;
      long double p2 = 0.5l + 2*eps;
      ASSERT_PRED(crpcut::match<crpcut::abs_diff>(eps), p1, p2);
    }

    TEST(should_fail_sub_2epsilon_long_double, epsilon<long double>)
    {
      long double p1 = 0.5l;
      long double p2 = 0.5l - 2*eps;
      ASSERT_PRED(crpcut::match<crpcut::abs_diff>(eps), p1, p2);
    }

  }
  TESTSUITE(relative)
  {
    TEST(should_fail_relative_epsilon_float, epsilon<float>)
    {
      float p1 = 0.5f;
      float p2 = 0.5f + eps;
      ASSERT_PRED(crpcut::match<crpcut::relative_diff>(eps), p1, p2);
    }

    TEST(should_succeed_relative_epsilon_float, epsilon<float>)
    {
      float p1 = 1.0f;
      float p2 = 1.0f + eps;
      ASSERT_PRED(crpcut::match<crpcut::relative_diff>(eps), p1, p2);
    }

    TEST(should_fail_relative_epsilon_double, epsilon<double>)
    {
      double p1 = 0.5f;
      double p2 = 0.5f + eps;
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
      long double p1 = 0.5l;
      long double p2 = 0.5l + eps;
      ASSERT_PRED(crpcut::match<crpcut::relative_diff>(eps), p1, p2);
    }

    TEST(should_succeed_relative_epsilon_long_double, epsilon<long double>)
    {
      long double p1 = 1.0l;
      long double p2 = 1.0l + eps;
      ASSERT_PRED(crpcut::match<crpcut::relative_diff>(eps), p1, p2);
    }

  }

  TESTSUITE(ulps)
  {
    TESTSUITE(using_float)
    {
      TEST(should_succeed_equal_zeroes_0_ulps)
      {
        float f1 = -0.0;
        float f2 = +0.0;
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(0U), f1, f2);
      }

      TEST(should_succeed_eps_diff_1_ulp)
      {
        float f1 = 1.0;
        float f2 = f1 + std::numeric_limits<float>::epsilon();
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(1U), f1, f2);
      }

      TEST(should_fail_eps_diff_0_ulp)
      {
        float f1 = 1.0;
        float f2 = f1 + std::numeric_limits<float>::epsilon();
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(0U), f1, f2);
      }

      TEST(should_succeed_high_denorm_1_ulp)
      {
        float f1 = std::numeric_limits<float>::min();
        float f2 = f1 - std::numeric_limits<float>::denorm_min();
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(1U), f1, f2);
      }

      TEST(should_fail_high_denorm_0_ulp)
      {
        float f1 = std::numeric_limits<float>::min();
        float f2 = f1 - std::numeric_limits<float>::denorm_min();
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(0U), f1, f2);
      }

      TEST(should_succeed_low_denorm_1_ulp)
      {
        float f1 = 0.0;
        float f2 = std::numeric_limits<float>::denorm_min();
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(1U), f1, f2);
      }

      TEST(should_fail_low_denorm_0_ulp)
      {
        float f1 = 0.0;
        float f2 = std::numeric_limits<float>::denorm_min();
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(0U), f1, f2);
      }

      TEST(should_succeed_pos_neg_denorm_min_2_ulps)
      {
        float f1 = +std::numeric_limits<float>::denorm_min();
        float f2 = -std::numeric_limits<float>::denorm_min();
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(2U), f1, f2);
      }

      TEST(should_fail_pos_neg_denorm_min_1_ulp)
      {
        float f1 = +std::numeric_limits<float>::denorm_min();
        float f2 = -std::numeric_limits<float>::denorm_min();
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(1U), f1, f2);
      }

      TEST(should_fail_nan)
      {
        float d1 = 0.0;
        float d2 = 0.0;
        float f1 = d1/d2;
        float f2 = 0.0;
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(~unsigned()), f1, f2);
      }

      TEST(should_fail_max_inf_1_ulp)
      {
        float d1 = 1.0;
        float d2 = 0.0;
        float f1 = d1/d2;
        float f2 = std::numeric_limits<float>::max();
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(1U), f1, f2);
      }

      TEST(should_succeed_max_inf_1_ulp)
      {
        float d1 = 1.0;
        float d2 = 0.0;
        float f1 = d1/d2;
        float f2 = std::numeric_limits<float>::max();
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(1U, crpcut::include_inf),
                    f1,
                    f2);
      }
    }
    TESTSUITE(using_double)
    {
      TEST(should_succeed_equal_zeroes_0_ulps)
      {
        double f1 = -0.0;
        double f2 = +0.0;
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(0U), f1, f2);
      }

      TEST(should_succeed_eps_diff_1_ulp)
      {
        double f1 = 1.0;
        double f2 = f1 + std::numeric_limits<double>::epsilon();
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(1U), f1, f2);
      }

      TEST(should_fail_eps_diff_0_ulp)
      {
        double f1 = 1.0;
        double f2 = f1 + std::numeric_limits<double>::epsilon();
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(0U), f1, f2);
      }

      TEST(should_succeed_high_denorm_1_ulp)
      {
        double f1 = std::numeric_limits<double>::min();
        double f2 = f1 - std::numeric_limits<double>::denorm_min();
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(1U), f1, f2);
      }

      TEST(should_fail_high_denorm_0_ulp)
      {
        double f1 = std::numeric_limits<double>::min();
        double f2 = f1 - std::numeric_limits<double>::denorm_min();
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(0U), f1, f2);
      }

      TEST(should_succeed_low_denorm_1_ulp)
      {
        double f1 = 0.0;
        double f2 = std::numeric_limits<double>::denorm_min();
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(1U), f1, f2);
      }

      TEST(should_fail_low_denorm_0_ulp)
      {
        double f1 = 0.0;
        double f2 = std::numeric_limits<double>::denorm_min();
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(0U), f1, f2);
      }

      TEST(should_succeed_pos_neg_denorm_min_2_ulps)
      {
        double f1 = +std::numeric_limits<double>::denorm_min();
        double f2 = -std::numeric_limits<double>::denorm_min();
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(2U), f1, f2);
      }

      TEST(should_fail_pos_neg_denorm_min_1_ulp)
      {
        double f1 = +std::numeric_limits<double>::denorm_min();
        double f2 = -std::numeric_limits<double>::denorm_min();
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(1U), f1, f2);
      }

      TEST(should_fail_nan)
      {
        double d1 = 0.0;
        double d2 = 0.0;
        double f1 = d1/d2;
        double f2 = 0.0;
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(~unsigned()), f1, f2);
      }

      TEST(should_fail_max_inf_1_ulp)
      {
        double d1 = 1.0;
        double d2 = 0.0;
        double f1 = d1/d2;
        double f2 = std::numeric_limits<double>::max();
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(1U), f1, f2);
      }

      TEST(should_succeed_max_inf_1_ulp)
      {
        double d1 = 1.0;
        double d2 = 0.0;
        double f1 = d1/d2;
        double f2 = std::numeric_limits<double>::max();
        ASSERT_PRED(crpcut::match<crpcut::ulps_diff>(1U, crpcut::include_inf),
                    f1,
                    f2);
      }
    }
  }
}
