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

    std::size_t namespace_info::full_name_len() const
    {
      return (name ? wrapped::strlen(name) : 0)
        + (parent ? 2 + parent->full_name_len() : 0);
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

    void fdreader::set_fd(int fd)
    {
      assert(fd_ == 0);
      assert(reg != 0);
      fd_ = fd;
      poller.add_fd(fd_, this);
      reg->crpcut_activate_reader();
    }

    void fdreader::unregister()
    {
      assert(fd_ != 0);
      assert(reg != 0);
      reg->crpcut_deactivate_reader();
      poller.del_fd(fd_);
      fd_ = 0;
    }

    bool report_reader::do_read(int fd)
    {
      comm::type t;
      ssize_t rv;
      do {
        rv = wrapped::read(fd, &t, sizeof(t));
      } while (rv == -1 && errno == EINTR);
      if (rv == 0) return false; // eof
      assert(rv == sizeof(t));
      if (t == comm::exit_fail)
        {
          reg->crpcut_register_success(false);
        }
      size_t len = 0;
      do {
        rv = wrapped::read(fd, &len, sizeof(len));
      } while (rv == -1 && errno == EINTR);
      assert(rv == sizeof(len));

      size_t bytes_read = 0;
      if (t == comm::set_timeout)
        {
          assert(len == sizeof(reg->crpcut_absolute_deadline_ms));
          assert(!reg->crpcut_deadline_is_set());
          clocks::monotonic::timestamp ts;
          char *p = static_cast<char*>(static_cast<void*>(&ts));
          while (bytes_read < len)
            {
              rv = wrapped::read(fd, p + bytes_read, len - bytes_read);
              if (rv == -1 && errno == EINTR) continue;
              assert(rv > 0);
              bytes_read += rv;
            }
          ts+= clocks::monotonic::timestamp_ms_absolute();
          reg->crpcut_absolute_deadline_ms = ts;
          reg->crpcut_deadline_set = true;
          do {
            rv = wrapped::write(response_fd, &len, sizeof(len));
          } while (rv == -1 && errno == EINTR);
          assert(reg->crpcut_deadline_is_set());
          test_case_factory::set_deadline(reg);
          return true;
        }
      if (t == comm::cancel_timeout)
        {
          assert(len == 0);
          reg->crpcut_clear_deadline();
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
          reg->crpcut_phase = running;
          return true;
        case comm::end_test:
          reg->crpcut_phase = destroying;
          return true;
        default:
          ; // silence warning
        }
      test_case_factory::present(reg->crpcut_get_pid(), t, reg->crpcut_phase,
                                 len, buff);
      if (t == comm::exit_ok || t == comm::exit_fail)
        {
          if (!reg->crpcut_death_note && reg->crpcut_deadline_is_set())
            {
              reg->crpcut_clear_deadline();
            }
          reg->crpcut_death_note = true;
        }
      return true;
    }


    crpcut_test_case_registrator
    ::crpcut_test_case_registrator(const char *name,
                                   const namespace_info &ns)
      : crpcut_name_(name),
        crpcut_ns_info(&ns),
        crpcut_next(&test_case_factory::obj().reg),
        crpcut_prev(test_case_factory::obj().reg.crpcut_prev),
        crpcut_death_note(false),
        crpcut_rep_reader(this),
        crpcut_stdout_reader(this),
        crpcut_stderr_reader(this),
        crpcut_phase(creating)
    {
      test_case_factory::obj().reg.crpcut_prev = this;
      crpcut_prev->crpcut_next = this;
    }

    bool
    crpcut_test_case_registrator
    ::crpcut_match_name(const char *name_param) const
    {
      const char *p = crpcut_ns_info->match_name(name_param);
      if (p)
        {
          if (p != name_param || *p == ':')
            {
              if (!*p) return true; // match for whole suites
              if (!*p++ == ':') return false;
              if (!*p++ == ':') return false;
            }
        }
      else
        {
          p = name_param;
        }
      return crpcut::wrapped::strcmp(p, crpcut_name_) == 0;
    }

    std::size_t
    crpcut_test_case_registrator
    ::crpcut_full_name_len() const
    {
      return crpcut_ns_info->full_name_len()
        + 2
        + wrapped::strlen(crpcut_name_);
    }

    std::ostream &
    crpcut_test_case_registrator
    ::crpcut_print_name(std::ostream &os) const
    {
      os << *crpcut_ns_info;
      return os << crpcut_name_;
    }

    void
    crpcut_test_case_registrator
    ::crpcut_manage_test_case_execution(test_case_base* p)
    {
      comm::report(comm::begin_test, 0, 0);

      try {
        p->run();
      }
      catch (std::exception &e)
        {
          heap::set_limit(heap::system);
          const size_t len = wrapped::strlen(e.what());
#define TEMPLATE_HEAD "Unexpectedly caught std::exception\n  what()="
          const size_t head_size = sizeof(TEMPLATE_HEAD) - 1;
          char *msg = static_cast<char *>(alloca(head_size + len + 1));
          lib::strcpy(lib::strcpy(msg, TEMPLATE_HEAD), e.what());
#undef TEMPLATE_HEAD
          comm::report(comm::exit_fail, msg, head_size + len);
        }
      catch (...)
        {
          static const char msg[] = "Unexpectedly caught ...";
          comm::report(comm::exit_fail, msg);
        }


      if (!test_case_factory::tests_as_child_procs())
        {
          crpcut_register_success();
        }
    }

    void
    crpcut_test_case_registrator
    ::crpcut_kill()
    {
      wrapped::kill(crpcut_pid_, SIGKILL);
      crpcut_death_note = true;
      crpcut_deadline_set = false;
      static const char msg[] = "Timed out - killed";
      crpcut_register_success(false);
      test_case_factory::present(crpcut_pid_,
                                 comm::exit_fail,
                                 crpcut_phase,
                                 sizeof(msg) - 1,
                                 msg);
    }

    unsigned long
    crpcut_test_case_registrator
    ::crpcut_ms_until_deadline() const
    {
      clocks::monotonic::timestamp now
        = clocks::monotonic::timestamp_ms_absolute();
      long diff = crpcut_absolute_deadline_ms - now;
      return diff < 0 ? 0UL : diff;
    }

    void
    crpcut_test_case_registrator
    ::crpcut_clear_deadline()
    {
      assert(crpcut_deadline_is_set());
      test_case_factory::clear_deadline(this);
      crpcut_deadline_set = false;
    }

    void
    crpcut_test_case_registrator
    ::crpcut_setup(pid_t pid,
                   int in_fd, int out_fd,
                   int stdout_fd,
                   int stderr_fd)
    {
      crpcut_pid_ = pid;
      crpcut_stdout_reader.set_fd(stdout_fd);
      crpcut_stderr_reader.set_fd(stderr_fd);
      crpcut_rep_reader.set_fds(in_fd, out_fd);
      stream::toastream<1024> os;
      os << *this;
      test_case_factory::introduce_name(pid, os.begin(), os.size());
    }

    void
    crpcut_test_case_registrator::crpcut_set_wd(int n)
    {
      crpcut_dirnum = n;
      stream::toastream<std::numeric_limits<int>::digits/3+1> name;
      name << n << '\0';
      if (wrapped::mkdir(name.begin(), 0700) != 0)
        {
          assert(errno == EEXIST);
        }
    }

    void
    crpcut_test_case_registrator
    ::crpcut_goto_wd() const
    {
      stream::toastream<std::numeric_limits<int>::digits/3+1> name;
      name << crpcut_dirnum << '\0';
      if (wrapped::chdir(name.begin()) != 0)
        {
          comm::report(comm::exit_fail, "Couldn't chdir working dir");
          assert("unreachable code reached" == 0);
        }
    }


    void
    crpcut_test_case_registrator
    ::crpcut_manage_death()
    {
      ::siginfo_t info;
      for (;;)
        {
          int rv = wrapped::waitid(P_PID, crpcut_pid_, &info, WEXITED);
          int n = errno;
          if (rv == -1 && n == EINTR) continue;
          assert(rv == 0);
          break;
        }
      assert(!crpcut_succeeded());
      if (!crpcut_death_note && crpcut_deadline_is_set())
        {
          crpcut_clear_deadline();
        }
      comm::type t = comm::exit_ok;
      {
        stream::toastream<std::numeric_limits<int>::digits/3+1> dirname;
        dirname << crpcut_dirnum << '\0';
        if (!is_dir_empty(dirname.begin()))
          {
            stream::toastream<1024> tcname;
            tcname << *this << '\0';
            test_case_factory::present(crpcut_pid_, comm::dir, crpcut_phase,
                                       0, 0);
            wrapped::rename(dirname.begin(), tcname.begin());
            t = comm::exit_fail;
            crpcut_register_success(false);
          }
      }

      if (!crpcut_death_note)
        {
          stream::toastream<1024> out;
          {
            switch (info.si_code)
              {
              case CLD_EXITED:
                {
                  if (!crpcut_failed())
                    {
                      if (!crpcut_is_expected_exit(info.si_status))
                        {
                          crpcut_phase = post_mortem;
                          out << "Exited with code "
                              << info.si_status << "\nExpected ";
                          crpcut_expected_death(out);
                          t = comm::exit_fail;
                        }
                    }
                }
                break;
              case CLD_KILLED:
                {
                  if (!crpcut_failed())
                    {
                      if (!crpcut_is_expected_signal(info.si_status))
                        {
                          crpcut_phase = post_mortem;
                          out << "Died on signal "
                              << info.si_status << "\nExpected ";
                          crpcut_expected_death(out);
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
          crpcut_death_note = true;
          test_case_factory::present(crpcut_pid_, t, crpcut_phase,
                                     out.size(), out.begin());
        }
      crpcut_register_success(t == comm::exit_ok);
      test_case_factory::return_dir(crpcut_dirnum);
      test_case_factory::present(crpcut_pid_, comm::end_test, crpcut_phase,
                                 0, 0);
      assert(crpcut_succeeded() || crpcut_failed());
      if (crpcut_succeeded())
        {
          test_case_factory::test_succeeded(this);
        }
    }

  } // namespace implementation

}
