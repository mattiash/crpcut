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


#include <string>
#include <fstream>
#include <ostream>
#include "crpcut.hpp"
extern "C" {
#include <signal.h> // raise
}
template <int N>
struct fixture
{
  fixture() : num(N) {}
  int num ;
};

TEST(default_success)
{
}

TEST(very_slow_success)
{
  sleep(3);
}

TESTSUITE(exit_deaths)
{
  TEST(should_fail_with_exit_code_3)
  {
    exit(3);
  }

  TEST(should_succeed_with_exit_code_3, EXPECT_EXIT(3))
    {
      exit(3);
    }

  TEST(should_fail_with_no_exit, EXPECT_EXIT(3))
    {
    }

  TEST(should_fail_with_wrong_exit_code, EXPECT_EXIT(3))
    {
      exit(4);
    }
}
TESTSUITE(signal_deaths)
{
  TEST(should_fail_with_left_behind_core_dump_due_to_death_on_signal_11)
  {
    raise(11);
  }

  TEST(should_fail_without_core_dump_with_death_on_signal_11, NO_CORE_FILE)
    {
      raise(11);
    }

  TEST(should_succeed_with_death_on_signal_11,
       NO_CORE_FILE,
       EXPECT_SIGNAL_DEATH(11))
    {
      raise(11);
    }

  TEST(should_fail_with_wrong_signal, NO_CORE_FILE, EXPECT_SIGNAL_DEATH(11))
    {
      raise(6);
    }
  TEST(should_fail_with_normal_exit, EXPECT_SIGNAL_DEATH(11))
    {
    }
}

TESTSUITE(exception_deaths)
{
  TEST(should_fail_due_to_unknown_exception)
  {
    throw 1;
  }

  TEST(should_fail_due_to_std_exception_with_string_apa)
  {
    throw std::range_error("apa");
  }

  TEST(should_succeed_with_range_error_thrown,
       EXPECT_EXCEPTION(std::range_error))
    {
      throw std::range_error("apa");
    }
  TEST(should_fail_with_wrong_exception, EXPECT_EXCEPTION(std::range_error))
    {
      throw std::bad_alloc();
    }

  TEST(should_succed_with_any_exception, EXPECT_EXCEPTION(...))
    {
      throw "apa";
    }

  TEST(should_fail_any_exception, EXPECT_EXCEPTION(...))
    {
    }

  TEST(should_fail_with_no_exception, EXPECT_EXCEPTION(std::exception))
    {
    }
}





TESTSUITE(asserts)
{
  TEST(should_succeed_assert_throw_with_correct_exception)
  {
    ASSERT_THROW(throw std::bad_alloc(), std::bad_alloc);
  }

  TEST(should_fail_assert_exception_with_wrong_exception)
  {
    ASSERT_THROW(throw std::bad_alloc(), std::domain_error);
  }


  TEST(should_fail_assert_throw_with_no_exception)
  {
    int i;
    ASSERT_THROW(i=1, std::exception);
  }

  TEST(should_succeed_assert_no_throw)
  {
    int i;
    ASSERT_NO_THROW(i=1);
  }

  TEST(should_fail_assert_throw_any_with_no_exception)
  {
    int i;
    ASSERT_THROW(i=1, ...);
  }
  TEST(should_succeed_throw_any_with_int_exception)
  {
    ASSERT_THROW(throw 1, ...);
  }

  TEST(should_fail_assert_no_throw_with_unknown_exception)
  {
    ASSERT_NO_THROW(throw 1);
  }

  TEST(should_fail_assert_no_throw_with_std_exception_string_apa)
  {
    ASSERT_NO_THROW(throw std::range_error("apa"));
  }

  TEST(should_succeed_on_assert_eq_with_fixture, fixture<3>)
    {
      ASSERT_EQ(num, 3);
    }

  TEST(should_fail_on_assert_eq_with_fixture, fixture<4>)
    {
      ASSERT_EQ(num, 3);
    }

  TEST(should_succeed_on_assert_ne_with_fixture, fixture<4>)
    {
      ASSERT_NE(num, 3);
    }

  TEST(should_fail_on_assert_ne_with_fixture, fixture<3>)
    {
      ASSERT_NE(num, 3);
    }

  TEST(should_succeed_on_assert_gt_with_fixture, fixture<4>)
    {
      ASSERT_GT(num, 3);
    }

  TEST(should_fail_on_assert_gt_with_fixture, fixture<3>)
    {
      ASSERT_GT(num, 3);
    }

  TEST(should_succeed_on_assert_ge_with_fixture, fixture<3>)
    {
      ASSERT_GE(num, 3);
    }

  TEST(should_fail_on_assert_ge_with_fixture, fixture<2>)
    {
      ASSERT_GE(num, 3);
    }

  TEST(should_succeed_on_assert_lt_with_fixture, fixture<2>)
    {
      ASSERT_LT(num, 3);
    }

  TEST(should_fail_on_assert_lt_with_fixture, fixture<3>)
    {
      ASSERT_LT(num, 3);
    }

  TEST(should_succeed_on_assert_le_with_fixture, fixture<3>)
    {
      ASSERT_LE(num, 3);
    }

  TEST(should_fail_on_assert_le_with_fixture, fixture<4>)
    {
      ASSERT_LE(num, 3);
    }

  TEST(should_succeed_on_assert_true_with_fixture, fixture<3>)
    {
      ASSERT_TRUE(num);
    }

  TEST(should_fail_on_assert_true_with_fixture, fixture<0>)
    {
      ASSERT_TRUE(num);
    }

  TEST(should_succeed_on_assert_false_with_fixture, fixture<0>)
    {
      ASSERT_FALSE(num);
    }

  TEST(should_fail_on_assert_false_with_fixture, fixture<3>)
    {
      ASSERT_FALSE(num);
    }

  template <typename T>
    class unstreamable
  {
    class nieu;
  public:
    unstreamable(T t) : data(t) {}
    unstreamable& operator=(const T& t) { data = t; return *this; }
    operator T&() { return data; }
    operator const T&() const { return data; }
    bool operator!() const { return !data; }
    operator const nieu*() const
    {
      return data ? reinterpret_cast<const nieu*>(&data) : 0;
    }
  private:
    T data;
  };

  TEST(should_fail_on_assert_gt_with_unstreamable_param_i, fixture<3>)
    {
      unstreamable<int> i(3);
      ASSERT_GT(i, num);
    }

}

