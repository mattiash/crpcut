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
#include "poll.hpp"
#include "implementation.hpp"
#include "xml.hpp"

extern "C" {
#include <sys/wait.h>
}
#include <map>
#include <list>
#include <cstdio>
#include <limits>
#include <ctime>
#include <queue>
#include <fstream>
namespace crpcut {

  test_case_factory::test_case_factory()
    : pending_children(0),
      verbose_mode(false),
      nodeps(false),
      num_parallel(1),
      num_registered_tests(0),
      num_tests_run(0),
      num_successful_tests(0),
      first_free_working_dir(0)
  {
    std::strcpy(dirbase, "/tmp/crpcutXXXXXX");
    for (unsigned n = 0; n < max_parallel; ++n)
      {
        working_dirs[n] = n+1;
      }
  }

  void test_case_factory
  ::do_set_deadline(implementation::test_case_registrator *i)
  {
    assert(i->deadline_is_set());
    deadlines.push_back(i);
    std::push_heap(deadlines.begin(), deadlines.end(),
                   &implementation::test_case_registrator::timeout_compare);
  }

  void test_case_factory
  ::do_clear_deadline(implementation::test_case_registrator *i)
  {
    assert(i->deadline_is_set());
    for (size_t n = 0; n < deadlines.size(); ++n)
      {
        if (deadlines[n] == i)
          {
            using namespace implementation;
            for (;;)
              {
                size_t m = (n+1)*2-1;
                if (m >= deadlines.size() - 1) break;
                if (test_case_registrator::timeout_compare(deadlines[m],
                                                           deadlines[m+1]))
                  {
                    deadlines[n] = deadlines[m];
                  }
                else
                  {
                    deadlines[n] = deadlines[++m];
                  }
                n = m;
              }
            deadlines[n] = deadlines.back();
            deadlines.pop_back();
            return;
          }
      }
    assert("clear deadline when none was ordered" == 0);
  }

  void test_case_factory::do_return_dir(int num)
  {
    working_dirs[num] = first_free_working_dir;
    first_free_working_dir = num;
  }

  void test_case_factory::do_introduce_name(pid_t pid, const char *name, size_t len)
  {
    int pipe = presenter_pipe;
    for (;;)
      {
        int rv = ::write(pipe, &pid, sizeof(pid));
        if (rv == sizeof(pid)) break;
        assert (rv == -1 && errno == EINTR);
      }
    const comm::type t = comm::begin_test;
    for (;;)
      {
        int rv = ::write(pipe, &t, sizeof(t));
        if (rv == sizeof(t)) break;
        assert(rv == -1 && errno == EINTR);
      }

    for (;;)
      {
        int rv = ::write(pipe, &len, sizeof(len));
        if (rv == sizeof(len)) break;
        assert(rv == -1 && errno == EINTR);
      }
    for (;;)
      {
        int rv = ::write(pipe, name, len);
        if (size_t(rv) == len) break;
        assert(rv == -1 && errno == EINTR);
      }
  }

  void test_case_factory::do_present(pid_t pid,
                                     comm::type t,
                                     size_t len,
                                     const char *buff)
  {
    int pipe = presenter_pipe;
    int rv = ::write(pipe, &pid, sizeof(pid));
    assert(rv == sizeof(pid));
    rv = ::write(pipe, &t, sizeof(t));
    assert(rv == sizeof(t));
    rv = ::write(pipe, &len, sizeof(len));
    assert(rv == sizeof(len));
    if (len)
      {
        rv = ::write(pipe, buff, len);
        assert(size_t(rv) == len);
      }
  }

  namespace {
    struct test_case_result
    {
      bool                   success;
      bool                   nonempty_dir;
      std::string            name;
      std::string            termination;
      std::list<std::string> history;
    };


    class esc
    {
    public:
      esc(const std::string &an_s) : s(an_s) {}
      friend std::ostream &operator<<(std::ostream &os, const esc &e)
      {
        for (std::string::const_iterator i = e.s.begin(); i != e.s.end(); ++i)
          {
            switch (*i)
              {
              case '<': os << "&lt;"; break;
              case '>': os << "&gt;"; break;
              case '&': os << "&amp;"; break;
              case '"': os << "&quot;"; break;
              case '\'': os << "&apos;"; break;
              default: os << *i;
              }
          }
        return os;
      }
    private:
      const std::string &s;
    };

