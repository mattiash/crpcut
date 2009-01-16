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


#include "crpcut.hpp"
#define POLL_USE_EPOLL
#include "poll.hpp"
extern "C" {
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
}
#include <map>
#include <list>
#include <limits>
#include <cstdlib>
#include <cstdio>

namespace crpcut {

  namespace policies {
    no_core_file::no_core_file()
    {
      rlimit r = { 0, 0};
      ::setrlimit(RLIMIT_CORE, &r);
    }

    namespace deaths {
      void none::expected_death(std::ostream &os)
      {
        os << "normal exit";
      }
    }

    namespace dependencies {
      void base::register_success(bool value)
      {
        if (value)
          {
            if (state != not_run) return;
            state = success;
            for (basic_enforcer *p = dependants; p; p = p->next)
              {
                --p->num;
              }
          }
        else
          {
            state = fail;
          }
      }
    }
    namespace timeout {
      basic_enforcer::basic_enforcer(type t, unsigned)
      {
        ::clock_gettime(t == realtime
                        ? CLOCK_MONOTONIC
                        : CLOCK_PROCESS_CPUTIME_ID,
                        &ts);
      }

      void basic_enforcer::check(type t, unsigned timeout_ms)
      {
        timespec now;
        ::clock_gettime(t == realtime
                        ? CLOCK_MONOTONIC
                        : CLOCK_PROCESS_CPUTIME_ID,
                        &now);
        now.tv_sec -= ts.tv_sec;
        if (now.tv_nsec < ts.tv_nsec)
          {
            now.tv_nsec += 1000000000;
            now.tv_sec -= 1;
          }
        now.tv_nsec -= ts.tv_nsec;
        unsigned long ms = now.tv_sec*1000 + now.tv_nsec / 1000000;
        if (ms > timeout_ms)
          {
            std::ostringstream os;
            os << (t == realtime ? "Realtime" : "Cputime")
               << " timeout " << timeout_ms
               << "ms exceeded.\n  Actual time to completion was "
               << ms << "ms";
            report(comm::exit_fail, os);
          }
      }

    } // namespace timeout

  } // namespace policies

  namespace {

    bool is_dir_empty(const char *name)
    {
      DIR *d = ::opendir(name);
      assert(d);
      char buff[sizeof(dirent) + PATH_MAX];
      dirent *ent = reinterpret_cast<dirent*>(buff),*result = ent;
      bool empty = true;
      while (empty && result && (::readdir_r(d, ent, &result) == 0))
        {
          if (std::strcmp(ent->d_name, ".") == 0 ||
              std::strcmp(ent->d_name, "..") == 0)
            continue;
          empty = false;
        }
      ::closedir(d);
      return empty;
    }
  } // unnamed namespace

  namespace implementation {

    typedef poll<fdreader, test_case_factory::max_parallel*3> polltype;

    polltype poller;

    void fdreader::set_fd(int fd_)
    {
      assert(fd == 0);
      assert(reg != 0);
      fd = fd_;
      poller.add_fd(fd, this);
      reg->activate_reader();
    }

    void fdreader::unregister()
    {
      assert(fd != 0);
      assert(reg != 0);
      reg->deactivate_reader();
      poller.del_fd(fd);
      fd = 0;
    }

    bool report_reader::do_read(int fd)
    {
      comm::type t;
      int rv;
      do {
        rv = ::read(fd, &t, sizeof(t));
      } while (rv == -1 && errno == EINTR);
      if (rv == 0) return false; // eof
      assert(rv == sizeof(t));
      if (t == comm::exit_fail)
        {
          reg->register_success(false);
        }
      size_t len = 0;
      do {
        rv = ::read(fd, &len, sizeof(len));
      } while (rv == -1 && errno == EINTR);
      assert(rv == sizeof(len));

      size_t bytes_read = 0;
      if (t == comm::set_timeout)
        {
          assert(len == sizeof(reg->deadline));
          assert(!reg->deadline_is_set());
          char *p = static_cast<char*>(static_cast<void*>(&reg->deadline));
          while (bytes_read < len)
            {
              rv = ::read(fd, p + bytes_read, len - bytes_read);
              if (rv == -1 && errno == EINTR) continue;
              assert(rv > 0);
              bytes_read += rv;
            }
          do {
            rv = ::write(response_fd, &len, sizeof(len));
          } while (rv == -1 && errno == EINTR);
          assert(reg->deadline_is_set());
          test_case_factory::set_deadline(reg);
          return true;
        }
      if (t == comm::cancel_timeout)
        {
          assert(len == 0);
          reg->clear_deadline();
          do {
            rv = ::write(response_fd, &len, sizeof(len));
          } while (rv == -1 && errno == EINTR);
          return true;
        }
      char *buff = static_cast<char *>(::alloca(len));
      while (bytes_read < len)
        {
          rv = ::read(fd, buff + bytes_read, len - bytes_read);
          if (rv == 0) break;
          if (rv == -1 && errno == EINTR) continue;
          assert(rv > 0);
          bytes_read += rv;
        }
      do {
        rv = ::write(response_fd, &len, sizeof(len));
      } while (rv == -1 && errno == EINTR);
      test_case_factory::present(reg->get_pid(), t, len, buff);
      if (t == comm::exit_ok || t == comm::exit_fail)
        {
          if (!reg->death_note && reg->deadline_is_set())
            {
              reg->clear_deadline();
            }
          reg->death_note = true;
        }
      return true;
    }


