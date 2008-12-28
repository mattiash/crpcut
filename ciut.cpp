#include "ciut.hpp"
#define POLL_USE_EPOLL
#include "poll.hpp"
extern "C" {
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <fcntl.h>
}
#include <map>
extern "C" {
  static void child_handler(int, siginfo_t *, void *)
  {
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

    poll<test_case_registrator> poller;

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
      pid_ = pid;
      in_fd_ = in_fd;
      out_fd_ = out_fd;
      poller.add_fd(in_fd, this);
      std::ostringstream os;
      os << *this;
      test_case_factory::introduce_name(pid, os.str());
    }

    void test_case_registrator::unregister_fds()
    {
      poller.del_fd(in_fd_);
      ::close(in_fd_);
      ::close(out_fd_);
    }

    bool test_case_registrator::read_report()
    {
      comm::type t;
      int rv;
      do {
        rv = ::read(in_fd_, &t, sizeof(t));
      } while (rv == -1 && errno == EINTR);
      if (rv == 0) return false; // eof
      assert(rv == sizeof(t));
      successful |= t == comm::exit_ok;

      size_t len = 0;
      do {
        rv = ::read(in_fd_, &len, sizeof(len));
      } while (rv == -1 && errno == EINTR);
      assert(rv == sizeof(len));

      size_t bytes_read = 0;
      char buff[len];
      while (bytes_read < len)
        {
          rv = ::read(in_fd_, buff + bytes_read, len - bytes_read);
          if (rv == 0) break;
          if (rv == -1 && errno == EINTR) continue;
          assert(rv > 0);
          bytes_read += rv;
        }
      do {
        rv = ::write(out_fd_, &len, sizeof(len));
      } while (rv == -1 && errno == EINTR);
      test_case_factory::present(pid_, t, len, buff);
      death_note |= (t == comm::exit_ok || t == comm::exit_fail);
      return true;
    }

    namespace {

      void save_core_file(const siginfo_t& info, const std::string& newname)
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
        ::rename(core_name, newname.c_str());
      }

    } // unnamed namespace

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

      comm::type t = comm::exit_ok;
      std::ostringstream out;
      switch (info.si_code)
        {
        case CLD_EXITED:
          if (!(successful |= is_expected_exit(info.si_status)))
            {
              out << "Unexpectedly exited with status = " << info.si_status;
              t = comm::exit_fail;
            }
          break;
        case CLD_KILLED:
          if (!(successful |= is_expected_signal(info.si_status)))
            {
              out << "Died on signal " << info.si_status;
              t = comm::exit_fail;
            }
          break;
        case CLD_DUMPED:
          {
            out << "Dumped core";
            t = comm::exit_fail;
            std::ostringstream newname;
            newname << *this << ".core";
            save_core_file(info, newname.str());
            out << " - saving core file as "
                << newname.str();
          }
          break;
        default:
          out << "Died with unknown code=" << info.si_code;
          t = comm::exit_fail;
        }
      if (!death_note)
        {
          const std::string &s = out.str();
          test_case_factory::present(pid_, t, s.length(), s.c_str());
          death_note = true;
        }
    }

  } // namespace implementation

  void test_case_factory::introduce_name(pid_t pid, const std::string &name)
  {
    int pipe = obj().presenter_pipe;
    for (;;)
      {
        int rv = ::write(pipe, &pid, sizeof(pid));
        if (rv == sizeof(pid)) break;
        assert (rv == -1 && errno == EINTR);
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

  void test_case_factory::present(pid_t pid,
                                  comm::type t,
                                  size_t len,
                                  const char *buff)
  {
    int pipe = obj().presenter_pipe;
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
    obj().num_failed_tests += (t == comm::exit_fail);
    if (!tests_as_child_procs() && t == comm::exit_fail)
      {
        obj().kill_presenter_process();
        exit(1);
      }
  }

  void test_case_factory::start_presenter_process()
  {
    int fds[2];
    int rv = ::pipe(fds);
    assert(rv == 0);
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
    std::map<pid_t, std::pair<std::string, std::string> > messages;
    for (;;)
      {
        pid_t test_case_id;
        int rv = ::read(presenter_pipe, &test_case_id, sizeof(test_case_id));
        if (rv == 0)
          {
            assert(messages.size() == 0);
            exit(0);
          }
        std::pair<std::string,std::string> &s = messages[test_case_id];
        if (s.first.length() == 0)
          {
            // introduction to test case, name follows

            size_t len;
            rv = ::read(presenter_pipe, &len, sizeof(len));
            assert(size_t(rv) == sizeof(len));
            char buff[len];
            rv = ::read(presenter_pipe, buff, len);
            assert(size_t(rv) == len);
            s.first = std::string(buff, len);
            continue;
          }

        static const char * const header[] = {
          "OK", "FAILED!", "INFO", "WARNING"
        };

        comm::type t;
        rv = ::read(presenter_pipe, &t, sizeof(t));
        assert(rv == sizeof(t));
        s.second += header[t];

        size_t len;
        rv = ::read(presenter_pipe, &len, sizeof(len));
        if (len)
          {
            static const char separator[] = " - ";
            const size_t bufflen = len + sizeof(separator) - 1;
            char buff[bufflen];

            std::strcpy(buff, separator);
            rv = ::read(presenter_pipe, buff + sizeof(separator) - 1, len);
            assert(size_t(rv) == len);
            s.second += std::string(buff, bufflen);
          }
        if ((t == comm::exit_ok && verbose_mode) || t == comm::exit_fail)
          {
            std::cout << s.first << " - " << s.second << std::endl;
          }
        if (t == comm::exit_ok || t == comm::exit_fail)
          {
            messages.erase(test_case_id);
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
    if (tests_as_child_procs())
      {
        p->~test_case_base(); // Ugly, but since report kills when parallel
      }                       // it takes care of a memory leak.
    report(comm::exit_ok);
  }

  void test_case_factory::manage_children(unsigned max_pending_children)
  {
    while (pending_children >= max_pending_children)
      {
        using namespace implementation;
        poll<test_case_registrator>::descriptor desc = poller.wait();
        bool read_failed = false;
        if (desc.read())
          {
            read_failed = !desc->read_report();
          }
        if (read_failed || desc.hup())
          {
            desc->manage_death();
            ++num_tests_run;
            if (!desc->failed()) desc->register_success();
            desc->unregister_fds();
            --pending_children;
          }
      }
  }

  void test_case_factory::start_test(implementation::test_case_registrator *i)
  {
    if (!tests_as_child_procs())
      {
        std::ostringstream os;
        os << *i;
        introduce_name(0, os.str());
        run_test_case(i);
        ++num_tests_run;
        if (!i->failed()) i->register_success();
        return;
      }
    struct sigaction action, old_action;
    static bool sighandler_registered = false;
    if (!sighandler_registered)
      {
        memset(&action, 0, sizeof action);
        action.sa_sigaction = child_handler;
        ::sigaction(SIGCHLD, &action, &old_action);
      }
    int c2p[2];
    ::pipe(c2p);
    int p2c[2];
    ::pipe(p2c);

    ::pid_t pid = ::fork();
    if (pid < 0) return;
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

  unsigned test_case_factory::do_run(int, const char *argv[])
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
                return 1;
              }
          }
          break;
        case 'l':
          {
            const char **names = ++p;
            if (*names && **names == '-')
              {
                std::cerr << "-l must be followed by a (possibly empty) test case list\n";
                return 1;
              }
            for (implementation::test_case_registrator *i = reg.next;
                 i != &reg;
                 i = i->next)
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
          std::cout <<
            "Usage: " << argv[0] << " [flags] {testcases}\n"
            "  where flags can be:\n"
            "    -l           - list test cases\n"
            "    -d           - ignore dependencies\n"
            "    -v           - verbose mode\n"
            "    -c number    - Control number of spawned test case processes\n"
            "                   if 0 the tests are run in the parent process\n";
          return 1;
        }
        ++p;
      }
    start_presenter_process();
    const char **names = p;

    for (;;)
      {
        bool progress = false;
        implementation::test_case_registrator *i = reg.next;
        while (i != &reg)
          {
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
                i = i->next;
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
      }
    if (pending_children) manage_children(1);
    kill_presenter_process();
    std::cout << num_tests_run << " tests run ("
              << num_failed_tests  << " failed/"
              << num_tests_run - num_failed_tests << " OK)\n";
    if (verbose_mode && reg.next != &reg)
      {
        std::cout << "Not run tests are:\n";
        for (implementation::test_case_registrator *i = reg.next;
             i != &reg;
             i = i->next)
          {
            std::cout << "  " << *i << '\n';
          }
      }
    return num_failed_tests;
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

    void reporter::operator()(type t, std::ostringstream &os) const
    {
      if (!test_case_factory::tests_as_child_procs())
        {
          const std::string &s = os.str();
          test_case_factory::present(pid_t(), t, s.length(), os.str().c_str());
          return;
        }
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
        os.~ostringstream(); // man, this is ugly, but _Exit() causes leaks
      }
      _Exit(0);
    }

    reporter report;

  } // namespace comm

} // namespace ciut

ciut::implementation::namespace_info current_namespace(0,0);
