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
    Mutex g_linked_ptr_mutex;

    const char * String::CloneCString(const char *p)
    {
      if (!p) return 0;
      size_t len = std::strlen(p);
      char *rv = new char[len + 1];
      std::strcpy(rv, p);
      return rv;
    }

    const String &String::operator=(const char * p)
    {
      if (p != c_str_)
        {
          const char *pc = CloneCString(p);
          delete[] c_str_;
          c_str_ = pc;
        }
      return *this;
    }

    String String::Format(const char *format, ...)
    {
      va_list args;
      va_start(args, format);

      size_t len = std::vsnprintf("", 0, format, args);
      char *array = static_cast<char*>(::alloca(len + 1));
      std::vsnprintf(array, len + 1, format, args);
      va_end(args);
      return String(array);
    }

    String StrStreamToString(StrStream *s)
    {
      const std::string &str = s->str();
      const char *const begin = str.c_str();
      const char *const end = begin + str.length();

      std::ostringstream os;
      for (const char *p = begin; p != end; ++p)
        {
          if (*p == 0)
            {
              os << "\\0";
            }
          else
            {
              os << *p;
            }
        }
      return os.str();
    }

    AssertHelper::AssertHelper(TestPartResultType type, const char* file,
                               int line, const char* message)
      : type_(type),
        file_(file),
        line_(line),
        message_(message)
    {
    }

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

    String GetCurrentOsStackTraceExceptTop(UnitTest *, int)
    {
      return "";
    }
    bool String::CStringEquals(const char *lh, const char *rh)
    {
      return lh == rh || (std::strcmp(lh, rh) == 0);
    }

    String String::ShowWideCString(const wchar_t * wstr)
    {
      if (!wstr) return "(null)";
      std::ostringstream os;
      for (;*wstr; ++wstr)
        {
          os << static_cast<char>(*wstr & 0xff);
        }
      return os.str();
    }

  }

  UnitTest* UnitTest::GetInstance() { static UnitTest obj; return &obj; }
  UnitTest::UnitTest() {}
  UnitTest::~UnitTest() { }
}
