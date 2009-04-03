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

#ifndef OUTPUT_HPP
#define OUTPUT_HPP

namespace crpcut
{
  namespace output
  {
    class formatter
    {
    public:
      typedef enum { escaped, verbatim } type;
      virtual void begin_case(const std::string &name, bool result) = 0;
      virtual void end_case()  = 0;
      virtual void terminate(test_phase phase,
                             const std::string &msg,
                             const char *dirname = 0) = 0;
      virtual void print(const std::string &tag, const std::string &data) = 0;
      virtual void statistics(unsigned num_registered,
                              unsigned num_run,
                              unsigned num_failed) = 0;
      virtual void nonempty_dir(const std::string &)  = 0;
      virtual void blocked_test(const implementation::test_case_registrator *)  = 0;
      virtual ~formatter() {} // keeps compilers happy. Not needed for this use.
    protected:
      formatter(int fd) : fd_(fd) {}
      size_t write(const std::string &s, type t = verbatim) const
      {
        return write(s.c_str(), s.size(), t);
      }
      template <size_t N>
      size_t write(const char (&str)[N], type t = verbatim) const
      {
        return write(&str[0], N - 1, t);
      }
      size_t write(const char *str, size_t len, type t = verbatim) const;
    private:
      size_t do_write(const char *p, size_t len) const;
      int fd_;
    };


    class xml_formatter : public formatter
    {
    public:
      xml_formatter(int fd, int argc_, const char *argv_[]);
      virtual ~xml_formatter();
      virtual void begin_case(const std::string &name, bool result);
      virtual void end_case();
      virtual void terminate(test_phase phase,
                             const std::string &msg,
                             const char *dirname = 0);
      virtual void print(const std::string &tag, const std::string &data);
      virtual void statistics(unsigned num_registered,
                              unsigned num_run,
                              unsigned num_failed);
      virtual void nonempty_dir(const std::string &s);
      virtual void blocked_test(const implementation::test_case_registrator*);
    private:
      void make_closed();

      bool                      last_closed;
      bool                      blocked_tests;
      bool                      statistics_printed;
      int                       argc;
      const char *const * const argv;
    };


    class text_formatter : public formatter
    {
    public:
      text_formatter(int fd, int, const char**) : formatter(fd) {}
      virtual void begin_case(const std::string &name, bool result);
      virtual void end_case();
      virtual void terminate(test_phase phase,
                             const std::string &msg,
                             const char *dirname = 0);
      virtual void print(const std::string &tag, const std::string &data);
      virtual void statistics(unsigned num_registered,
                              unsigned num_run,
                              unsigned num_failed);
      virtual void nonempty_dir(const std::string &s);
      virtual void blocked_test(const implementation::test_case_registrator *i);
    private:
      bool did_output;
      bool blocked_tests;
    };

  }
}
#endif // OUTPUT_HPP
