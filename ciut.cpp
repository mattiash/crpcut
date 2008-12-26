#include "ciut.hpp"
extern "C" {
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <fcntl.h>
}

extern "C" {
  static bool child = false;
  static void child_handler(int, siginfo_t *, void *)
  {
    child = true;
  }
}

namespace ciut {
  namespace policies {
    no_core_file::no_core_file()
    {
      rlimit r = { 0, 0};
      setrlimit(RLIMIT_CORE, &r);
    }
  }
  namespace implementation {
    test_case_registrator
    ::test_case_registrator(const char *name,
                            test_case_base & (*func)())
      : name_(name),
        next(&test_case_factory::obj().reg),
        prev(test_case_factory::obj().reg.prev),
        func_(func),
        death_note(false),
        successful(false)
    {
      test_case_factory::obj().reg.prev = this;
      prev->next = this;
    }

    void test_case_registrator::setup(pid_t pid, int in_fd, int out_fd)
    {
      epoll_event ev;
      memset(&ev, 0, sizeof ev);
      ev.events = EPOLLIN;
      ev.data.ptr = this;
      pid_ = pid;
      in_fd_ = in_fd;
      out_fd_ = out_fd;
      int rv = epoll_ctl(test_case_factory::epollfd(), EPOLL_CTL_ADD, in_fd, &ev);
      assert(rv == 0);
    }
    void test_case_registrator::unregister_fds()
    {
      int rv = epoll_ctl(test_case_factory::epollfd(), EPOLL_CTL_DEL, in_fd_, 0);
      assert(rv == 0);
      ::close(in_fd_);
      ::close(out_fd_);
      std::string().swap(report);
    }
    comm::type test_case_registrator::read_report()
    {
      static const char * const header[] = { "OK", "FAILED!", "INFO", "WARNING" };
      comm::type t;
      int rv;
      do {
        rv = ::read(in_fd_, &t, sizeof(t));
      } while (rv == -1 && errno == EINTR);
      assert(rv == sizeof(t));
      successful |= t == comm::exit_ok;
      report += header[t];
      size_t len = 0;
      do {
        rv = ::read(in_fd_, &len, sizeof(len));
      } while (rv == -1 && errno == EINTR);
      assert(rv == sizeof(len));

      size_t bytes_read = 0;
      char buff[len+1];
      while (bytes_read < len)
        {
          rv = ::read(in_fd_, buff + bytes_read, len - bytes_read);
          if (rv == 0) break;
          if (rv == -1 && errno == EINTR) continue;
          assert(rv > 0);
          bytes_read += rv;
        }
      buff[bytes_read] = 0;
      if (len)
        {
          report += " - ";
          report += buff;
        }
      do {
        rv = ::write(out_fd_, &len, sizeof(len));
      } while (rv == -1 && errno == EINTR);
      death_note |= (t == comm::exit_ok || t == comm::exit_fail);
      return t;
    }

    void test_case_registrator::manage_death()
    {
      ::siginfo_t info;
      for (;;)
        {
          int rv = ::waitid(P_PID, pid_, &info, WEXITED);
          if (rv == -1 && errno == EINTR) continue;
          assert(rv == 0);
          break;
        }

      std::ostringstream out;
      switch (info.si_code)
        {
        case CLD_EXITED:
          if (is_expected_exit(info.si_status))
            {
              out << "OK";
              successful = true;
            }
          else
            {
              out << "FAILED! Unexpectedly exited with status = " << info.si_status;
            }
          break;
        case CLD_KILLED:
          if (is_expected_signal(info.si_status))
            {
              out << "OK";
              successful = true;
            }
          else
            {
              out << "FAILED! Died on signal " << info.si_status;
            }
          break;
        case CLD_DUMPED:
          {
            static char core_pattern[PATH_MAX] = "core";
            static int cpfd = 0;
            if (cpfd == 0)
              {
                cpfd = ::open("/proc/sys/kernel/core_pattern", O_RDONLY);
                if (cpfd > 0)
                  {
                    int num = ::read(cpfd, core_pattern, sizeof(core_pattern));
                    if (num > 1) core_pattern[num-1] = 0;
                    ::close(cpfd);
                  }
              }
            out << "FAILED! Dumped core";
            static char core_name[PATH_MAX];

            const char *s = core_pattern;
            char *d = core_name;
            while (*s)
              {
                if (*s != '%')
                  {
                    *d++ = *s;
                  }
                else
                  {
                    ++s;
                    switch (*s)
                      {
                      case 0:
                        break;
                      case '%':
                        *d++='%';
                        break;
                      case 'p':
                        d+=std::sprintf(d, "%u", info.si_pid);
                        break;
                      case 'u':
                        d+=std::sprintf(d, "%u", info.si_uid);
                        break;
                      case 'g':
                        break; // ignore for now
                      case 's':
                        d+=std::sprintf(d, "%u", info.si_status);
                        break;
                      case 't':
                        d+=std::sprintf(d, "%u", (unsigned)time(NULL));
                        break;
                      case 'h':
                        gethostname(d, core_name+PATH_MAX-d);
                        while (*d)
                          {
                            ++d;
                          }
                        break;
                      case 'c':
                        break; // ignore for now
                      default:
                        ; // also ignore
                      }
                  }
                ++s;
              }
            *d = 0;
            std::ostringstream newname;
            newname << *this << ".core";
            out << " - renaming file from "
                << core_name
                << " to "
                << newname.str();
            int rv = ::rename(core_name, newname.str().c_str());
            if (rv) out << " failed";
          }
          break;
        default:
          out << "FAILED! Died with unknown code=" << info.si_code;
        }
      if (!death_note)
        {
          report += out.str();
          death_note = true;
        }
    }
  }