    int start_presenter_process(std::ostream &os, int verbose,
                                int argc, const char *argv[])
    {
      int fds[2];
      int rv = ::pipe(fds);
      assert(rv == 0);
      (void)rv; // silence warning
      pid_t pid = ::fork();
      if (pid != 0)
        {
          ::close(fds[0]);
          return fds[1];;
        }
      int presenter_pipe = fds[0];
      ::close(fds[1]);

      char time_string[] = "2009-01-09T23:59:59Z";
      time_t now = std::time(0);
      struct tm *tmdata = std::gmtime(&now);
      std::sprintf(time_string, "%4.4d-%2.2d-%2.2dT%2.2d:%2.2d:%2.2dZ",
                   tmdata->tm_year + 1900,
                   tmdata->tm_mon + 1,
                   tmdata->tm_mday,
                   tmdata->tm_hour,
                   tmdata->tm_min,
                   tmdata->tm_sec);
      char machine_string[PATH_MAX];
      ::gethostname(machine_string, sizeof(machine_string));
      os <<
        "<?xml version=\"1.0\"?>\n\n"
        "<crpcut xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
        " xsi:noNamespaceSchemaLocation=\"crpcut.xsd\""
        " starttime=\"" << time_string <<
        "\" host=\"" << machine_string <<
        "\" command=\"";
      for (const char **p = argv; *p; ++p)
        {
          if (p != argv) os << " ";
          os << *p;
        }
      os << "\">\n" << std::flush;

      std::map<pid_t, test_case_result> messages;
      for (;;)
        {
          pid_t test_case_id;
          int rv = ::read(presenter_pipe, &test_case_id, sizeof(test_case_id));
          if (rv == 0)
            {
              assert(messages.size() == 0);
              exit(0);
            }
          assert(rv == sizeof(test_case_id));
          test_case_result &s = messages[test_case_id];

          comm::type t;
          rv = ::read(presenter_pipe, &t, sizeof(t));
          assert(rv == sizeof(t));

          switch (t)
            {
            case comm::begin_test:
              {
                assert(s.name.length() == 0);
                assert(s.history.size() == 0);

                // introduction to test case, name follows

                size_t len = 0;
                char *p = static_cast<char*>(static_cast<void*>(&len));
                size_t bytes_read = 0;
                while (bytes_read < sizeof(len))
                  {
                    rv = ::read(presenter_pipe,
                                p + bytes_read,
                                sizeof(len) - bytes_read);
                    assert(rv > 0);
                    bytes_read += rv;
                  }
                char *buff = static_cast<char *>(::alloca(len));
                bytes_read = 0;
                while (bytes_read < len)
                  {
                    rv = ::read(presenter_pipe,
                                buff + bytes_read,
                                len - bytes_read);
                    assert(rv >= 0);
                    bytes_read += rv;
                  }
                s.name.assign(buff, len);
                s.success = true;
                s.nonempty_dir = false;
              }
              break;
            case comm::end_test:
              {
                size_t len;
                rv = ::read(presenter_pipe, &len, sizeof(len));
                assert(rv == sizeof(len));
                assert(len == 0);
              }
              if (!s.success || verbose)
                {
                  bool history_print = false;
                  os << "  <test name=\"" << s.name
                     << "\" result=\""
                     << (s.success ? "OK" : "FAILED") << '"';
                  for (std::list<std::string>::iterator i = s.history.begin();
                       i != s.history.end();
                       ++i)
                    {
                      bool prev_ended = true;
                      std::string &s = *i;
                      for (std::string::size_type prevpos = 0, endpos = 0;
                           ;
                           prevpos = endpos + 1)
                        {
                          if (!history_print)
                            {
                              os  << ">\n    <log>\n";
                              history_print = true;
                            }
                          endpos = s.find('\n', prevpos);
                          if (endpos == std::string::npos) break;
                          static const char *prefix[] = { "", "      " };
                          os << prefix[prev_ended]
                             << esc(std::string(s, prevpos, endpos - prevpos))
                             << "\n";
                          prev_ended = s[endpos-1] == '>';
                        }
                    }
                  if (!s.termination.empty() || s.nonempty_dir)
                    {
                      if (!history_print)
                        {
                          os << ">\n    <log>\n";
                      }
                      os << "      <termination";
                      if (s.nonempty_dir)
                        {
                          os << " nonempty_dir=\""
                             << test_case_factory::get_working_dir()
                             << '/'
                             << esc(s.name)
                             << '"';
                        }
                      if (s.termination.empty())
                        {
                          os << "/>\n";
                        }
                      else
                        {
                          os << '>' << esc(s.termination)
                             << "</termination>\n";
                        }
                      history_print = true;
                    }
                  if (history_print)
                    {
                      os << "    </log>\n  </test>\n";
                    }
                  else
                    {
                      os << "/>\n";
                    }
                }
              messages.erase(test_case_id);
              break;
            case comm::dir:
              {
                size_t len;
                rv = ::read(presenter_pipe, &len, sizeof(len));
                assert(rv == sizeof(len));
                assert(len == 0);
                (void)len; // silense warning
                s.success = false;
                s.nonempty_dir = true;
              }
              break;
            case comm::exit_ok:
            case comm::exit_fail:
              s.success &= t == comm::exit_ok; // fallthrough
            case comm::stdout:
            case comm::stderr:
            case comm::info:
              {
                size_t len;
                rv = ::read(presenter_pipe, &len, sizeof(len));
                assert(rv == sizeof(len));
                if (len)
                  {
                    char *buff = static_cast<char *>(alloca(len));
                    rv = ::read(presenter_pipe, buff, len);
                    assert(size_t(rv) == len);

                    if (t == comm::exit_ok || t == comm::exit_fail)
                      {
                        s.termination=std::string(buff, len);
                      }
                    else
                      {
                        std::ostringstream oss;
                        if (t == comm::stdout)
                          {
                            CRPCUT_XML_TAG(stdout, oss)
                              {
                                stdout << std::string(buff, len);
                              }
                          }
                        else if (t == comm::stderr)
                          {
                            CRPCUT_XML_TAG(stderr, oss)
                              {
                                stderr << std::string(buff, len);
                              }
                          }
                        else
                          {
                            CRPCUT_XML_TAG(info, oss)
                              {
                                info << std::string(buff, len);
                              }
                          }
                        s.history.push_back(oss.str());
                      }
                  }
              }
              break;
            default:
              assert("unreachable code reached" == 0);
            }
        }
      assert("unreachable code reached" == 0);
    }
  }