    test_case_registrator
    ::test_case_registrator(const char *name,
                            test_case_base & (*func)())
      : name_(name),
        next(&test_case_factory::obj().reg),
        prev(test_case_factory::obj().reg.prev),
        func_(func),
        death_note(false),
        rep_reader(this),
        stdout_reader(this),
        stderr_reader(this)
    {
      test_case_factory::obj().reg.prev = this;
      prev->next = this;
    }

    void test_case_registrator::kill()
    {
      ::kill(pid_, SIGKILL);
      death_note = true;
      deadline.tv_sec = 0;
      static const char msg[] = "Timed out - killed";
      register_success(false);
      test_case_factory::present(pid_, comm::exit_fail, sizeof(msg) - 1, msg);
    }

    void test_case_registrator::clear_deadline()
    {
      assert(deadline_is_set());
      test_case_factory::clear_deadline(this);
      deadline.tv_sec = 0;
    }

    void test_case_registrator::setup(pid_t pid,
                                      int in_fd, int out_fd,
                                      int stdout_fd,
                                      int stderr_fd)
    {
      pid_ = pid;
      stdout_reader.set_fd(stdout_fd);
      stderr_reader.set_fd(stderr_fd);
      rep_reader.set_fds(in_fd, out_fd);
      std::ostringstream os;
      os << *this;
      test_case_factory::introduce_name(pid, os.str());
    }

    void test_case_registrator::set_wd(int n)
    {
      dirnum = n;
      char name[std::numeric_limits<int>::digits/3+1];
      int len = snprintf(name, sizeof(name), "%d", n);
      assert(len > 0 && len < int(sizeof(name)));
      (void)len; // silence warning when building with -DNDEBUG
      if (::mkdir(name, 0700) != 0)
        {
          assert(errno == EEXIST);
        }
    }

    void test_case_registrator::goto_wd() const
    {
      char name[std::numeric_limits<int>::digits/3+1];
      int len = snprintf(name, sizeof(name), "%d", dirnum);
      assert(len > 0 && len < int(sizeof(len)));
      (void)len; // silence warning when building with -DNDEBUG
      if (::chdir(name) != 0)
        {
          report(comm::exit_fail, "Couldn't chdir working dir");
          assert("unreachable code reached" == 0);
        }
    }


    void test_case_registrator::manage_death()
    {
      ::siginfo_t info;
      for (;;)
        {
          int rv = ::waitid(P_PID, pid_, &info, WEXITED);
          int n = errno;
          if (rv == -1 && n == EINTR) continue;
          assert(rv == 0);
          break;
        }
      assert(!succeeded());
      if (!death_note && deadline_is_set())
        {
          clear_deadline();
        }
      comm::type t = comm::exit_ok;
      {
        char dirname[std::numeric_limits<int>::digits/3+1];
        int len = std::snprintf(dirname, sizeof(dirname), "%d", dirnum);
        assert(len > 0 && len < int(sizeof(dirname)));
        (void)len; // silence warning when building with -DNDEBUG
        if (!is_dir_empty(dirname))
          {
            std::ostringstream tcname;
            tcname << *this;
            test_case_factory::present(pid_, comm::dir, 0, 0);
            std::rename(dirname, tcname.str().c_str());
            t = comm::exit_fail;
            register_success(false);
          }
      }
      if (!death_note)
        {
          std::ostringstream out;
            {

              switch (info.si_code)
                {
                case CLD_EXITED:
                  {
                    if (!failed())
                      {
                        if (!is_expected_exit(info.si_status))
                          {
                            out << "Exited with code "
                                << info.si_status << "\nExpected ";
                            expected_death(out);
                            t = comm::exit_fail;
                          }
                      }
                  }
                  break;
                case CLD_KILLED:
                  {
                    if (!failed())
                      {
                        if (!is_expected_signal(info.si_status))
                          {
                            out << "Died on signal "
                                << info.si_status << "\nExpected ";
                            expected_death(out);
                            t = comm::exit_fail;
                          }
                      }
                  }
                  break;
                case CLD_DUMPED:
                  out << "Died with core dump";
                  t = comm::exit_fail;
                  break;
                default:
                  out << "Died for unknown reason, code=" << info.si_code;
                  t = comm::exit_fail;
                }
            }
          death_note = true;
            {
              const std::string &s = out.str();
              test_case_factory::present(pid_, t, s.length(), s.c_str());
            }
        }
      register_success(t == comm::exit_ok);
      test_case_factory::return_dir(dirnum);
      test_case_factory::present(pid_, comm::end_test, 0, 0);
      assert(succeeded() || failed());
      if (succeeded())
        {
          test_case_factory::test_succeeded(this);
        }
    }
  } // namespace implementation


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

