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
#include <crpcut.hpp>
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
  sleep(1);
}

TEST(should_fail_after_delay)
{
  sleep(1);
  exit(1);
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
                  very_slow_success))
  {
  }

  TEST(should_not_run_due_to_one_failed_dependency_success_otherwise,
       DEPENDS_ON(default_success,
                  asserts::should_succeed_on_assert_eq_with_fixture,
                  should_fail_after_delay))
  {
  }
}



TEST(should_fail_due_to_left_behind_files)
{
  std::ofstream of("apa");
  of << "katt";
}

TEST(should_succeed_reading_file_in_start_dir)
{
  std::string s(crpcut::test_case_factory::get_start_dir());
  s+= "/apafil";
  std::ifstream in(s.c_str());
  std::string content;
  in >> content;
  INFO << "in.rdstate()=" << std::hex << in.rdstate();
  ASSERT_EQ(content, "apa");
}

TEST(should_not_run_due_to_failed_left_behind_files_success_otherwise,
     DEPENDS_ON(should_fail_due_to_left_behind_files))
{
}


