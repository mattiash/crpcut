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

#include <gmock/gmock.h>
#include <crpcut.hpp>
#include "posix_encapsulation.hpp"
#include <cstdio>

extern "C"
{
#include <stdarg.h>
#include <unistd.h>
}

namespace testing
{
  void InitGoogleTest(int*, char **) {}
  void InitGoogleTest(int*, wchar_t **) {}
  namespace internal
  {

    void AssertHelper::operator=(const Message &msg) const
    {
      if (type_ == TPRT_SUCCESS) return;

      std::ostringstream os;
      if (file_)
        {
          os << file_ << ':' << line_ << '\n';
        }
      os << message_ << msg;
      crpcut::comm::report(crpcut::comm::exit_fail, os);
    }
  }
  UnitTest* UnitTest::GetInstance() { static UnitTest obj; return &obj; }
  UnitTest::UnitTest() {}
  UnitTest::~UnitTest() { }
}
namespace crpcut {
  you_must_link_with_libcrpcut_gmock::you_must_link_with_libcrpcut_gmock()
  {
  }
}
