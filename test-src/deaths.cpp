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
extern "C" {
#include <signal.h> // raise
}
TESTSUITE(death)
{
  TESTSUITE(by_exit)
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

  TESTSUITE(by_signal)
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

  TESTSUITE(by_exception)
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
}
