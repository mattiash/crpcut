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
#include <fcntl.h>
}
#include <queue>
#include "posix_encapsulation.hpp"
namespace crpcut {


  test_case_factory::test_case_factory()
    : pending_children(0),
      verbose_mode(false),
      nodeps(false),
      num_parallel(1),
      num_registered_tests(0),
      num_selected_tests(0),
      num_tests_run(0),
      num_successful_tests(0),
      first_free_working_dir(0)
  {
    lib::strcpy(dirbase, "/tmp/crpcutXXXXXX");
    for (unsigned n = 0; n < max_parallel; ++n)
      {
        working_dirs[n] = n+1;
      }
  }

  void test_case_factory
  ::do_set_deadline(implementation::crpcut_test_case_registrator *i)
  {
    assert(i->crpcut_deadline_is_set());
    deadlines.push_back(i);
    std::push_heap(deadlines.begin(), deadlines.end(),
                   &implementation::crpcut_test_case_registrator
                   ::crpcut_timeout_compare);
  }

  void test_case_factory
  ::do_clear_deadline(implementation::crpcut_test_case_registrator *i)
  {
    assert(i->crpcut_deadline_is_set());
    for (size_t n = 0; n < deadlines.size(); ++n)
      {
        if (deadlines[n] == i)
          {
            using implementation::crpcut_test_case_registrator;
            for (;;)
              {
                size_t m = (n+1)*2-1;
                if (m >= deadlines.size() - 1) break;
                if (crpcut_test_case_registrator
                    ::crpcut_timeout_compare(deadlines[m+1],
                                             deadlines[m]))
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
        ssize_t rv = wrapped::write(pipe, &pid, sizeof(pid));
        if (rv == sizeof(pid)) break;
        assert (rv == -1 && errno == EINTR);
      }
    const comm::type t = comm::begin_test;
    for (;;)
      {
        ssize_t rv = wrapped::write(pipe, &t, sizeof(t));
        if (rv == sizeof(t)) break;
        assert(rv == -1 && errno == EINTR);
      }
    const test_phase p = running;
    for (;;)
      {
        ssize_t rv = wrapped::write(pipe, &p, sizeof(p));
        if (rv == sizeof(p)) break;
        assert(rv == -1 && errno == EINTR);
      }

    for (;;)
      {
        ssize_t rv = wrapped::write(pipe, &len, sizeof(len));
        if (rv == sizeof(len)) break;
        assert(rv == -1 && errno == EINTR);
      }
    for (;;)
      {
        ssize_t rv = wrapped::write(pipe, name, len);
        if (size_t(rv) == len) break;
        assert(rv == -1 && errno == EINTR);
      }
  }

  void test_case_factory::do_present(pid_t pid,
                                     comm::type t,
                                     test_phase phase,
                                     size_t len,
                                     const char *buff)
  {
    int pipe = presenter_pipe;
    ssize_t rv = wrapped::write(pipe, &pid, sizeof(pid));
    assert(rv == sizeof(pid));
    rv = wrapped::write(pipe, &t, sizeof(t));
    assert(rv == sizeof(t));
    rv = wrapped::write(pipe, &phase, sizeof(phase));
    assert(rv == sizeof(phase));
    rv = wrapped::write(pipe, &len, sizeof(len));
    assert(rv == sizeof(len));
    if (len)
      {
        rv = wrapped::write(pipe, buff, len);
        assert(size_t(rv) == len);
      }
  }

  namespace {

    struct info { size_t len; const char *str; };
#define ESTR(s) { sizeof(#s)-1, #s }
    static const info tag_info[] =
      {
        ESTR(stdout),
        ESTR(stderr),
        ESTR(info),
        ESTR(exit_ok),
        ESTR(exit_fail),
        ESTR(dir),
        ESTR(set_timeout),
        ESTR(cancel_timeout),
        ESTR(begin_test),
        ESTR(end_test)
      };
#undef ESTR

    template <typename T>
    class list_elem
    {
    public:
      list_elem();
      list_elem(T *p);
      virtual ~list_elem();
      void link_after(list_elem& r);
      void link_before(list_elem &r);
      T *next() { return next_; }
      T *prev() { return prev_; }
      bool is_empty() const;
    private:
      void unlink();
      list_elem(const list_elem&);
      list_elem& operator=(const list_elem&);
      T *next_;
      T *prev_;
    };

    template <typename T>
    inline list_elem<T>::list_elem()
      : next_(static_cast<T*>(this)),
        prev_(static_cast<T*>(this))
    {
    }

    template <typename T>
    inline list_elem<T>::list_elem(T *p)
      : next_(p),
        prev_(p)
    {
    }

    template <typename T>
    inline list_elem<T>::~list_elem()
    {
      unlink();
    }

    template <typename T>
    inline void list_elem<T>::link_after(list_elem& r)
    {
      next_ = r.next_;
      prev_ = static_cast<T*>(&r);
      next_->prev_ = static_cast<T*>(this);
      r.next_ = static_cast<T*>(this);
    }

    template <typename T>
    inline void list_elem<T>::link_before(list_elem &r)
    {
      prev_ = r.prev_;
      next_ = static_cast<T*>(&r);
      prev_->next_ = static_cast<T*>(this);
      r.prev_ = static_cast<T*>(this);
    }

    template <typename T>
    inline bool list_elem<T>::is_empty() const
    {
      return next_ == static_cast<const T*>(this);
    }

    template <typename T>
    inline void list_elem<T>::unlink()
    {
      T *n = next_;
      T *p = prev_;
      n->prev_ = p;
      p->next_ = n;
      prev_ = static_cast<T*>(this);
      next_ = static_cast<T*>(this);
    }


    template <typename T, size_t N>
    class fix_allocator
    {
      union elem {
        char ballast[sizeof(T)];
        elem *next;
      };
      static datatypes::array_v<elem, N> array;
      static elem *first_free;
    public:
      static void *alloc() {
        if (first_free)
          {
            elem *p = first_free;
            first_free = p->next;
            return p;
          }
        if (array.size() < N)
          {
            array.push_back(elem());
            return &array.back();
          }
        return wrapped::malloc(sizeof(T));
      }
      static void release(void *p)
      {
        if (p >= array.begin() && p < array.end())
          {
            elem *e = static_cast<elem*>(p);
            e->next = first_free;
            first_free = e;
          }
        else
          {
            wrapped::free(p);
          }
      }
    };

    template <typename T, size_t N>
    typename fix_allocator<T, N>::elem *fix_allocator<T, N>::first_free;

    template <typename T, size_t N>
    datatypes::array_v<typename fix_allocator<T, N>::elem, N>
    fix_allocator<T, N>::array;

    struct event : public list_elem<event>
    {
      event(comm::type t, const char *b, size_t bl)
        : list_elem<event>(this), tag(t), body(b), body_len(bl)
      {
      }
      ~event() {  wrapped::free(body); }
      comm::type tag;
      const char *body;
      size_t body_len;
      void *operator new(size_t) { return allocator::alloc(); }
      void operator delete(void *p) { allocator::release(p); }
    private:
      event();
      typedef fix_allocator<event,
                            test_case_factory::max_parallel*3> allocator;
    };
    struct test_case_result : public list_elem<test_case_result>
    {
      test_case_result(pid_t pid)
        :list_elem<test_case_result>(this),
         id(pid),
         success(false),
         nonempty_dir(false),
         name(0),
         name_len(0),
         termination(0),
         term_len(0)
      {}
      ~test_case_result()
      {
        wrapped::free(termination);
        wrapped::free(name);
        while (!history.is_empty())
          {
            event *e = history.next();
            delete e;
          }
      }
      void *operator new(size_t) { return allocator::alloc(); }
      void operator delete(void *p) { allocator::release(p); }
      pid_t            id;
      bool             success;
      bool             nonempty_dir;
      const char      *name;
      size_t           name_len;
      const char      *termination;
      size_t           term_len;
      list_elem<event> history;
    private:
      test_case_result(const test_case_result& r);
      test_case_result& operator=(const test_case_result&r);
      typedef fix_allocator<test_case_result,
                            test_case_factory::max_parallel> allocator;
    };



    class printer
    {
    public:
      printer(output::formatter& o_, const char *name, size_t n_len, bool result)
        : o(o_)
      {
        o.begin_case(name, n_len, result);
      }
      ~printer() { o.end_case(); }
    private:
      output::formatter &o;
    };

    class pipe_pair
    {
    public:
      typedef enum { release_ownership, keep_ownership } purpose;
      pipe_pair(const char *purpose_msg)
      {
        int rv = wrapped::pipe(fds);
        if (rv < 0) throw datatypes::posix_error(errno, purpose_msg);
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

      list_elem<test_case_result> messages;
      for (;;)
        {
          pid_t test_case_id;
          ssize_t rv = wrapped::read(presenter_pipe,
                                     &test_case_id,
                                     sizeof(test_case_id));
          if (rv == 0)
            {
              assert(messages.is_empty());
              wrapped::exit(0);
            }
          assert(rv == sizeof(test_case_id));
          test_case_result *s = 0;

          // a linear search isn't that great, but the
          // amount of data is small.
          for (test_case_result *i = messages.next();
               i != messages.next()->prev();
               i = i->next())
            {
              if (i->id == test_case_id)
                {
                  s = i;
                  break;
                }
            }
          if (!s)
            {
              s = new test_case_result(test_case_id);
              s->link_after(messages);
            }
          comm::type t;
          rv = wrapped::read(presenter_pipe, &t, sizeof(t));
          assert(rv == sizeof(t));

          test_phase phase;
          rv = wrapped::read(presenter_pipe, &phase, sizeof(phase));
          assert(rv == sizeof(phase));

          switch (t)
            {
            case comm::begin_test:
              {
                assert(s->name_len == 0);
                assert(s->history.is_empty());
                // introduction to test case, name follows

                size_t len = 0;
                char *ptr = static_cast<char*>(static_cast<void*>(&len));
                size_t bytes_read = 0;
                while (bytes_read < sizeof(len))
                  {
                    rv = wrapped::read(presenter_pipe,
                                      ptr + bytes_read,
                                      sizeof(len) - bytes_read);
                    assert(rv > 0);
                    bytes_read += rv;
                  }
                char *buff = static_cast<char *>(wrapped::malloc(len + 1));
                bytes_read = 0;
                while (bytes_read < len)
                  {
                    rv = wrapped::read(presenter_pipe,
                                      buff + bytes_read,
                                      len - bytes_read);
                    assert(rv >= 0);
                    bytes_read += rv;
                  }
                buff[len] = 0;
                s->name = buff;
                s->name_len = len;
                s->success = true;
                s->nonempty_dir = false;
              }
              break;
            case comm::end_test:
              {
                size_t len;
                rv = wrapped::read(presenter_pipe, &len, sizeof(len));
                assert(rv == sizeof(len));
                assert(len == 0);

                if (!s->success || verbose)
                  {
                    printer print(out, s->name, s->name_len, s->success);

                    for (event *i = s->history.next();
                         i != static_cast<event*>(&s->history);
                         i = i->next())
                      {
                        out.print(tag_info[i->tag].str, tag_info[i->tag].len,
                                  i->body, i->body_len);
                      }
                    if (s->term_len || s->nonempty_dir)
                      {
                        if (s->nonempty_dir)
                          {
                            const char *wd
                              = test_case_factory::get_working_dir();
                            const size_t dlen = wrapped::strlen(wd);
                            len = dlen;
                            len+= 1;
                            len+= s->name_len;
                            char *dn = static_cast<char*>(alloca(len + 1));
                            lib::strcpy(lib::strcpy(lib::strcpy(dn,  wd),
                                                    "/"),
                                        s->name);
                            out.terminate(phase, s->termination, s->term_len, dn, len);
                          }
                        else
                          {
                            out.terminate(phase, s->termination, s->term_len);
                          }
                      }
                  }
              }
              delete s;
              break;
            case comm::dir:
              {
                size_t len;
                rv = wrapped::read(presenter_pipe, &len, sizeof(len));
                assert(rv == sizeof(len));
                assert(len == 0);
                (void)len; // silense warning
                s->success = false;
                s->nonempty_dir = true;
              }
              break;
            case comm::exit_ok:
            case comm::exit_fail:
              s->success &= t == comm::exit_ok; // fallthrough
            case comm::stdout:
            case comm::stderr:
            case comm::info:
              {
                size_t len;
                rv = wrapped::read(presenter_pipe, &len, sizeof(len));
                assert(rv == sizeof(len));
                if (len)
                  {
                    char *buff = static_cast<char *>(wrapped::malloc(len));
                    rv = wrapped::read(presenter_pipe, buff, len);
                    assert(size_t(rv) == len);

                    if (t == comm::exit_ok || t == comm::exit_fail)
                      {
                        s->termination = buff;
                        s->term_len = len;
                      }
                    else
                      {
                        event *e = new event(t, buff, len);
                        e->link_before(s->history);
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

  void test_case_factory::manage_children(unsigned max_pending_children)
  {
    while (pending_children >= max_pending_children)
      {
        using namespace implementation;

        int timeout_ms = deadlines.size()
          ? int(deadlines.front()->crpcut_ms_until_deadline())
          : -1;
        polltype::descriptor desc = poller.wait(timeout_ms);

        if (desc.timeout())
          {
            assert(deadlines.size());
            crpcut_test_case_registrator *i = deadlines.front();
            std::pop_heap(deadlines.begin(), deadlines.end(),
                          &crpcut_test_case_registrator
                          ::crpcut_timeout_compare);
            deadlines.pop_back();
            i->crpcut_kill();
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
            crpcut_test_case_registrator *r = desc->get_registrator();
            if (!r->crpcut_has_active_readers())
              {
                r->crpcut_manage_death();
                --pending_children;
              }
          }
      }
  }

  void test_case_factory::start_test(implementation::crpcut_test_case_registrator *i)
  {
    ++num_tests_run;
    if (!tests_as_child_procs())
      {
        std::cout << *i << " ";
        i->crpcut_run_test_case();
        std::cout << "OK\n";
        return;
      }

    pipe_pair c2p("communication pipe test-case to main process");
    pipe_pair p2c("communication pipe main process to test-case");
    pipe_pair stderr("communication pipe for test-case stderr");
    pipe_pair stdout("communication pipe for test-case stdout");

    int wd = first_free_working_dir;
    first_free_working_dir = working_dirs[wd];
    i->crpcut_set_wd(wd);

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
        heap::control::enabled = true;
        comm::report.set_fds(p2c.for_reading(pipe_pair::release_ownership),
                             c2p.for_writing(pipe_pair::release_ownership));
        wrapped::dup2(stdout.for_writing(), 1);
        wrapped::dup2(stderr.for_writing(), 2);
        stdout.close();
        stderr.close();
        p2c.close();
        c2p.close();
        i->crpcut_goto_wd();
        i->crpcut_run_test_case();
        wrapped::exit(0);
      }

    // parent
    ++pending_children;
    i->crpcut_setup(pid,
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

  const char *test_case_factory::do_get_parameter(const char *name) const
  {
    for (const char** p = argv; *p; ++p)
      {
        if ((*p)[0] == '-' && (*p)[1] == 'p')
          {
            const char *v = *++p;
            const char *n = name;
            while (*n && *v == *n)
              {
                ++v;
                ++n;
              }
            if (*n == 0 && *v++ == '=')
              {
                return v;
              }
          }
      }
    return 0;
  }

  int test_case_factory::do_run(int argc, const char *argv_[],
                                std::ostream &err_os)
  {
    argv = argv_;
    const char *working_dir = 0;
    bool quiet = false;
    int output_fd = 1;
    bool xml = false;
    char process_limit_set = 0;
    const char **p = argv+1;
    while (const char *param = *p)
      {
        if (param[0] != '-') break;
        switch (param[1]) {
        case 'q':
          quiet = true;
          break;
        case 'o':
          ++p;
          if (!*p)
            {
              err_os << "-o must be followed by a filename\n";
              return -1;
            }
          output_fd = wrapped::open(*p, O_CREAT | O_WRONLY | O_TRUNC, 0666);
          if (output_fd < 0)
            {
              err_os << "Failed to open " << *p << " for writing\n";
              return -1;
            }
          xml = !xml;
          break;
        case 'v':
          verbose_mode = true;
          break;
        case 'c':
          if (process_limit_set)
            {
              err_os <<
                "The number of child processes is already limited with the -"
                     << process_limit_set << "flag\n";
              return -1;
            }
          ++p;
          if (*p)
            {
              stream::iastream i(*p);
              unsigned l;
              if ((i >> l) && l <= max_parallel && l > 0)
                {
                  num_parallel = l;
                  process_limit_set = 'c';
                  break;
                }
            }
          err_os <<
            "num child processes must be a positive integer no greater than "
                 << max_parallel
                 << "\n";
          return -1;
        case 's':
          if (process_limit_set)
            {
              err_os <<
                "The number of child processes is already limited with the -"
                     << process_limit_set << "flag\n";
              return -1;
            }
          num_parallel = 0;
          nodeps = true;
          process_limit_set = 's';
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
            for (implementation::crpcut_test_case_registrator *i
                   = reg.crpcut_get_next();
                 i != &reg;
                 i = i->crpcut_get_next())
              {
                bool matched = !*names;
                for (const char **name = names; !matched && *name; ++name)
                  {
                    matched = i->crpcut_match_name(*name);
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
          lib::strcpy(dirbase, working_dir);
          break;
        case 'n':
          nodeps = true;
          break;
        case 'x':
          xml = !xml;
          break;
        case 'p':
          // just make a syntax check here. What follows must be a name=value.
          {
            const char *n = *++p;
            while (*n && *n != '=')
              {
                ++n;
              }
            if (*n == 0)
              {
                err_os << "-p must be followed by a name and =\n";
                return -1;
              }
            break;
          }
        default:
          err_os <<
            "Usage: " << argv[0] << " [flags] {testcases}\n"
            "  where flags can be:\n"
            "   -c number  - Control number of spawned test case processes\n"
            "                number must be >= 1 and <= "
                      << max_parallel << "\n"
            "   -d dir     - specify working directory (must exist)\n"
            "   -l         - list test cases\n"
            "   -n         - ignore dependencies\n"
            "   -o file    - Direct XML output to named file. Brief result\n"
            "                will be displayed on stdout\n"
            "   -p var=val - Define a named variable for the test cases to\n"
            "                pick up\n"
            "   -q         - Don't display the -o brief\n"
            "   -s         - single shot. Run only one test case, and run it in\n"
            "                the main process\n"
            "   -v         - verbose mode\n"
            "   -x         - XML output on std-out or non-XML output on file\n";

          return -1;
        }
        ++p;
      }
    output::formatter &out = output_formatter(xml, output_fd, argc, argv);
    wrapped::getcwd(homedir, sizeof(homedir));
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
        {
          implementation::crpcut_test_case_registrator *i = reg.crpcut_get_next();
          while (i != &reg)
            {
              ++num_registered_tests;
              if (*names)
                {
                  for (const char **name = names; *name; ++name)
                    {
                      if (i->crpcut_match_name(*name)) goto found;
                    }
                  i = i->crpcut_unlink();
                  continue;
                }
            found:
              ++num_selected_tests;
              i = i->crpcut_get_next();
            }
        }
      for (;;)
        {
          bool progress = false;
          implementation::crpcut_test_case_registrator *i = reg.crpcut_get_next();
          while (i != &reg)
            {
              if (!nodeps && !i->crpcut_can_run())
                {
                  i = i->crpcut_get_next();
                  continue;
                }
              progress = true;
              start_test(i);
              i = i->crpcut_unlink();
              if (!tests_as_child_procs())
                {
                  return 0;
                }
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
      if (tests_as_child_procs())
        {
          kill_presenter_process();
          for (unsigned n = 0; n < max_parallel; ++n)
            {
              stream::toastream<std::numeric_limits<unsigned>::digits/3+1> name;
              name << n << '\0';
              (void)wrapped::rmdir(name.begin());
              // failure above is taken care of as error elsewhere
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
                  using datatypes::posix_error;
                  throw posix_error(errno,
                                    "chdir back from testcase working dir");
                }
              (void)wrapped::rmdir(dirbase); // ignore, taken care of as error
            }
        }

      if (reg.crpcut_get_next() != &reg)
        {
          if (output_fd != 1 && !quiet)
            {
              std::cout << "Blocked tests:\n";
            }
          for (implementation::crpcut_test_case_registrator *i
                 = reg.crpcut_get_next();
               i != &reg;
               i = i->crpcut_get_next())
            {
              out.blocked_test(i);
              if (output_fd != 1 && !quiet)
                {
                  std::cout << "  " << *i << '\n';
                }
            }
        }
          out.statistics(num_registered_tests,
                         num_selected_tests,
                         num_tests_run,
                         num_tests_run - num_successful_tests);
          if (output_fd != 1 && !quiet)
            {
              std::cout << "Total " << num_selected_tests
                        << " test cases selected"
                        << "\nUNTESTED : "
                        << num_selected_tests - num_tests_run
                        << "\nPASSED   : " << num_successful_tests
                        << "\nFAILED   : "
                        << num_tests_run - num_successful_tests;
              std::cout << std::endl;
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
