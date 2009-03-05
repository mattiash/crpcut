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
#include "output.hpp"
#include "posix_encapsulation.hpp"

#define STR(s) { "\"" #s "\"", sizeof(#s) + 1 }

namespace {
  struct phase_tag
  {
    const char *str;
    size_t      len;
  };

  phase_tag phase_str[] = {
    STR(creating),
    STR(running),
    STR(destroying),
    STR(post_mortem)
  };
}




namespace crpcut
{
  namespace output
  {
    size_t formatter::write(const char *str, size_t len, type t) const
    {
      if (t == verbatim)
        {
          return do_write(str, len);
        }
      const char *prev = str;
      for (size_t n = 0; n < len; ++n)
        {
          const char *esc;
          size_t esc_len;
          switch (str[n])
            {
            case '<' : esc = "&lt;"; esc_len = 4; break;
            case '>' : esc = "&gt;"; esc_len = 4; break;
            case '&' : esc = "&amp;"; esc_len = 5; break;
            case '"' : esc = "&quot;"; esc_len = 6; break;
            case '\'': esc = "&apos;"; esc_len = 6; break;
            case '\0': esc = 0; esc_len = 0; break;
            default: continue;
            }
          do_write(prev, &str[n] - prev);
          do_write(esc, esc_len);
          prev = &str[n] + 1;
        }
      do_write(prev, str + len - prev);
      return len;
    }

    size_t formatter::do_write(const char *p, size_t len) const
    {
      for (size_t bytes_written = 0; bytes_written < len;)
        {
          int rv = wrapped::write(fd, p, len);
          assert(rv >= 0);
          bytes_written += rv;
        }
    }

    xml_formatter::xml_formatter(int fd, int argc_, const char *argv_[])
      : formatter(fd),
        last_closed(false),
        blocked_tests(false),
        statistics_printed(false),
        argc(argc_),
        argv(argv_)
    {
      write("<?xml version=\"1.0\"?>\n\n"
            "<crpcut xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
            " xsi:noNamespaceSchemaLocation=\"crpcut.xsd\""
            " starttime=\"");

      char time_string[] = "2009-01-09T23:59:59Z";
      time_t now = wrapped::time(0);
      struct tm *tmdata = wrapped::gmtime(&now);
      int len = wrapped::snprintf(time_string, sizeof(time_string),
                                  "%4.4d-%2.2d-%2.2dT%2.2d:%2.2d:%2.2dZ",
                                  tmdata->tm_year + 1900,
                                  tmdata->tm_mon + 1,
                                  tmdata->tm_mday,
                                  tmdata->tm_hour,
                                  tmdata->tm_min,
                                  tmdata->tm_sec);
      assert(len < sizeof(time_string));
      assert(time_string[len] == 0);
      write(time_string);

      char machine_string[PATH_MAX];
      if (wrapped::gethostname(machine_string, sizeof(machine_string)))
        {
          machine_string[0] = 0;
        }
      write("\" host=\"");
      write(machine_string, wrapped::strlen(machine_string), escaped);

      write("\" command=\"");
      for (int i = 0; i < argc; ++i)
        {
          if (i > 0) write(" ", 1);
          write(argv[i], wrapped::strlen(argv[i]), escaped);
        }
      write("\">\n");
    }


    xml_formatter::~xml_formatter()
    {
      if (statistics_printed)
        {
          if (blocked_tests)
            {
              write("  </blocked_tests>\n");
            }
          write("</crpcut>\n");
        }
    }

    void xml_formatter::begin_case(const std::string &name, bool result)
    {
      write("  <test name=\"");
      write(name, escaped);
      write("\" result=");
      static const char *rstring[] = { "\"FAILED\"", "\"OK\"" };
      write(rstring[result]);
      last_closed=false;
    }

    void xml_formatter::end_case()
    {
      if (last_closed)
        {
          write("    </log>\n  </test>\n");
        }
      else
        {
          write("/>\n");
        }
    }

    void xml_formatter::terminate(test_phase phase,
                                  const std::string &msg,
                                  const char *dirname)
    {
      make_closed();
      write("      <violation phase=");
      write(phase_str[phase].str, phase_str[phase].len);
      if (dirname)
        {
          write(" nonempty_dir=\"");
          write(dirname, wrapped::strlen(dirname), escaped);
          write("\"");
        }
      if (msg.size() == 0)
        {
          write("/>\n");
          return;
        }
      write(">");
      write(msg, escaped);
      write("</violation>\n");
    }

