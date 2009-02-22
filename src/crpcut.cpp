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
#include "output.hpp"
extern "C" {
#include <sys/wait.h>
#include <fcntl.h>
}
#include <map>
#include <list>
#include <cstdio>
#include <limits>
#include <ctime>
#include <queue>
#include <fstream>
#include "posix_encapsulation.hpp"
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
    wrapped::strcpy(dirbase, "/tmp/crpcutXXXXXX");
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
        int rv = wrapped::write(pipe, &pid, sizeof(pid));
        if (rv == sizeof(pid)) break;
        assert (rv == -1 && errno == EINTR);
      }
    const comm::type t = comm::begin_test;
    for (;;)
      {
        int rv = wrapped::write(pipe, &t, sizeof(t));
        if (rv == sizeof(t)) break;
        assert(rv == -1 && errno == EINTR);
      }

    for (;;)
      {
        int rv = wrapped::write(pipe, &len, sizeof(len));
        if (rv == sizeof(len)) break;
        assert(rv == -1 && errno == EINTR);
      }
    for (;;)
      {
        int rv = wrapped::write(pipe, name, len);
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
    int rv = wrapped::write(pipe, &pid, sizeof(pid));
    assert(rv == sizeof(pid));
    rv = wrapped::write(pipe, &t, sizeof(t));
    assert(rv == sizeof(t));
    rv = wrapped::write(pipe, &len, sizeof(len));
    assert(rv == sizeof(len));
    if (len)
      {
        rv = wrapped::write(pipe, buff, len);
        assert(size_t(rv) == len);
      }
  }

  namespace {
    struct event
    {
      std::string tag;
      std::string body;
    };
    struct test_case_result
    {
      bool             success;
      bool             nonempty_dir;
      std::string      name;
      std::string      termination;
      std::list<event> history;
    };



    class printer
    {
    public:
      printer(output::formatter& o_, const std::string &name, bool result)
        : o(o_)
      {
        o.begin_case(name, result);
      }
      ~printer() { o.end_case(); }
      void terminate(const std::string &msg, const char *dirname = 0)
      {
        o.terminate(msg, dirname);
      }
      void print(const char *tag, const std::string &data)
      {
        o.print(tag, data);
      }
    private:
      output::formatter &o;
    };

    class pipe_pair
    {
    public:
      typedef enum { release_ownership, keep_ownership } purpose;
      pipe_pair(const char *purpose)
      {
        int rv = wrapped::pipe(fds);
        if (rv < 0) throw datatypes::posix_error(errno, purpose);
      }
      ~pipe_pair()
      {
        close();
      }
      void close()
      {
        if (fds[0] >= 0) { wrapped::close(fds[0]); fds[0] = -1; }
        if (fds[1] >= 0) { wrapped::close(fds[1]); fds[1] = -1; }
      }
      int for_reading(purpose p = keep_ownership)
      {
        wrapped::close(fds[1]);
        int n = fds[0];
        if (p == release_ownership) fds[0] = -1;
        return n;
      }
      int for_writing(purpose p = keep_ownership)
      {
        wrapped::close(fds[0]);
        int n = fds[1];
        if (p == release_ownership) fds[1] = -1;
        return n;
      }
    private:
      pipe_pair(const pipe_pair&);
      pipe_pair& operator=(const pipe_pair&);
      int fds[2];
    };

    int start_presenter_process(output::formatter& out, int verbose)
    {
      pipe_pair p("communication pipe for presenter process");

      pid_t pid = wrapped::fork();
      if (pid < 0)
        {
          throw datatypes::posix_error(errno, "forking presenter process");
        }
      if (pid != 0)
        {
          return p.for_writing(pipe_pair::release_ownership);
        }
      int presenter_pipe = p.for_reading();

      std::map<pid_t, test_case_result> messages;
      for (;;)
        {
          pid_t test_case_id;
          int rv = wrapped::read(presenter_pipe,
                                &test_case_id,
                                sizeof(test_case_id));
          if (rv == 0)
            {
              assert(messages.size() == 0);
              wrapped::_Exit(0);
            }
          assert(rv == sizeof(test_case_id));
          test_case_result &s = messages[test_case_id];

          comm::type t;
          rv = wrapped::read(presenter_pipe, &t, sizeof(t));
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
                    rv = wrapped::read(presenter_pipe,
                                      p + bytes_read,
                                      sizeof(len) - bytes_read);
                    assert(rv > 0);
                    bytes_read += rv;
                  }
                char *buff = static_cast<char *>(alloca(len));
                bytes_read = 0;
                while (bytes_read < len)
                  {
                    rv = wrapped::read(presenter_pipe,
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
                rv = wrapped::read(presenter_pipe, &len, sizeof(len));
                assert(rv == sizeof(len));
                assert(len == 0);
                std::ostringstream os;
                if (!s.success || verbose)
                  {
                    printer print(out, s.name, s.success);
                    for (std::list<event>::iterator i = s.history.begin();
                         i != s.history.end();
                         ++i)
                      {
                        out.print(i->tag, i->body);
                      }
                    if (!s.termination.empty() || s.nonempty_dir)
                      {
                        if (s.nonempty_dir)
                          {
                            const char *wd
                              = test_case_factory::get_working_dir();
                            const size_t dlen = wrapped::strlen(wd);
                            size_t len = dlen;
                            len+= 1;
                            len+= s.name.size();
                            char *dn = static_cast<char*>(alloca(len));
                            wrapped::strcpy(dn, wd);
                            dn[dlen]='/';
                            wrapped::strcpy(dn + dlen + 1, s.name.c_str());
                            out.terminate(s.termination, dn);
                          }
                        else
                          {
                            out.terminate(s.termination);
                          }
                      }
                  }
              }
              messages.erase(test_case_id);
              break;
            case comm::dir:
              {
                size_t len;
                rv = wrapped::read(presenter_pipe, &len, sizeof(len));
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
                rv = wrapped::read(presenter_pipe, &len, sizeof(len));
                assert(rv == sizeof(len));
                if (len)
                  {
                    char *buff = static_cast<char *>(alloca(len));
                    rv = wrapped::read(presenter_pipe, buff, len);
                    assert(size_t(rv) == len);

                    if (t == comm::exit_ok || t == comm::exit_fail)
                      {
                        s.termination=std::string(buff, len);
                      }
                    else
                      {
                        static const char *tag[] = { "stdout",
                                                     "stderr",
                                                     "info" };
                        assert(size_t(t) < (sizeof(tag)/sizeof(tag[0])));
                        event e = { tag[t], std::string(buff, len) };
                        s.history.push_back(e);
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
    wrapped::close(presenter_pipe);
    ::siginfo_t info;
    for (;;)
      {
        int rv = wrapped::waitid(P_ALL, 0, &info, WEXITED);
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
        const size_t len = wrapped::strlen(e.what());
#define TEMPLATE_HEAD "Unexpectedly caught std::exception\n  what()="
        const size_t head_size = sizeof(TEMPLATE_HEAD) - 1;
        char *msg = static_cast<char *>(alloca(head_size + len + 1));
        wrapped::strcpy(msg, TEMPLATE_HEAD);
        wrapped::strcpy(msg + head_size, e.what());
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
        std::cout << *i << " ";
        run_test_case(i);
        ++num_tests_run;
        std::cout << "OK\n";
        return;
      }

    pipe_pair c2p("communication pipe test-case to main process");
    pipe_pair p2c("communication pipe main process to test-case");
    pipe_pair stderr("communication pipe for test-case stderr");
    pipe_pair stdout("communication pipe for test-case stdout");

    int wd = first_free_working_dir;
    first_free_working_dir = working_dirs[wd];
    i->set_wd(wd);

    ::pid_t pid;
    for (;;)
      {
        pid = wrapped::fork();
        if (pid < 0) throw datatypes::posix_error(errno,
                                                  "fork test-case process");
        if (pid >= 0) break;
        assert(errno == EINTR);
      }
    if (pid < 0) return;
    if (pid == 0) // child
      {
        comm::report.set_fds(p2c.for_reading(pipe_pair::release_ownership),
                             c2p.for_writing(pipe_pair::release_ownership));
        wrapped::dup2(stdout.for_writing(), 1);
        wrapped::dup2(stderr.for_writing(), 2);
        stdout.close();
        stderr.close();
        p2c.close();
        c2p.close();
        i->goto_wd();
        run_test_case(i);
        // never executed!
        assert("unreachable code reached" == 0);
      }

    // parent
    ++pending_children;
    i->setup(pid,
             c2p.for_reading(pipe_pair::release_ownership),
             p2c.for_writing(pipe_pair::release_ownership),
             stdout.for_reading(pipe_pair::release_ownership),
             stderr.for_reading(pipe_pair::release_ownership));
    manage_children(num_parallel);
  }

  namespace {

    output::formatter &output_formatter(bool use_xml,
                                        int fd,
                                        int argc, const char *argv[])
    {
      if (use_xml)
        {
          static output::xml_formatter xo(fd, argc, argv);
          return xo;
        }
      static output::text_formatter to(fd, argc, argv);
      return to;
    }
  }

  unsigned test_case_factory::do_run(int argc, const char *argv[],
                                     std::ostream &err_os)
  {
    const char *working_dir = 0;
    bool quiet = false;
    int output_fd = 1;
    bool xml = false;
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
            int o = wrapped::open(*p, O_CREAT | O_WRONLY | O_TRUNC, 0666);
            if (o < 0)
              {
                err_os << "Failed to open " << *p << " for writing\n";
                return -1;
              }
            output_fd = o;
            xml = !xml;
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
          wrapped::strcpy(dirbase, working_dir);
          break;
        case 'n':
          nodeps = true;
          break;
        case 'x':
          xml = !xml;
          break;
        default:
          err_os <<
            "Usage: " << argv[0] << " [flags] {testcases}\n"
            "  where flags can be:\n"
            "   -l         - list test cases\n"
            "   -n         - ignore dependencies\n"
            "   -d dir     - specify workind directory (must exist)\n"
            "   -v         - verbose mode\n"
            "   -c number  - Control number of spawned test case processes\n"
            "                if 0 the tests are run in the parent process\n"
            "   -o file    - Direct xml output to named file. Brief result\n"
            "                will be displayed on stdout\n"
            "   -q         - Don't display the -o brief\n"
            "   -x         - XML output on std-out or non-XML output on file\n";

          return -1;
        }
        ++p;
      }
    output::formatter &out = output_formatter(xml, output_fd, argc, argv);

    try {
      if (tests_as_child_procs())
        {
          if (!working_dir && !wrapped::mkdtemp(dirbase))
            {
              err_os << argv[0] << ": failed to create working directory\n";
              return 1;
            }
          if (wrapped::chdir(dirbase) != 0)
            {
              err_os << argv[0] << ": couldn't move to working directoryy\n";
              wrapped::rmdir(dirbase);
              return 1;
            }
          presenter_pipe = start_presenter_process(out, verbose_mode);
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
          std::ostringstream os;
          out.statistics(num_registered_tests,
                         num_tests_run,
                         num_tests_run - num_successful_tests);
          if (output_fd != 1 && !quiet)
            {
              std::cout << num_registered_tests << " registered, "
                        << num_tests_run << " run, "
                        << num_successful_tests << " OK, "
                        << num_tests_run - num_successful_tests << " FAILED!\n";
            }
          for (unsigned n = 0; n < max_parallel; ++n)
            {
              char name[std::numeric_limits<unsigned>::digits/3+2];
              int len = wrapped::snprintf(name, sizeof(name), "%u", n);
              assert(len > 0 && len < int(sizeof(name)));
              (void)len; // silence warning
              (void)wrapped::rmdir(name); // ignore, taken care of as error
            }

          if (!implementation::is_dir_empty("."))
            {
              out.nonempty_dir(dirbase);
              if (output_fd != 1 && !quiet)
                {
                  std::cout << "Files remain in " << dirbase << '\n';
                }
            }
          else if (working_dir == 0)
            {
              if (wrapped::chdir("..") < 0)
                {
                  throw datatypes::posix_error(errno,
                                               "chdir back from testcase working dir");
                }
              (void)wrapped::rmdir(dirbase); // ignore, taken care of as error
            }
          const std::string &s = os.str();
          wrapped::write(output_fd, s.c_str(), s.length());
        }

      if (reg.get_next() != &reg)
        {
          if (output_fd != 1 && !quiet)
            {
              std::cout << "Blocked tests:\n";
            }
          for (implementation::test_case_registrator *i = reg.get_next();
               i != &reg;
               i = i->get_next())
            {
              out.blocked_test(i);
              if (output_fd != 1 && !quiet)
                {
                  std::cout << "  " << *i << '\n';
                }
            }
        }
      return num_tests_run - num_successful_tests;
    }
    catch (datatypes::posix_error &e)
      {
        err_os << "Fatal error:"
               << e.what()
               << "\nCan't continue\n";
      }
    return -1;
  }

} // namespace crpcut

crpcut::implementation::namespace_info current_namespace(0,0);
