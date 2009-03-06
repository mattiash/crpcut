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

TESTSUITE(ext_parameters)
{
  TEST(should_succeed_expected_value)
  {
    const char *p = crpcut::test_case_factory::get_parameter("apa");
    INFO << p;
    ASSERT_EQ(p, std::string("katt"));
  }

  TEST(should_succeed_no_value)
  {
    const char *p = crpcut::test_case_factory::get_parameter("orm");
    ASSERT_EQ(p, 0);
  }

  TEST(should_succeed_value_interpret)
  {
    std::string s(crpcut::test_case_factory::get_parameter<std::string>("apa"));
    ASSERT_EQ(s, "katt");
  }

  TEST(should_fail_value_interpret)
  {
    int i = crpcut::test_case_factory::get_parameter<int>("apa");
  }

  TEST(should_fail_no_value_interpret)
  {
    crpcut::test_case_factory::get_parameter<std::string>("orm");
  }
}
