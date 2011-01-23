/*
 * Copyright 2011 Bjorn Fahller <bjorn@fahller.se>
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
#include <string>

TEST(heap_in_balance)
{
  std::string before("a");
  VERIFY_SCOPE_HEAP_LEAK_FREE
  {
    std::string after("b");
  }
}

TEST(heap_imbalance)
{
  VERIFY_SCOPE_HEAP_LEAK_FREE     // no leak reported here
  {
    std::string before("a");
    VERIFY_SCOPE_HEAP_LEAK_FREE
    {                             // This is not a memory leak, but it is
      std::string after("b");     // reported as such because the object
      std::swap(before, after);   // allocated here is not released at the
    }                             // end of the block
  }
}

TEST(heap_leak)
{
  char *p[5];
  VERIFY_SCOPE_HEAP_LEAK_FREE
  {
    int i;
    for (i = 0; i < 5; ++i)
      {
        p[i] = new char[i+1];
      }
    while (--i > 0)
      {
        delete[] p[i];
      }
  }
}
int main(int argc, char *argv[])
{
  return crpcut::run(argc, argv);
}