  int test_case_factory::epollfd()
  {
    static int rv = epoll_create(test_case_factory::max_parallel);
    assert(rv != -1);
    return rv;
  }

  void test_case_factory::run_test_case(implementation::test_case_registrator *i) const
  {
    test_case_base *p = 0;
    const char *msg = 0;
    try {
      p = (i->instantiate_obj());
    }
    catch (std::exception &e)
      {
        msg = e.what();
      }
    catch (...)
      {
        msg = "unknown exception";
      }
    if (msg)
      {
        std::ostringstream out;
        out << "Creating fixture for "
            << *i
            << " failed with "
            << msg << std::endl;
        report(comm::exit_fail, out);
      }
    try {
      p->run();
    }
    catch (std::exception &e)
      {
        std::ostringstream out;
        out << "unexpected std exception, what() is \"" << e.what() << "\"";
        report(comm::exit_fail, out);
      }
    catch (...)
      {
        report(comm::exit_fail, "unknown exception");
      }
    p->~test_case_base();
    report(comm::exit_ok);
  }

  void test_case_factory::manage_children(unsigned max_pending_children)
  {
    while (pending_children >= max_pending_children)
      {
        epoll_event ev;
        for (;;)
          {
            int rv = epoll_wait(epollfd(), &ev,  1, -1);
            if (rv == 1) break;
          }
        typedef implementation::test_case_registrator registrator;
        registrator *child = static_cast<registrator*>(ev.data.ptr);

        if (ev.events & EPOLLIN)
          {
            child->read_report();
          }
        if (ev.events & EPOLLHUP)
          {
            child->manage_death();
            ++num_tests_run;
            num_failed_tests+= child->failed();
            if (child->failed() || verbose_mode)
              {
                child->print_report(std::cout);
              }
            child->unregister_fds();

            --pending_children;
          }
      }
  }

  void test_case_factory::do_run(int, const char *argv[])
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
            std::istringstream is(*p);
            if (!(is >> num_parallel) || num_parallel > max_parallel)
              {
                std::cout
                  << "num child processes must be a positive integer no greater than "
                  << max_parallel
                  << "\nA value of 0 means test cases are executed in the parent process"
                  "\n";
                return;
              }
          }
          break;
        default:
          std::cout <<
            "Usage: " << argv[0] << " [flags] {testcases}\n"
            "  where flags can be:\n"
            "    -v           - verbose mode\n"
            "    -c number    - Control number of spawned test case processes\n"
            "                   if 0 the tests are run in the parent process\n";
          return;
        }
        ++p;
      }
    const char **names = p;
    struct sigaction action, old_action;
    memset(&action, 0, sizeof action);
    action.sa_sigaction = child_handler;
    ::sigaction(SIGCHLD, &action, &old_action);

    for (implementation::test_case_registrator *i = reg.next;
         i != &reg;
         i = i->next)
      {
        ++num_tests;
        if (*names)
          {
            bool found = false;
            for (const char **name = names; *name; ++name)
              {
                if ((found = i->match_name(*name))) break;
              }
            if (!found) continue;
          }
        int c2p[2];
        ::pipe(c2p);
        int p2c[2];
        ::pipe(p2c);

        ::pid_t pid = ::fork();
        if (pid < 0) continue;
        if (pid == 0) // child
          {
            ::sigaction(SIGCHLD, &old_action, 0);
            comm::report.set_fds(p2c[0], c2p[1]);
            ::close(p2c[1]);
            ::close(c2p[0]);
            run_test_case(i);
            // never executed!
            assert("unreachable code reached" == 0);
          }

        // parent
        ++pending_children;
        i->setup(pid, c2p[0], p2c[1]);
        ::close(c2p[1]);
        ::close(p2c[0]);

        manage_children(num_parallel);
      }
    if (pending_children) manage_children(1);
    std::cout << num_tests_run << " tests run (" << num_failed_tests << " failed/"
              << num_tests_run - num_failed_tests << " OK) - totally "
              << num_tests << " tests registered\n";
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
  }



  namespace comm {
    void reporter::operator()(type t, std::ostringstream &os) const
    {
      write(t);
      {
        const std::string &s = os.str();
        const size_t len = s.length();
        write(len);
        const char *p = s.c_str();
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
        using std::ostringstream;
        os.~ostringstream(); // man, this is ugly, but _Exit() leaks
      }
      _Exit(0);
    }

    reporter report;

  }
}

ciut::implementation::namespace_info current_namespace(0,0);