  void test_case_factory::kill_presenter_process()
  {
    ::close(presenter_pipe);
    ::siginfo_t info;
    for (;;)
      {
        int rv = ::waitid(P_ALL, 0, &info, WEXITED);
        if (rv == -1 && errno == EINTR) continue;
        assert(rv == 0);
        break;
      }
  }

  void test_case_factory
  ::run_test_case(implementation::test_case_registrator *i) const
  {
    test_case_base *p = 0;
    const char *msg = 0;
    const char *type = 0;
    try {
      p = (i->instantiate_obj());
    }
    catch (std::exception &e)
      {
        type = "std::exception";
        msg = e.what();
      }
    catch (...)
      {
        type = "...";
      }
    if (type)
      {
        std::ostringstream out;
        out << "Fixture contructor threw " << type;
        if (msg)
          {
            out << "\n  what()=" << msg;
          }
        report(comm::exit_fail, out);
      }
    // report start of test
    try {
      p->run();
    }
    catch (std::exception &e)
      {
        const size_t len = std::strlen(e.what());
#define TEMPLATE_HEAD "Unexpectedly caught std::exception\n  what()="
        const size_t head_size = sizeof(TEMPLATE_HEAD) - 1;
        char *msg = static_cast<char *>(alloca(head_size + len + 1));
        std::strcpy(msg, TEMPLATE_HEAD);
        std::strcpy(msg + head_size, e.what());
#undef TEMPLATE_HEAD
        report(comm::exit_fail, head_size + len, msg);
      }
    catch (...)
      {
        static const char msg[] = "Unexpectedly caught ...";
        report(comm::exit_fail, msg);
      }
    // report end of test
    if (tests_as_child_procs())
      {
        p->~test_case_base(); // Ugly, but since report kills when parallel
      }                       // it takes care of a memory leak.
    else
      {
        i->register_success();
      }
    report(comm::exit_ok, 0, 0);
  }