    void xml_formatter::print(const std::string &tag, const std::string &data)
    {
      make_closed();
      write("    <");
      write(tag);
      if (data.size() == 0)
        {
          write("/>\n");
          return;
        }
      write(">");
      write(data, escaped);
      write("</");
      write(tag);
      write(">\n");
    }

    void xml_formatter::statistics(unsigned num_registered,
                                   unsigned num_run,
                                   unsigned num_failed)
    {
      write("  <statistics>\n"
            "    <registered_test_cases>");
      {
        stream::toastream<10> o;
        o << num_registered;
        write(o.begin(), o.size());
      }
      write("</registered_test_cases>\n"
            "    <run_test_cases>");
      {
        stream::toastream<10> o;
        o << num_run;
        write(o.begin(), o.size());
      }
      write("</run_test_cases>\n"
            "    <failed_test_cases>");
      {
        stream::toastream<10> o;
        o << num_failed;
        write(o.begin(), o.size());
      }
      write("</failed_test_cases>\n"
            "  </statistics>\n");
      statistics_printed = true;
    }

    void xml_formatter::nonempty_dir(const std::string &s)
    {
      write("  <remaining_files nonempty_dir=\"");
      write(s);
      write("\"/>\n");
    }

    void
    xml_formatter::blocked_test(const implementation::test_case_registrator *i)
    {
      if (!blocked_tests)
        {
          write("  <blocked_tests>\n");
          blocked_tests = true;
        }
      std::ostringstream os;
      write("    <test name=\"");
      os << *i;
      write(os.str(), escaped);
      write("\"/>\n");
    }

    void xml_formatter::make_closed()
    {
      if (!last_closed)
        {
          write(">\n    <log>\n");
          last_closed = true;
        }
    }
  }
}

namespace {
  static const char barrier[] =
    "===============================================================================\n";
  static const char rlabel[2][9] = { "FAILED! ", "OK      " };
  static const char delim[]=
    "-------------------------------------------------------------------------------\n";
}

namespace crpcut {
  namespace output {

    void text_formatter::begin_case(const std::string &name, bool result)
    {
      did_output = false;
      write(rlabel[result], 8);
      write(name);
      write("\n");
    }

    void text_formatter::end_case()
    {
      write(barrier);
    }

    void text_formatter::terminate(test_phase         phase,
                                   const std::string &msg,
                                   const char        *dirname)
    {
      did_output = true;
      if (dirname)
        {
          write(dirname, wrapped::strlen(dirname));
          write(" is not empty!!\n");
        }
      if (!msg.empty())
        {
          write("phase=");
          write(phase_str[phase].str, phase_str[phase].len);
          write("  ");
          write(delim + 8 + phase_str[phase].len,
                sizeof(delim) - 8 - phase_str[phase].len - 1);
          write(msg);
          write("\n");
          write(delim);
        }
    }

    void text_formatter::print(const std::string &tag, const std::string &data)
    {
      did_output = true;
      const size_t len = write(tag);
      if (len < sizeof(delim))
        {
          write(delim + len, sizeof(delim) - len - 1);
        }
      write(data);
      write("\n");
    }

    void text_formatter::statistics(unsigned num_registered,
                                    unsigned num_run,
                                    unsigned num_failed)
    {
      {
        stream::toastream<10> o;
        o << num_registered;
        write(o.begin(), o.size());
      }
      write(" registered, ");
      {
        stream::toastream<10> o;
        o << num_run;
        write(o.begin(), o.size());
      }
      write(" run, ");
      {
        stream::toastream<10> o;
        o << num_run - num_failed;
        write(o.begin(), o.size());
      }
      write(" OK, ");
      {
        stream::toastream<10> o;
        o << num_failed;
        write(o.begin(), o.size());
      }
      write(" FAILED!\n");
    }

    void text_formatter::nonempty_dir(const std::string &s)
    {
      write("Files remain under ");
      write(s);
      write("\n");
    }

    void text_formatter::blocked_test(const implementation::test_case_registrator *i)
    {
      if (!blocked_tests)
        {
          write("The following tests were blocked from running:\n");
          blocked_tests = true;
        }
      std::ostringstream os;
      os << "  " << *i << '\n';
      write(os.str());
    }

  }
}
