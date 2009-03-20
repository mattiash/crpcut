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
#include "clocks.hpp"
extern "C" {
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>
}

#define POLL_USE_EPOLL
#include "poll.hpp"
#include "implementation.hpp"

#include <cassert>
#include <limits>
#include <cstdio>
#include "posix_encapsulation.hpp"

namespace crpcut {

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
      while (*p && *match && *p == *match)
        {
          ++p;
          ++match;
        }
      return *p ? 0 : match;
    }

    std::ostream &operator<<(std::ostream &os, const namespace_info &ns)
    {
      if (!ns.parent) return os;
      os << *ns.parent;
      os << ns.name;
      os << "::";
      return os;
    }

    bool is_dir_empty(const char *name)
    {
      DIR *d = wrapped::opendir(name);
      assert(d);
      char buff[sizeof(dirent) + PATH_MAX];
      dirent *ent = reinterpret_cast<dirent*>(buff),*result = ent;
      bool empty = true;
      while (empty && result && (wrapped::readdir_r(d, ent, &result) == 0))
        {
          if (wrapped::strcmp(ent->d_name, ".") == 0 ||
              wrapped::strcmp(ent->d_name, "..") == 0)
            continue;
          empty = false;
        }
      wrapped::closedir(d);
      return empty;
    }

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
        rv = wrapped::read(fd, &t, sizeof(t));
      } while (rv == -1 && errno == EINTR);
      if (rv == 0) return false; // eof
      assert(rv == sizeof(t));
      if (t == comm::exit_fail)
        {
          reg->register_success(false);
        }
      size_t len = 0;
      do {
        rv = wrapped::read(fd, &len, sizeof(len));
      } while (rv == -1 && errno == EINTR);
      assert(rv == sizeof(len));

      size_t bytes_read = 0;
      if (t == comm::set_timeout)
        {
          assert(len == sizeof(reg->absolute_deadline_ms));
          assert(!reg->deadline_is_set());
          unsigned ts;
          char *p = static_cast<char*>(static_cast<void*>(&ts));
          while (bytes_read < len)
            {
              rv = wrapped::read(fd, p + bytes_read, len - bytes_read);
              if (rv == -1 && errno == EINTR) continue;
              assert(rv > 0);
              bytes_read += rv;
            }
          ts+= clocks::monotonic::timestamp_ms_absolute();
          reg->absolute_deadline_ms = ts;
          reg->deadline_set = true;
          do {
            rv = wrapped::write(response_fd, &len, sizeof(len));
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
            rv = wrapped::write(response_fd, &len, sizeof(len));
          } while (rv == -1 && errno == EINTR);
          return true;
        }
      char *buff = static_cast<char *>(::alloca(len));
      while (bytes_read < len)
        {
          rv = wrapped::read(fd, buff + bytes_read, len - bytes_read);
          if (rv == 0) break;
          if (rv == -1 && errno == EINTR) continue;
          assert(rv > 0);
          bytes_read += rv;
        }
      do {
        rv = wrapped::write(response_fd, &len, sizeof(len));
      } while (rv == -1 && errno == EINTR);
      switch (t)
        {
        case comm::begin_test:
          reg->phase = running;
          return true;
        case comm::end_test:
          reg->phase = destroying;
          return true;
        }
      test_case_factory::present(reg->get_pid(), t, reg->phase, len, buff);
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
        stderr_reader(this),
        phase(creating)
    {
      test_case_factory::obj().reg.prev = this;
      prev->next = this;
    }

    void test_case_registrator::kill()
    {
      wrapped::kill(pid_, SIGKILL);
      death_note = true;
      deadline_set = false;
      static const char msg[] = "Timed out - killed";
      register_success(false);
      test_case_factory::present(pid_,
                                 comm::exit_fail,
                                 phase,
                                 sizeof(msg) - 1,
                                 msg);
    }

    int test_case_registrator::ms_until_deadline() const
    {
      unsigned now = clocks::monotonic::timestamp_ms_absolute();
      int diff = absolute_deadline_ms - now;
      return diff < 0 ? 0 : diff;
    }

    void test_case_registrator::clear_deadline()
    {
      assert(deadline_is_set());
      test_case_factory::clear_deadline(this);
      deadline_set = false;
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
      stream::toastream<1024> os;
      os << *this;
      test_case_factory::introduce_name(pid, os.begin(), os.size());
    }

    void test_case_registrator::set_wd(int n)
    {
      dirnum = n;
      stream::toastream<std::numeric_limits<int>::digits/3+1> name;
      name << n << '\0';
      if (wrapped::mkdir(name.begin(), 0700) != 0)
        {
          assert(errno == EEXIST);
        }
    }

    void test_case_registrator::goto_wd() const
    {
      stream::toastream<std::numeric_limits<int>::digits/3+1> name;
      name << dirnum << '\0';
      if (wrapped::chdir(name.begin()) != 0)
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
          int rv = wrapped::waitid(P_PID, pid_, &info, WEXITED);
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
        stream::toastream<std::numeric_limits<int>::digits/3+1> dirname;
        dirname << dirnum << '\0';
        if (!is_dir_empty(dirname.begin()))
          {
            stream::toastream<1024> tcname;
            tcname << *this << '\0';
            test_case_factory::present(pid_, comm::dir, phase, 0, 0);
            wrapped::rename(dirname.begin(), tcname.begin());
            t = comm::exit_fail;
            register_success(false);
          }
      }

      if (!death_note)
        {
          stream::toastream<1024> out;
            {

              switch (info.si_code)
                {
                case CLD_EXITED:
                  {
                    if (!failed())
                      {
                        if (!is_expected_exit(info.si_status))
                          {
                            phase = post_mortem;
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
                            phase = post_mortem;
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
          test_case_factory::present(pid_, t, phase, out.size(), out.begin());
        }
      register_success(t == comm::exit_ok);
      test_case_factory::return_dir(dirnum);
      test_case_factory::present(pid_, comm::end_test, phase, 0, 0);
      assert(succeeded() || failed());
      if (succeeded())
        {
          test_case_factory::test_succeeded(this);
        }
    }

  } // namespace implementation

}