  void test_case_factory::do_introduce_name(pid_t pid, const std::string &name)
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

    size_t len = name.length();
    for (;;)
      {
        int rv = ::write(pipe, &len, sizeof(len));
        if (rv == sizeof(len)) break;
        assert(rv == -1 && errno == EINTR);
      }
    for (;;)
      {
        int rv = ::write(pipe, name.c_str(), len);
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
  }

  void test_case_factory::start_presenter_process()
  {
    int fds[2];
    int rv = ::pipe(fds);
    assert(rv == 0);
    (void)rv; // silence warning
    pid_t pid = ::fork();
    if (pid != 0)
      {
        presenter_pipe = fds[1];
        ::close(fds[0]);
        presenter_pid = pid;
        return;
      }
    presenter_pipe = fds[0];
    ::close(fds[1]);

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
            if (!s.success || verbose_mode)
              {
                bool history_print = false;
                std::cout << "  <test name=\"" << s.name
                          << "\" result=\"" << (s.success ? "OK" : "FAILED") << '"';
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
                            std::cout  << ">\n    <log>\n";
                            history_print = true;
                          }
                        endpos = s.find('\n', prevpos);
                        if (endpos == std::string::npos) break;
                        static const char *prefix[] = { "", "      " };
                        std::cout << prefix[prev_ended]
                                  << std::string(s, prevpos, endpos - prevpos)
                                  << "\n";
                        prev_ended = s[endpos-1] == '>';
                      }
                  }
                if (!s.termination.empty() || s.nonempty_dir)
                  {
                    if (!history_print)
                      {
                        std::cout << ">\n    <log>\n";
                      }
                    std::cout << "      <termination";
                    if (s.nonempty_dir)
                      {
                        std::cout << " nonempty_dir=\""
                                  << test_case_factory::get_working_dir()
                                  << '/'
                                  << s.name
                                  << '"';
                      }
                    if (s.termination.empty())
                      {
                        std::cout << "/>\n";
                      }
                    else
                      {
                        std::cout << '>' << s.termination << "</termination>\n";
                      }
                    history_print = true;
                  }
                if (history_print)
                  {
                    std::cout << "    </log>\n  </test>\n";
                  }
                else
                  {
                    std::cout << "/>\n";
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
                      std::ostringstream os;
                      if (t == comm::stdout)
                        {
                          CRPCUT_XML_TAG(stdout, os)
                            {
                              stdout << std::string(buff, len);
                            }
                        }
                      else
                        {
                          CRPCUT_XML_TAG(stderr, os)
                            {
                              stderr << std::string(buff, len);
                            }
                        }
                      s.history.push_back(os.str());
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

  void test_case_factory::kill_presenter_process()
  {
    ::close(presenter_pipe);
    ::siginfo_t info;
    for (;;)
      {
        int rv = ::waitid(P_PID, presenter_pid, &info, WEXITED);
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

  unsigned test_case_factory::do_run(int, const char *argv[], std::ostream &os)
  {
    const char **p = argv+1;
    while (const char *param = *p)
      {
        if (param[0] != '-') break;
        switch (param[1]) {
        case 'v':
          verbose_mode = true;
          break;
        case 'c':
          ++p;
          {
            char *end;
            unsigned long l = std::strtol(*p, &end, 10);
            if (*end != 0 || l > max_parallel)
              {
                os
                  << "num child processes must be a positive integer no greater than "
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
                os << "-l must be followed by a (possibly empty) test case list\n";
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
          nodeps = true;
          break;
        default:
          os <<
            "Usage: " << argv[0] << " [flags] {testcases}\n"
            "  where flags can be:\n"
            "    -l           - list test cases\n"
            "    -d           - ignore dependencies\n"
            "    -v           - verbose mode\n"
            "    -c number    - Control number of spawned test case processes\n"
            "                   if 0 the tests are run in the parent process\n";
          return -1;
        }
        ++p;
      }
    if (tests_as_child_procs())
      {
        if (!::mkdtemp(dirbase))
          {
            os << argv[0] << ": failed to create working directory\n";
            return 1;
          }
        if (::chdir(dirbase) != 0)
          {
            os << argv[0] << ": couldn't move to working directoryy\n";
            ::rmdir(dirbase);
            return 1;
          }
      }
    else
      {
        // strangely needed to get decent output for failed test cases,
        // even though the failed test-case output needs posix ::write()
        // to even show!
        std::setvbuf(::stdout, 0, _IONBF, 0);
        std::cout.rdbuf()->pubsetbuf(0, 0);
        std::cout.sync_with_stdio();
      }
    {
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
      std::cout <<
        "<?xml version=\"1.0\"?>\n\n"
        "<crpcut xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
        " xsi:noNamespaceSchemaLocation=\"crpcut.xsd\""
        " starttime=\"" << time_string <<
        "\" host=\"" << machine_string <<
        "\" name=\"" << argv[0] << "\">\n" << std::flush;
    }

    if (tests_as_child_procs()) start_presenter_process();
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
        CRPCUT_XML_TAG(statistics, std::cout)
          {
            CRPCUT_XML_TAG(registered_test_cases, statistics)
              {
                registered_test_cases << num_registered_tests;
              }
            CRPCUT_XML_TAG(run_test_cases, statistics)
              {
                run_test_cases << num_tests_run;
              }
            CRPCUT_XML_TAG(failed_test_cases, statistics)
              {
                failed_test_cases << num_tests_run - num_successful_tests;
              }
          }

        for (unsigned n = 0; n < max_parallel; ++n)
          {
            char name[std::numeric_limits<unsigned>::digits/3+2];
            int len = snprintf(name, sizeof(name), "%u", n);
            assert(len > 0 && len < int(sizeof(name)));
            (void)len; // silence warning
            ::rmdir(name);
          }
        if (!is_dir_empty("."))
          {
            std::cout << "  <remaining_files nonempty_dir=\"" << dirbase << "\"/>\n";
          }
        else
          {
            ::chdir("..");
            ::rmdir(dirbase);
          }
      }
    if (reg.get_next() != &reg)
      {
        std::cout << "  <blocked_tests>\n";
        for (implementation::test_case_registrator *i = reg.get_next();
             i != &reg;
             i = i->get_next())
          {
            std::cout << "    <test name=\"" << *i << "\"/>\n";
          }
        std::cout << "  </blocked_tests>\n";
      }

    std::cout << "</crpcut>\n";
    return num_tests_run - num_successful_tests;
  }

  namespace implementation {

    const char *namespace_info::match_name(const char *n) const
    {
      if (!parent) return n;

      const char *match = parent->match_name(n);
      if (!match) return match;
      if (!*match) return match; // actually a perfect match
      if (match != n && *match++ != ':') return 0;
      if (match != n && *match++ != ':') return 0;

      const char *p = name;
      while (*p && *n && *p == *n)
        {
          ++p;
          ++n;
        }
      return *p ? 0 : n;
    }

    std::ostream &operator<<(std::ostream &os, const namespace_info &ns)
    {
      if (!ns.parent) return os;
      os << *ns.parent;
      os << ns.name;
      os << "::";
      return os;
    }

  } // namespace implementation



  namespace comm {

    void reporter::operator()(type t, size_t len, const char *msg) const
    {
      if (!test_case_factory::tests_as_child_procs())
        {
          // this is strange. If I use std::cout, output is lost, despite
          // that it's set to unbuffered, and even if it's explicitly
          // flushed.
          ::write(1, msg, len);
          if (t == exit_fail)
            {
              ::abort();
            }
          return;
        }
      std::cout << std::flush;
      write(t);
      write(len);
      const char *p = msg;
      size_t bytes_written = 0;
      while (bytes_written < len)
        {
          int rv = ::write(write_fd,
                           p + bytes_written,
                           len - bytes_written);
          if (rv == -1 && errno == EINTR) continue;
          if (rv <= 0) throw "report failed";
          bytes_written += rv;
        }
      read(bytes_written);
      assert(len == bytes_written);
      bool terminal = (t == comm::exit_ok) || (t == comm::exit_fail);
      if (!terminal) return;
      std::_Exit(0);
    }

    reporter report;

  } // namespace comm

} // namespace crpcut

crpcut::implementation::namespace_info current_namespace(0,0);
