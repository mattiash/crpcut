#include <gmock/gmock.h>
#include <crpcut.hpp>

TESTSUITE(google_mock)
{
  class iface_base
  {
  public:
    virtual void func(int n) = 0;
  };

  class iface : public iface_base
  {
  public:
    MOCK_METHOD1(func, void(int));
  };

  struct mock_fix
  {
    iface obj;
  };

  TEST(basic_success, mock_fix)
  {
    EXPECT_CALL(obj, func(3)).Times(1);
    obj.func(3);
  }

  TEST(fail_by_calling_with_wrong_value, mock_fix)
  {
    EXPECT_CALL(obj, func(3)).Times(1);
    obj.func(4);
  }

  TEST(fail_by_calling_too_often, mock_fix)
  {
    EXPECT_CALL(obj, func(3)).Times(1);
    obj.func(3);
    obj.func(3);
  }

  TEST(fail_by_not_calling, mock_fix)
  {
    EXPECT_CALL(obj, func(3)).Times(1);
  }

  struct seq_fix
  {
    testing::Sequence s1, s2;
    iface o1, o2;

    seq_fix()
    {
      EXPECT_CALL(o1, func(3))
        .InSequence(s1, s2);
      EXPECT_CALL(o2, func(4))
        .InSequence(s1);
      EXPECT_CALL(o1, func(1))
        .InSequence(s2);
      EXPECT_CALL(o1, func(0))
        .InSequence(s1, s2);
    }
  };

  TEST(sequence_success_1, seq_fix)
  {
    o1.func(3);
    o1.func(1);
    o2.func(4);
    o1.func(0);
  }

  TEST(sequence_success_2, seq_fix)
  {
    o1.func(3);
    o2.func(4);
    o1.func(1);
    o1.func(0);
  }

  TEST(sequence_fail_incomplete, seq_fix)
  {
    o1.func(3);
    o2.func(4);
    o1.func(1);
  }

  TEST(sequence_fail_one_too_many, seq_fix)
  {
    o1.func(3);
    o2.func(4);
    o1.func(1);
    o1.func(0);
    o1.func(0);
  }

  TEST(sequence_fail_one_wrong_value, seq_fix)
  {
    o1.func(3);
    o2.func(4);
    o1.func(1);
    o1.func(4);
  }
}