  void test_case_factory::manage_children(unsigned max_pending_children)
  {
    while (pending_children >= max_pending_children)
      {
        using namespace implementation;

        int timeout_ms = deadlines.size()
          ? deadlines.front()->ms_until_deadline()
          : -1;
        polltype::descriptor desc = poller.wait(timeout_ms);

        if (desc.timeout())
          {
            assert(deadlines.size());
            deadlines.front()->kill();
            std::pop_heap(deadlines.begin(), deadlines.end());
            deadlines.pop_back();
            continue;
          }
        bool read_failed = false;
        if (desc.read())
          {
            read_failed = !desc->read();
          }
        if (read_failed || desc.hup())
          {
            desc->unregister();
            test_case_registrator *r = desc->get_registrator();
            if (!r->has_active_readers())
              {
                r->manage_death();
                ++num_tests_run;
                --pending_children;
              }
          }
      }
  }

  void test_case_factory::start_test(implementation::test_case_registrator *i)
  {
    if (!tests_as_child_procs())
      {
        std::cout << "  <test name=\"" << *i;
        run_test_case(i);
        ++num_tests_run;
        std::cout << " result=\"OK\"/>\n";
        return;
      }

    int c2p[2];
    ::pipe(c2p);
    int p2c[2];
    ::pipe(p2c);
    int stderr[2];
    ::pipe(stderr);
    int stdout[2];
    ::pipe(stdout);

    int wd = first_free_working_dir;
    first_free_working_dir = working_dirs[wd];
    i->set_wd(wd);

    ::pid_t pid;
    for (;;)
      {
        pid = ::fork();
        if (pid >= 0) break;
        assert(errno == EINTR);
      }
    if (pid < 0) return;
    if (pid == 0) // child
      {
        comm::report.set_fds(p2c[0], c2p[1]);
        ::dup2(stdout[1], 1);
        ::dup2(stderr[1], 2);
        ::close(stdout[0]);
        ::close(stderr[0]);
        ::close(stdout[1]);
        ::close(stderr[1]);
        ::close(p2c[1]);
        ::close(c2p[0]);
        i->goto_wd();
        run_test_case(i);
        // never executed!
        assert("unreachable code reached" == 0);
      }

    // parent
    ++pending_children;
    i->setup(pid, c2p[0], p2c[1], stdout[0], stderr[0]);
    ::close(c2p[1]);
    ::close(p2c[0]);
    ::close(stdout[1]);
    ::close(stderr[1]);
    manage_children(num_parallel);
  }

