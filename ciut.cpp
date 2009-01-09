#include "ciut.hpp"
#define POLL_USE_SELECT
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

namespace ciut {

  namespace policies {
    no_core_file::no_core_file()
    {
      rlimit r = { 0, 0};
      setrlimit(RLIMIT_CORE, &r);
    }

    namespace timeout {
      basic_enforcer::basic_enforcer(type t, unsigned)
      {
        clock_gettime(t == realtime
                      ? CLOCK_MONOTONIC
                      : CLOCK_PROCESS_CPUTIME_ID,
                      &ts);
      }

      void basic_enforcer::check(type t, unsigned timeout_ms)
      {
        timespec now;
        clock_gettime(t == realtime
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
            CIUT_XML_TAG(timeout, os)
              {
                CIUT_XML_TAG(type, timeout)
                  {
                    type << (t == realtime ? "realtime" : "cputime");
                  }
                CIUT_XML_TAG(max_ms, timeout)
                  {
                    max_ms << timeout_ms;
                  }
                CIUT_XML_TAG(actual_ms, timeout)
                  {
                    actual_ms << ms;
                  }
              }
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
      dirent *ent;
      bool empty = true;
      while ((ent = readdir(d)) != 0 && empty)
        {
          if (strcmp(ent->d_name, ".") == 0 ||
              strcmp(ent->d_name, "..") == 0)
            continue;
          empty = false;
        }
      ::closedir(d);
      return empty;
    }
  } // unnamed namespace

  namespace implementation {

    typedef poll<fdreader,
                 test_case_factory::max_parallel*3> polltype;

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
      reg->successful |= t == comm::exit_ok;

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
      char buff[len];
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
        successful(false),
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
      char msg[] = "<timeout action=\"killed\"/>\n";
      test_case_factory::present(pid_, comm::exit_fail, sizeof(msg) - 1, msg);
    }

    void test_case_registrator::clear_deadline()
    {
      assert(deadline_is_set());
      test_case_factory::clear_deadline(this);
      deadline.tv_sec = 0;
    }

    void test_case_registrator::setup(pid_t pid, int in_fd, int out_fd, int stdout_fd, int stderr_fd)
    {
      pid_ = pid;
      stdout_reader.set_fd(stdout_fd);
      stderr_reader.set_fd(stderr_fd);
      rep_reader.set_fds(in_fd, out_fd);
      std::ostringstream os;
      os << *this;
      test_case_factory::introduce_name(pid, os.str());
    }
#if 0
    void test_case_registrator::unregister_fds()
    {
      stdout_reader.unregister();
      stderr_reader.unregister();
      rep_reader.close();
    }
#endif
    void test_case_registrator::set_wd(int n)
    {
      dirnum = n;
      std::ostringstream os;
      os << n;
      if (::mkdir(os.str().c_str(), 0700) != 0)
        {
          assert(errno == EEXIST);
        }
    }

