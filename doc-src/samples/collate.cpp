/*
 * Copyright 2009 Bjorn Fahller <bjorn@fahller.se>
 * All rights reserved
 *
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

char str1_8859_1[] = { 0xc3, 0xb6, 'z', 0 };
char str2_8859_1[] = { 'z', 0xc3, 0xb6, 0};


TEST(in_german_locale)
{
  ASSERT_PRED(crpcut::collate(str1_8859_1, std::locale("de_DE.utf8")) < str2_8859_1);
  ASSERT_PRED(crpcut::collate(str1_8859_1, std::locale("de_DE.utf8")) > str2_8859_1);
}

TEST(in_swedish_locale)
{
  ASSERT_PRED(crpcut::collate(str1_8859_1, std::locale("sv_SE.utf8")) < str2_8859_1);
  ASSERT_PRED(crpcut::collate(str1_8859_1, std::locale("sv_SE.utf8")) > str2_8859_1);
}

int main(int argc, char *argv[])
{
  return crpcut::run(argc, argv);
}