  unsigned test_case_factory::do_run(int argc, const char *argv[],
                                     std::ostream &err_os)
  {
    const char *working_dir = 0;
    bool quiet = false;
    std::ostream *output = &std::cout;
    const char **p = argv+1;
    while (const char *param = *p)
      {
        if (param[0] != '-') break;
        switch (param[1]) {
        case 'q':
          quiet = true;
          break;
        case 'o':
          {
            ++p;
            static std::ofstream of(*p);
            if (!of)
              {
                err_os << "Failed to open " << *p << " for writing\n";
                return -1;
              }
            output = &of;
          }
          break;
        case 'v':
          verbose_mode = true;
          break;
        case 'c':
          ++p;
          {
            stream::iastream i(*p);
            unsigned l;
            if (!(i >> l) || l > max_parallel)
              {
                err_os <<
                  "num child processes must be a positive integer no greater than "
                   << max_parallel
                   << "\nA value of 0 means test cases are executed in the parent process"
                  "\n";
                return -1;
              }
            num_parallel = l;
          }
          break;
        case 'l':
          {
            const char **names = ++p;
            if (*names && **names == '-')
              {
                err_os <<
                  "-l must be followed by a (possibly empty) test case list\n";
                return -1;
              }
            for (implementation::test_case_registrator *i = reg.get_next();
                 i != &reg;
                 i = i->get_next())
              {
                bool matched = !*names;
                for (const char **name = names; !matched && *name; ++name)
                  {
                    matched = i->match_name(*name);
                  }
                if (matched) std::cout << *i << '\n';
              }
            return 0;
          }
        case 'd':
          working_dir = *++p;
          if (!working_dir)
            {
              err_os << "-d must be followed by a directory name\n";
              return -1;
            }
          strcpy(dirbase, working_dir);
          break;
        case 'n':
          nodeps = true;
          break;
        default:
          err_os <<
            "Usage: " << argv[0] << " [flags] {testcases}\n"
            "  where flags can be:\n"
            "    -l           - list test cases\n"
            "    -n           - ignore dependencies\n"
            "    -d dir       - specify workind directory (must exist)\n"
            "    -v           - verbose mode\n"
            "    -c number    - Control number of spawned test case processes\n"
            "                   if 0 the tests are run in the parent process\n"
            "    -o file      - Direct xml output to named file. Brief result\n"
            "                   will be displayed on stdout\n"
            "    -q           - Don't display the -o brief\n";

          return -1;
        }
        ++p;
      }

    if (tests_as_child_procs())
      {
        if (!working_dir && !::mkdtemp(dirbase))
          {
            err_os << argv[0] << ": failed to create working directory\n";
            return 1;
          }
        if (::chdir(dirbase) != 0)
          {
            err_os << argv[0] << ": couldn't move to working directoryy\n";
            ::rmdir(dirbase);
            return 1;
          }
      }
    if (tests_as_child_procs())
      {
        presenter_pipe = start_presenter_process(*output, verbose_mode,
                                                 argc, argv);
      }
    const char **names = p;
    for (;;)
      {
        bool progress = false;
        implementation::test_case_registrator *i = reg.get_next();
        unsigned count = 0;
        while (i != &reg)
          {
            ++count;
            if (*names)
              {
                bool found = false;
                for (const char **name = names; *name; ++name)
                  {
                    if ((found = i->match_name(*name))) break;
                  }
                if (!found)
                  {
                    progress = true;
                    i = i->unlink();
                    continue;
                  }
              }
            if (!nodeps && !i->can_run())
              {
                i = i->get_next();
                continue;
              }
            start_test(i);
            progress = true;
            i = i->unlink();
          }
        if (!progress)
          {
            if (pending_children == 0)
              {
                break;
              }
            manage_children(1);
          }
        if (count > num_registered_tests)
          {
            num_registered_tests = count;
          }
      }
    if (pending_children) manage_children(1);

    if (tests_as_child_procs())
      {
        kill_presenter_process();
        *output <<
          "  <statistics>\n"
          "    <registered_test_cases>"
                  << num_registered_tests
                  << "</registered_test_cases>\n"
          "    <run_test_cases>" << num_tests_run << "</run_test_cases>\n"
          "    <failed_test_cases>"
                  << num_tests_run - num_successful_tests
                  << "</failed_test_cases>\n"
          "  </statistics>\n";
        if (output != &std::cout && !quiet)
          {
            std::cout << num_registered_tests << " registered, "
                      << num_tests_run << " run, "
                      << num_successful_tests << " OK, "
                      << num_tests_run - num_successful_tests << " FAILED!\n";
          }
        for (unsigned n = 0; n < max_parallel; ++n)
          {
            char name[std::numeric_limits<unsigned>::digits/3+2];
            int len = snprintf(name, sizeof(name), "%u", n);
            assert(len > 0 && len < int(sizeof(name)));
            (void)len; // silence warning
            ::rmdir(name);
          }

        if (!implementation::is_dir_empty("."))
          {
            *output << "  <remaining_files nonempty_dir=\""
                    << dirbase
                    << "\"/>\n";
            if (output != &std::cout && !quiet)
              {
                std::cout << "Files remain in " << dirbase << '\n';
              }
          }
        else if (working_dir == 0)
          {
            ::chdir("..");
            ::rmdir(dirbase);
          }
      }

    if (reg.get_next() != &reg)
      {
        *output << "  <blocked_tests>\n";
        if (output != &std::cout && !quiet)
          {
            std::cout << "Blocked tests:\n";
          }
        for (implementation::test_case_registrator *i = reg.get_next();
             i != &reg;
             i = i->get_next())
          {
           *output << "    <test name=\"" << *i << "\"/>\n";
           if (output != &std::cout && !quiet) std::cout << "  " << *i << '\n';
          }
        *output << "  </blocked_tests>\n";
      }

    *output << "</crpcut>\n";
    return num_tests_run - num_successful_tests;
  }

} // namespace crpcut

crpcut::implementation::namespace_info current_namespace(0,0);
