#include <string>
#include "ciut.hpp"
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

TEST(should_fail_with_exit_code_3)
{
  exit(3);
}

TEST(should_dump_core_due_to_death_on_signal_11)
{
  raise(11);
}

TEST(should_fail_due_to_unknown_exception)
{
  throw 1;
}

TEST(should_fail_due_to_std_exception_with_string_apa)
{
  throw std::range_error("apa");
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
TEST(should_succeed_with_range_error_thrown, EXPECT_EXCEPTION(std::range_error))
{
  throw std::range_error("apa");
}

TEST(should_fail_with_wrong_exception, EXPECT_EXCEPTION(std::range_error))
{
  throw std::bad_alloc();
}
TEST(should_fail_with_no_exception, EXPECT_EXCEPTION(std::exception))
{
}
TESTSUITE(apa)
{
  TEST(should_succeed_assert_throw_with_correct_exception)
  {
    ASSERT_THROW(throw std::bad_alloc(), std::bad_alloc);
  }

  TEST(should_fail_assert_exception_with_wrong_exception)
  {
    ASSERT_THROW(throw std::bad_alloc(), std::domain_error);
  }
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
TEST(should_succeed_on_asert_gt_with_fixture, fixture<4>)
{
  ASSERT_GT(num, 3);
}
TEST(should_fail_on_assert_gt_with_fixture, fixture<3>)
{
  ASSERT_GT(num, 3);
}
TEST(should_succeed_on_asert_ge_with_fixture, fixture<3>)
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

DISABLED_TEST(should_never_run, fixture<3>)
{
  ASSERT_FALSE(num);
}
int main(int argc, const char *argv[])
{
  return ciut::test_case_factory::run_test(argc, argv);
}
