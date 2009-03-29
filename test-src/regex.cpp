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

TESTSUITE(regex)
{
  TEST(should_succeed_simple_re)
  {
    ASSERT_PRED(crpcut::match<crpcut::regex>("apa"), "apa");
  }
  TEST(should_fail_illegal_re)
  {
    ASSERT_PRED(crpcut::match<crpcut::regex>("[a"), "apa");
  }
  TEST(should_fail_no_match)
  {
    ASSERT_PRED(crpcut::match<crpcut::regex>("apa"), "katt");
  }
  TEST(should_fail_case_mismatch)
  {
    ASSERT_PRED(crpcut::match<crpcut::regex>("apa"), "APA");
  }
  TEST(should_succeed_case_mismatch)
  {
    ASSERT_PRED(crpcut::match<crpcut::regex>("apa", crpcut::regex::i), "APA");
  }
  TEST(should_fail_ere_paren_on_non_e_re)
  {
    ASSERT_PRED(crpcut::match<crpcut::regex>("apa(katt)*tupp"),
                "apakattkattkatttupp");
  }
  TEST(should_succeed_ere_paren_on_e_re)
  {
    ASSERT_PRED(crpcut::match<crpcut::regex>("apa(katt)*tupp",
                                             crpcut::regex::e),
                "apakattkattkatttupp");
  }
  TEST(should_succeed_non_ere_paren_on_non_e_re)
  {
    ASSERT_PRED(crpcut::match<crpcut::regex>("apa\\(katt\\)*tupp"),
                "apakattkattkatttupp");
  }
  TEST(should_fail_non_ere_paren_on_e_re)
  {
    ASSERT_PRED(crpcut::match<crpcut::regex>("apa\\(katt\\)*tupp",
                                             crpcut::regex::e),
                "apakattkattkatttupp");
  }
  TEST(should_succeed_paren_litteral_e_re)
  {
    ASSERT_PRED(crpcut::match<crpcut::regex>("apa\\(", crpcut::regex::e),
                "apa(");
  }
  TEST(should_succeed_paren_litteral_non_e_re)
  {
    ASSERT_PRED(crpcut::match<crpcut::regex>("apa("),
                "apa(");
  }
  TEST(should_fail_ere_on_non_e_re)
  {
    ASSERT_PRED(crpcut::match<crpcut::regex>("apa+"),
                "apaaa");
  }
  TEST(should_succeed_ere_on_e_re)
  {
    using crpcut::match;
    using crpcut::regex;
    ASSERT_PRED(match<regex>("apa+", regex::e),
                "apaaa");
  }
}
