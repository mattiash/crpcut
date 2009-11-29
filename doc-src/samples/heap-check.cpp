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

#include "stream-cast.hpp"
#include <crpcut.hpp>

class leak_detect
{
protected:
  leak_detect() : pre(crpcut::heap::allocated_bytes()) {}
  ~leak_detect()
  {
    size_t post = crpcut::heap::allocated_bytes();
    if (post != pre)
      {
        FAIL << "Memory leak detected\n"
             << pre << " Bytes allocated at construction time\n"
             << post << " Bytes allocated at destruction time";
      }
  }
private:
  size_t pre;
};

TEST(simple_string_to_int, leak_detect)
{
  const char s[] = "123";
  int n = stream_cast(s);
  ASSERT_EQ(n, 123);
}

TEST(constrained_string_to_int, leak_detect)
{
  crpcut::heap::set_limit(crpcut::heap::allocated_bytes() + 10);
  const char s[] = "1234567";
  int n = stream_cast(s);
  ASSERT_EQ(1234567, n);
}

int main(int argc, char *argv[])
{
  return crpcut::run(argc, argv);
}
