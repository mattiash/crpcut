#include "ciut.hpp"
extern "C" {
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <fcntl.h>
}

extern "C" {
  static int child = 0;
  static void child_handler(int)
  {
    child = 1;
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
      : name_(name), func_(func),
        next(&test_case_factory::obj().reg),
        prev(test_case_factory::obj().reg.prev)
    {
      test_case_factory::obj().reg.prev = this;
      prev->next = this;
    }
  }
  void test_case_factory::do_run(const char *name)
  {
    static char core_pattern[PATH_MAX] = "core";
    {
      int fd = ::open("/proc/sys/kernel/core_pattern", O_RDONLY);
      if (fd > 0)
        {
          int num = ::read(fd, core_pattern, sizeof(core_pattern));
          if (num > 1) core_pattern[num-1] = 0;
          ::close(fd);
        }
    }
    ::signal(SIGCHLD, &child_handler);
    for (implementation::test_case_registrator *i = reg.next;
         i != &reg;
         i = i->next)
      {
        int death_note = 0;
        if (name && !i->match_name(name)) continue;
        ::pid_t pid = ::fork();
        if (pid < 0) continue;
        if (pid == 0) // child
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
                testcase_pipe.report(comm::exit_fail, out);
                std::_Exit(2);
              }
            try {
              p->run();
            }
            catch (std::exception &e)
              {
                std::ostringstream out;
                out << "unexpected std exception, what() is \"" << e.what() << "\"";
                testcase_pipe.report(comm::exit_fail, out);
                std::_Exit(1);
              }
            catch (...)
              {
                testcase_pipe.report(comm::exit_fail, "unknown exception");
                std::_Exit(1);
              }
            p->~test_case_base();
            testcase_pipe.report(comm::exit_ok);
            std::_Exit(0);
          }
        else
          {
            while (!child)
              {
                fd_set readfds;
                FD_ZERO(&readfds);
                int n = testcase_pipe.populate<comm::pipe::read_op,
                  comm::pipe::c2p>(readfds, 0);
                int rv = ::select(n, &readfds, 0, 0, 0);
                if (rv == -1) continue;
                assert((testcase_pipe.ok_to<comm::pipe::read_op, comm::pipe::c2p>(readfds)));

                comm::type type;
                testcase_pipe.read<comm::pipe::c2p>(type);
                size_t size;
                testcase_pipe.read<comm::pipe::c2p>(size);
                std::cout << *i << " - ";
                switch (type)
                  {
                  case comm::exit_ok:
                    std::cout << "OK"; death_note = 1; break;
                  case comm::exit_fail:
                    std::cout << "FAILED!"; death_note = 1; break;
                  case comm::info:
                    std::cout << "FYI"; break;
                  case comm::violation:
                    std::cout << "ERROR"; break;
                  default:
                    std::cout << "unknown(" << type << ')';
                  }
                if (size)
                  {
                    char buff[size];
                    testcase_pipe.read<comm::pipe::c2p>(buff, size);
                    std::cout << " - ";
                    std::cout.write(buff, size);
                  }
                testcase_pipe.write<comm::pipe::p2c>(size); // ack
                std::cout << std::endl;
              }
            child = 0;

            ::siginfo_t info;
            ::waitid(P_PID, pid, &info, WEXITED | WNOHANG);
            std::ostringstream out;
            out << *i << " - ";
            switch (info.si_code)
              {
              case CLD_EXITED:
                if (i->is_expected_exit(info.si_status))
                  {
                    out << "OK";
                  }
                else
                  {
                    out << "FAILED! Unexpectedly exited with status = " << info.si_status;
                  }
                break;
              case CLD_KILLED:
                if (i->is_expected_signal(info.si_status))
                  {
                    out << "OK";
                  }
                else
                  {
                    out << "FAILED! Died on signal " << info.si_status;
                  }
                break;
              case CLD_STOPPED:
                out << "was stopped";
                break;
              case CLD_DUMPED:
                {
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
                              d+=std::sprintf(d, "%u", pid);
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
                  newname << *i << ".core";
                  out << " - renaming file from "
                      << core_name
                      << " to "
                      << newname.str();
                  int rv = ::rename(core_name, newname.str().c_str());
                  if (rv) out << " failed";
                }
                break;
              case CLD_TRAPPED:
                out << " trapped";
                break;
              case CLD_CONTINUED:
                out << " was continued";
                break;
              default:
                out << "FAILED! Died with unknown code=" << info.si_code;
              }
            if (!death_note) std::cout << out.str() << '\n';
          }
      }
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
}
ciut::implementation::namespace_info current_namespace(0,0);
comm::pipe testcase_pipe;