DISABLED_TEST(should_never_run, fixture<3>)
{
  ASSERT_FALSE(num);
}


TESTSUITE(depends)
{
  TEST(should_succeed_after_success_dependencies,
       DEPENDS_ON(default_success,
                  asserts::should_succeed_on_assert_eq_with_fixture,
                  signal_deaths::should_succeed_with_death_on_signal_11,
                  exit_deaths::should_succeed_with_exit_code_3,
                  exception_deaths::should_succeed_with_range_error_thrown,
                  very_slow_success))
    {
    }

  TEST(should_not_run_due_to_one_failed_dependency_success_otherwise,
       DEPENDS_ON(default_success,
                  exit_deaths::should_succeed_with_exit_code_3,
                  exception_deaths::should_fail_due_to_unknown_exception,
                  asserts::should_succeed_on_assert_eq_with_fixture))
    {
    }
}

TESTSUITE(timeouts)
{
  TEST(should_succeed_slow_realtime_deadline,
       DEADLINE_REALTIME_MS(1500),
       NO_CORE_FILE)
    {
      sleep(1);
    }
  TEST(should_fail_slow_realtime_deadline,
       DEADLINE_REALTIME_MS(500),
       NO_CORE_FILE)
    {
      sleep(1);
    }

  TEST(should_succeed_slow_cputime_deadline, DEADLINE_CPU_MS(500), NO_CORE_FILE)
    {
      sleep(3);
    }

  TEST(should_fail_slow_cputime_deadline, DEADLINE_CPU_MS(500), NO_CORE_FILE)
    {
      struct timespec deadline;
      clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &deadline);
      deadline.tv_sec+=1;
      for (;;)
        {
          struct timespec now;
          clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now);
          if (now.tv_sec > deadline.tv_sec) break;
          if (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec)
            break;
        }
    }

  TEST(should_fail_slow_cputime_deadline_by_death,
       DEADLINE_CPU_MS(500),
       NO_CORE_FILE)
    {
      for (;;)
        {
        }
    }

  TEST(should_fail_slow_realtime_deadline_by_death,
       DEADLINE_REALTIME_MS(500),
       NO_CORE_FILE)
    {
      sleep(3);
    }
}
TESTSUITE(stderr_and_stdout)
{
  TEST(should_succeed_with_stdout)
  {
    std::cout << "hello";
  }

  TEST(should_succeed_with_stderr)
  {
    std::cerr << "hello";
  }

  TEST(should_fail_with_death_due_to_assert_on_stderr, NO_CORE_FILE)
  {
    assert("apa" == 0);
  }

  TEST(should_fail_with_death_and_left_behind_core_dump, EXPECT_SIGNAL_DEATH(SIGABRT))
  {
    assert("apa" == 0);
  }
}

TEST(should_fail_due_to_left_behind_files)
{
  std::ofstream of("apa");
  of << "katt";
}
TEST(should_not_run_due_to_failed_left_behind_files_success_otherwise,
     DEPENDS_ON(should_fail_due_to_left_behind_files))
{
}

TESTSUITE(parametrized)
{
  class ptest : DEPENDS_ON(very_slow_success)
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

int main(int argc, const char *argv[])
{
  return crpcut::test_case_factory::run_test(argc, argv);
}