    void test_case_registrator::goto_wd() const
    {
      std::ostringstream os;
      os << dirnum;
      if (::chdir(os.str().c_str()) != 0)
        {
          report(comm::exit_fail, "couldn't chdir working dir");
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
      if (!death_note && deadline_is_set())
        {
          clear_deadline();
        }
      comm::type t = comm::exit_ok;
      std::ostringstream out;
      CIUT_XML_TAG(termination, out)
        {
          switch (info.si_code)
            {
            case CLD_EXITED:
              if (!(successful |= is_expected_exit(info.si_status)))
                {
                  CIUT_XML_TAG(exit, termination,
                               xml::attr("code", info.si_status));

                  t = comm::exit_fail;
                }
              break;
            case CLD_KILLED:
              if (!(successful |= is_expected_signal(info.si_status)))
                {
                  CIUT_XML_TAG(signal, termination,
                               xml::attr("number", info.si_status));
                  t = comm::exit_fail;
                }
              break;
            case CLD_DUMPED:
              CIUT_XML_TAG(core_dump, termination);
              t = comm::exit_fail;
              break;
            default:
              CIUT_XML_TAG(unknown_reason, termination,
                           xml::attr("code", info.si_code));
              t = comm::exit_fail;
            }
        }
      if (!death_note)
        {
          const std::string &s = out.str();
          test_case_factory::present(pid_, t, s.length(), s.c_str());
          death_note = true;
        }
      {
        std::ostringstream dirname;
        dirname << dirnum;
        std::ostringstream tcname;
        tcname << *this;
        if (!is_dir_empty(dirname.str().c_str()))
          {
            std::ostringstream msg;
            CIUT_XML_TAG(remaining_files, msg)
              {
                CIUT_XML_TAG(directory, remaining_files)
                  {
                    directory << test_case_factory::get_working_dir() << '/' << tcname.str();
                  }
              }
            ::rename(dirname.str().c_str(), tcname.str().c_str());
            const std::string &s = msg.str();
            test_case_factory::present(pid_, comm::exit_fail, s.length(), s.c_str());
          }
        test_case_factory::return_dir(dirnum);
      }
      test_case_factory::present(pid_, comm::end_test, 0, 0);
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
    num_failed_tests += (t == comm::exit_fail);
    if (!tests_as_child_procs() && t == comm::exit_fail)
      {
        kill_presenter_process();
        exit(1);
      }
  }

  namespace {
    struct test_case_result
    {
      bool                   success;
      std::string            name;
      std::list<std::string> history;
    };
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

    std::map<pid_t, test_case_result> messages;
    std::cout << "  <log>\n";
    for (;;)
      {
        pid_t test_case_id;
        int rv = ::read(presenter_pipe, &test_case_id, sizeof(test_case_id));
        if (rv == 0)
          {
            assert(messages.size() == 0);
            std::cout << "  </log>\n";
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
              char buff[len];
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
                std::cout << "    <test name=\"" << s.name
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
                            std::cout  << ">\n";
                            history_print = true;
                          }
                        endpos = s.find('\n', prevpos);
                        if (endpos == std::string::npos) break;
                        static const char *prefix[] = { "", "      " };
                        std::cout << prefix[prev_ended] << std::string(s, prevpos, endpos - prevpos) << "\n";
                        prev_ended = s[endpos-1] == '>';
                      }
                  }
                if (history_print)
                  {
                    std::cout << "    </test>\n";
                  }
                else
                  {
                    std::cout << "/>\n";
                  }
              }
            messages.erase(test_case_id);
            break;
          case comm::exit_ok:
          case comm::exit_fail:
            s.success = t == comm::exit_ok; // fallthrough
          case comm::stdout:
          case comm::stderr:
            {
              size_t len;
              rv = ::read(presenter_pipe, &len, sizeof(len));
              if (len)
                {
                  char buff[len];

                  rv = ::read(presenter_pipe, buff, len);
                  assert(size_t(rv) == len);

                  if (t == comm::exit_ok || t == comm::exit_fail)
                    {
                      s.history.push_back(std::string(buff, len));
                    }
                  else
                    {
                      std::ostringstream os;
                      if (t == comm::stdout)
                        {
                          CIUT_XML_TAG(stdout, os)
                            {
                              stdout << std::string(buff, len);
                            }
                        }
                      else
                        {
                          CIUT_XML_TAG(stderr, os)
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

  void test_case_factory::run_test_case(implementation::test_case_registrator *i) const
  {
    i->goto_wd();
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
        CIUT_XML_TAG(exception_death, out)
          {
            CIUT_XML_TAG(caught, exception_death,
                         ciut::xml::attr("type", "std::exception"),
                         ciut::xml::attr("what", e.what()));
          }
        report(comm::exit_fail, out);
      }
    catch (...)
      {
        std::ostringstream out;
        CIUT_XML_TAG(exception_death, out)
          {
            CIUT_XML_TAG(caught, exception_death, ciut::xml::attr("type", "..."));
          }
        report(comm::exit_fail, out);
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
        int timeout_ms = deadlines.size()
          ? deadlines.front()->ms_until_deadline()
          : -1;
        implementation::polltype::descriptor desc = implementation::poller.wait(timeout_ms);
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
            implementation::test_case_registrator *r = desc->get_registrator();
            if (!r->has_active_readers())
              {
                r->manage_death();
                ++num_tests_run;
                if (!r->failed()) r->register_success();
                --pending_children;
              }
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
            std::istringstream is(*p);
            if (!(is >> num_parallel) || num_parallel > max_parallel)
              {
                os
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
                os << "-l must be followed by a (possibly empty) test case list\n";
                return 1;
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
          return 1;
        }
        ++p;
      }
    if (!mkdtemp(dirbase))
      {
        os << argv[0] << ": failed to create working directory\n";
        return 1;
      }
    if (chdir(dirbase) != 0)
      {
        os << argv[0] << ": couldn't move to working directori\n";
        ::rmdir(dirbase);
        return 1;
      }
    {
      static char time_string[] = "2009-01-09T23:59:59";
      time_t now = ::time(0);
      struct tm tmdata;
      ::gmtime_r(&now, &tmdata);
      std::sprintf(time_string, "%4.4d-%2.2d-%2.2dT%2.2d:%2.2d:%2.2d",
                   tmdata.tm_year + 1900,
                   tmdata.tm_mon + 1,
                   tmdata.tm_mday,
                   tmdata.tm_hour,
                   tmdata.tm_min,
                   tmdata.tm_sec);
      static char machine_string[PATH_MAX];
      ::gethostname(machine_string, sizeof(machine_string));
      std::cout <<
        "<?xml version=\"1.0\"?>\n\n"
        "<test_run"
        " time=\"" << time_string <<
        "\" machine=\"" << machine_string <<
        "\" name=\"" << argv[0] << "\">\n" << std::flush;
    }

    start_presenter_process();
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
    kill_presenter_process();
    std::cout <<
      "  <statistics>\n"
      "    <tests_registered>" << num_registered_tests << "</tests_registered>\n"
      "    <tests_run>"        << num_tests_run        << "</tests_run>\n"
      "    <tests_failed>"     << num_failed_tests     << "</tests_failed>\n"
      "  </statistics>\n";
    if (reg.get_next() != &reg)
      {
        std::cout << "  <blocked_tests>\n";
        for (implementation::test_case_registrator *i = reg.get_next();
             i != &reg;
             i = i->get_next())
          {
            std::cout << "    <testcase name=\"" << *i << "\"/>\n";
          }
        std::cout << "  </blocked_tests>\n";
      }
    for (unsigned n = 0; n < max_parallel; ++n)
      {
        std::ostringstream name;
        name << n;
        ::rmdir(name.str().c_str());
      }
    if (!is_dir_empty("."))
      {
        std::cout << "  <remaining_files directory=\"" << dirbase << "\"/>\n";
      }
    else
      {
        ::chdir("..");
        ::rmdir(dirbase);
      }
    std::cout << "</test_run>\n";
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
      bool terminal = (t == comm::exit_ok) || (t == comm::exit_fail);
      if (!test_case_factory::tests_as_child_procs())
        {
          const std::string &s = os.str();
          test_case_factory::present(pid_t(), t, s.length(), os.str().c_str());
          return;
        }
      std::cout << std::flush;
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
        if (!terminal) return;
        os.~ostringstream(); // man, this is ugly, but _Exit() causes leaks
      }
      _Exit(0);
    }

    reporter report;

  } // namespace comm

} // namespace ciut

ciut::implementation::namespace_info current_namespace(0,0);
