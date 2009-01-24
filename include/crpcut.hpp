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

#ifndef CRPCUT_HPP
#define CRPCUT_HPP

#include <stdexcept>
#include <sstream>
#include <iostream>
#include <ostream>
#include <cerrno>
#include <cassert>
#include <tr1/type_traits>
#include <tr1/array>
#include <cstring>
#include <cstdlib>

extern "C"
{
#include <limits.h>
}

namespace std {
  using namespace std::tr1;
}

#define NO_CORE_FILE \
  protected virtual crpcut::policies::no_core_file

#define EXPECT_EXIT(num) \
  protected virtual crpcut::policies::exit_death<num>

#define EXPECT_SIGNAL_DEATH(num) \
  protected virtual crpcut::policies::signal_death<num>

#define EXPECT_EXCEPTION(type) \
  protected virtual crpcut::policies::exception_specifier<void (type)>

#define DEPENDS_ON(...) \
  protected virtual crpcut::policies::dependency_policy<crpcut::datatypes::tlist_maker<__VA_ARGS__>::type >

#define ANY_CODE -1


#define DEADLINE_CPU_MS(time) \
  crpcut::policies::timeout_policy<crpcut::policies::timeout::cputime, time>

#define DEADLINE_REALTIME_MS(time) \
  crpcut::policies::timeout_policy<crpcut::policies::timeout::realtime, time>



namespace crpcut {
  class none {};

  namespace datatypes {

    template <typename T, std::size_t N>
    class array_v : private std::array<T, N>
    {
      typedef std::array<T, N> array;
    public:
      typedef typename array::value_type             value_type;
      typedef typename array::reference              reference;
      typedef typename array::const_reference        const_reference;
      typedef typename array::iterator               iterator;
      typedef typename array::const_iterator         const_iterator;
      typedef typename array::size_type              size_type;
      typedef typename array::difference_type        difference_type;
      typedef typename array::reverse_iterator       reverse_iterator;
      typedef typename array::const_reverse_iterator const_reverse_iterator;

      array_v();
      using array::assign;
      using array::begin;
      iterator end();
      const_iterator end() const;
      reverse_iterator rbegin();
      const_reverse_iterator rbegin() const;
      using array::rend;
      size_type size() const;
      using array::max_size;
      bool empty() const;

      using array::operator[];
      reference at(size_type n);
      const_reference at(size_type n) const;
      using array::front;
      reference back();
      const_reference back() const;
      using array::data;
      void push_back(const value_type &x);
      void pop_back();
    private:
      size_t num_elements;
    };

    class none;

    template <typename T1 = none, typename T2 = none>
    class tlist : public T1,
                  public T2
    {
    public:
      typedef T1 head;
      typedef T2 tail;
    };

    template <typename T>
    struct tlist<none, T>
    {
      typedef none head;
    };

    // Man, I'm longing for variadic templates... this is insanity
    template <typename T1 = none, typename T2 = none, typename T3 = none,
              typename T4 = none, typename T5 = none, typename T6 = none,
              typename T7 = none, typename T8 = none, typename T9 = none,
              typename T10 = none, typename T11 = none, typename T12 = none,
              typename T13 = none, typename T14 = none, typename T15 = none,
              typename T16 = none, typename T17 = none, typename T18 = none>
    struct tlist_maker
    {
      typedef tlist<
        T1,
        tlist<
          T2,
          tlist<
            T3,
            tlist<
              T4,
              tlist<
                T5,
                tlist<
                  T6,
                  tlist<
                    T7,
                    tlist<
                      T8,
                      tlist<
                        T9,
                        tlist<
                          T10,
                          tlist<
                            T11,
                            tlist<
                              T12,
                              tlist<
                                T13,
                                tlist<
                                  T14,
                                  tlist<
                                    T15,
                                    tlist<
                                      T16,
                                      tlist<
                                        T17,
                                        tlist<T18>
                                        >
                                      >
                                    >
                                  >
                                >
                              >
                            >
                          >
                        >
                      >
                    >
                  >
                >
              >
            >
          >
        >
      type;
    };

    template <template <typename> class envelope, typename T>
    class wrap
    {
    public:
      typedef datatypes::tlist<envelope<typename T::head>,
                               typename wrap<envelope,
                                             typename T::tail>::type> type;
    };

    template <template <typename> class envelope, typename T>
    class wrap<envelope, datatypes::tlist<none, T> >
    {
    public:
      typedef datatypes::tlist<> type;
    };

    template <typename T>
    const typename std::remove_cv<typename std::remove_reference<T>::type>::type&
    gettype();

  } // namespace datatypes

  namespace policies {
    namespace deaths {
      class none;
    }

    namespace dependencies {
      class none {};
    }

    namespace timeout {
      class none { };
    }

    class default_policy
    {
    protected:
      typedef void               crpcut_run_wrapper;
      typedef deaths::none       crpcut_expected_death_cause;
      typedef dependencies::none crpcut_dependency;
      typedef timeout::none      crpcut_timeout_enforcer;
    };

    namespace deaths {

      class none
      {
      public:
        virtual ~none() {}
        virtual bool is_expected_exit(int) const;
        virtual bool is_expected_signal(int) const;
        virtual void expected_death(std::ostream &os);
      };

      template <int N>
      class signal : public virtual none
      {
      public:
        virtual bool is_expected_signal(int code) const;
        virtual void expected_death(std::ostream &os);
      };

      template <int N>
      class exit : public virtual none
      {
      public:
        virtual bool is_expected_exit(int code) const;
        virtual void expected_death(std::ostream &os);
      };

      class wrapper;

    } // namespace deaths

    template <int N>
    class signal_death : protected virtual default_policy
    {
    public:
      typedef deaths::wrapper crpcut_run_wrapper;
      typedef deaths::signal<N> crpcut_expected_death_cause;
    };

    template <int N>
    class exit_death : protected virtual default_policy
    {
    public:
      typedef deaths::wrapper  crpcut_run_wrapper;
      typedef deaths::exit<N>  crpcut_expected_death_cause;
    };

    class any_exception_wrapper;

    template <typename exc>
    class exception_wrapper;

    template <typename T>
    class exception_specifier;

    template <typename T>
    class exception_specifier<void (T)> : protected virtual default_policy
    {
    public:
      typedef exception_wrapper<T> crpcut_run_wrapper;
    };

    template <>
    class exception_specifier<void (...)> : protected virtual default_policy
    {
    public:
      typedef any_exception_wrapper crpcut_run_wrapper;
    };

    class no_core_file : protected virtual default_policy
    {
    protected:
      no_core_file();
    };

    namespace dependencies {

      class basic_enforcer;
      class base
      {
      protected:
        void inc();
      public:
        base();
        void add(basic_enforcer * other);
        bool can_run() const;
        bool failed() const;
        bool succeeded() const;
        void register_success(bool value = true);
      private:
        enum { success, fail, not_run } state;
        int num;
        basic_enforcer *dependants;
      };

      class basic_enforcer : public virtual base
      {
        friend class base;
        basic_enforcer *next;
      protected:
        basic_enforcer();
      };


      template <typename T>
      class enforcer : private basic_enforcer
      {
      public:
        enforcer();
      };

    } // namespace dependencies

    template <typename T>
    class dependency_policy : protected virtual default_policy
    {
    public:
      typedef typename datatypes::wrap<dependencies::enforcer,
                                       T>::type  crpcut_dependency;
    };

    namespace timeout {

      typedef enum { realtime, cputime } type;

      template <type t, unsigned timeout_ms>
      class enforcer;

      class cputime_enforcer
      {
      protected:
        cputime_enforcer(unsigned timeout_ms);
        ~cputime_enforcer();
      private:

        unsigned duration_ms;
        unsigned start_timestamp_ms;
      };

      class monotonic_enforcer
      {
      protected:
        monotonic_enforcer(unsigned timeout_ms);
        ~monotonic_enforcer();
      private:

        unsigned duration_ms;
        unsigned start_timestamp_ms;
      };

      template <unsigned timeout_ms>
      class enforcer<cputime, timeout_ms> : public cputime_enforcer
      {
      public:
        enforcer();
      };

      template <unsigned timeout_ms>
      class enforcer<realtime, timeout_ms> : public monotonic_enforcer
      {
      public:
        enforcer();
      };

    } // namespace timeout


    template <timeout::type t, unsigned timeout_ms>
    class timeout_policy : protected virtual default_policy
    {
    public:
      typedef timeout::enforcer<t, timeout_ms> crpcut_timeout_enforcer;
    };


  } // namespace policies


  class test_case_base : protected virtual policies::default_policy
  {
  public:
    virtual ~test_case_base();
    void run();
  private:
    virtual void run_test() = 0;
  };

  class test_case_factory;

  namespace comm {

    typedef enum {
      exit_ok, exit_fail, dir,
      stdout, stderr,
      set_timeout, cancel_timeout,
      begin_test,
      end_test
    } type;

    // protocol is type -> size_t(length) -> char[length]. length may be 0.
    // reader acknowledges with length.

    class reporter
    {
      int write_fd;
      int read_fd;
    public:
      reporter();
      void set_fds(int read, int write);
      void operator()(type t, std::ostringstream &os) const;
      void operator()(type t, const char *msg) const;
      void operator()(type t, size_t len, const char *msg) const;
      template <typename T>
      void operator()(type t, const T& data) const;
    private:
      template <typename T>
      void write(const T& t) const;

      template <typename T>
      void read(T& t) const;
    };


    extern reporter report;

  } // namespace comm

  namespace implementation {

    struct namespace_info
    {
    public:
      namespace_info(const char *n, namespace_info *p);
      const char* match_name(const char *n) const;
      // returns 0 on mismatch, otherwise a pointer into n where namespace name
      // ended.

      friend std::ostream &operator<<(std::ostream &, const namespace_info &);
    private:
      const char *name;
      namespace_info *parent;
    };

    class test_case_registrator;
    class fdreader
    {
    public:
      bool read();
      test_case_registrator *get_registrator() const;
      void close();
      void unregister();
      virtual ~fdreader() {} // silence gcc, it's really not needed
    protected:
      fdreader(test_case_registrator *r, int fd_ = 0);
      void set_fd(int fd_);
      test_case_registrator *const reg;
    private:
      virtual bool do_read(int fd) = 0;
      int fd;
    };

    template <comm::type t>
    class reader : public fdreader
    {
    public:
      reader(test_case_registrator *r, int fd = 0);
      void set_fd(int fd);
    private:
      virtual bool do_read(int fd);
    };

    class report_reader : public fdreader
    {
    public:
      report_reader(test_case_registrator *r);
      void set_fds(int in_fd, int out_fd);
    private:
      virtual bool do_read(int fd);
      int response_fd;
    };

    class test_case_registrator : public virtual policies::deaths::none,
                                  public virtual policies::dependencies::base
    {
    public:
      typedef test_case_base &(*test_case_creator)();
      test_case_registrator(const char *name, test_case_creator func);
      friend std::ostream &operator<<(std::ostream &os,
                                      const test_case_registrator &t)
      {
        return t.print_name(os);
      }
      virtual bool match_name(const char *name) const = 0;
      test_case_base *instantiate_obj() const;
      void setup(pid_t pid,
                 int in_fd, int out_fd,
                 int stdout_fd,
                 int stderr_fd);
      void manage_death();
      test_case_registrator *unlink();
      void kill();
      int ms_until_deadline() const;
      void clear_deadline();
      bool deadline_is_set() const;
      static bool timeout_compare(const test_case_registrator *lh,
                                  const test_case_registrator *rh);
      void unregister_fds();
      test_case_registrator *get_next() const;
      void set_wd(int n);
      void goto_wd() const;
      pid_t get_pid() const;
      bool has_active_readers() const;
      void deactivate_reader();
      void activate_reader();
    protected:
      const char *name_;
      test_case_registrator();
    private:
      virtual std::ostream &print_name(std::ostream &) const = 0;

      test_case_registrator *next;
      test_case_registrator *prev;
      test_case_creator      func_;
      unsigned               active_readers;
      bool                   death_note;
      bool                   deadline_set;
      pid_t                  pid_;
      unsigned               absolute_deadline_ms;
      int                    dirnum;
      report_reader          rep_reader;
      reader<comm::stdout>   stdout_reader;
      reader<comm::stderr>   stderr_reader;

      friend class report_reader;
    };

  } // namespace implementation

  class test_case_factory
  {
  public:
    static const unsigned max_parallel = 8;

    static unsigned run_test(int argc, const char *argv[],
                             std::ostream &os = std::cerr);
    static void introduce_name(pid_t pid, const std::string &s);
    static void present(pid_t pid, comm::type t, size_t len, const char *buff);
    static bool tests_as_child_procs();
    static void set_deadline(implementation::test_case_registrator *i);
    static void clear_deadline(implementation::test_case_registrator *i);
    static void return_dir(int num);
    static const char *get_working_dir();
    static void test_succeeded(implementation::test_case_registrator*);
  private:
    static test_case_factory& obj();
    test_case_factory();
    void start_presenter_process();
    void kill_presenter_process();
    void manage_children(unsigned max_pending_children);
    void run_test_case(implementation::test_case_registrator *i) const;
    void start_test(implementation::test_case_registrator *i);

    unsigned do_run(int argc, const char *argv[], std::ostream &os);
    void do_present(pid_t pid, comm::type t, size_t len, const char *buff);
    void do_introduce_name(pid_t pid, const std::string &s);
    void do_set_deadline(implementation::test_case_registrator *i);
    void do_clear_deadline(implementation::test_case_registrator *i);
    void do_return_dir(int num);
    const char *do_get_working_dir() const;
    friend class implementation::test_case_registrator;

    class registrator_list : public implementation::test_case_registrator
    {
      virtual bool match_name(const char *) const { return false; }
      virtual std::ostream& print_name(std::ostream &os) const { return os; }
    };

    typedef datatypes::array_v<implementation::test_case_registrator*,
                               max_parallel> timeout_queue;


    registrator_list reg;
    unsigned         pending_children;
    bool             verbose_mode;
    bool             nodeps;
    unsigned         num_parallel;
    bool             single_process;
    unsigned         num_registered_tests;
    unsigned         num_tests_run;
    unsigned         num_successful_tests;
    pid_t            presenter_pid;
    int              presenter_pipe;
    timeout_queue    deadlines;
    int              working_dirs[max_parallel];
    int              first_free_working_dir;
    char             dirbase[PATH_MAX];
  };

  namespace implementation {

    template <comm::type t>
    bool reader<t>::do_read(int fd)
    {
      static char buff[1024];
      for (;;)
        {
          int rv = ::read(fd, buff, sizeof(buff));
          if (rv == 0) return false;
          if (rv == -1)
            {
              int n = errno;
              assert(n == EINTR);
              (void)n; // silence warning
            }

          test_case_factory::present(reg->get_pid(),
                                     t,
                                     rv,
                                     buff);
          return true;
        }
    }

    template <typename C, typename T>
    class test_wrapper;

    template <typename exc, typename T>
    class test_wrapper<policies::exception_wrapper<exc>, T>
    {
    public:
      static void run(T *t);
    };

    template <typename T>
    class test_wrapper<void, T>
    {
    public:
      static void run(T *t);
    };

    template <typename T>
    class test_wrapper<policies::deaths::wrapper, T>
    {
    public:
      static void run(T *t);
    };

    template <typename T>
    class test_wrapper<policies::any_exception_wrapper, T>
    {
    public:
      static void run(T* t)
      {
        try {
          t->test();
        }
        catch (...) {
          return;
        }
        comm::report(comm::exit_fail,
                     "Unexpectedly did not throw");
      }
    };

    template <typename exc, typename T>
    void test_wrapper<policies::exception_wrapper<exc>, T>::run(T* t)
    {
      try {
        t->test();
      }
      catch (exc&) {
        return;
      }
      catch (...) {
        comm::report(comm::exit_fail,
                     "Unexpectedly caught ...");
      }
      comm::report(comm::exit_fail,
                   "Unexpectedly did not throw");
    }

    template <typename T>
    void test_wrapper<void, T>::run(T *t)
    {
      t->test();
    }

    template <typename T>
    void test_wrapper<policies::deaths::wrapper, T>::run(T *t)
    {
      t->test();
      std::ostringstream os;
      os << "Unexpectedly survived\nExpected ";
      T::crpcut_reg.expected_death(os);
      comm::report(comm::exit_fail, os);
    }

  } // namespace implementation

  namespace stream_checker
  {
    template <typename T>
    class is_output_streamable
    {
      static std::ostream &os;
      static T& t;
      template <typename V, typename U>
      friend char operator<<(V&, const U&);

      static char check(char);
      static std::pair<char, char> check(std::ostream&);
    public:
      static const bool value = sizeof(check(os << t)) != sizeof(char);
    };


    template <typename T>
    struct is_output_streamable<const T>
    {
      static const bool value = is_output_streamable<T>::value;
    };

    template <typename T>
    struct is_output_streamable<volatile T>
    {
      static const bool value = is_output_streamable<T>::value;
    };

    template <typename T>
    struct is_output_streamable<T&>
    {
      static const bool value = is_output_streamable<T>::value;
    };

    template <size_t N>
    struct is_output_streamable<char[N]>
    {
      static const bool value = true;
    };

    template <size_t N>
    struct is_output_streamable<const char[N]>
    {
      static const bool value = true;
    };
  }

  namespace implementation {
    template <typename T,
              bool b = stream_checker::is_output_streamable<T>::value>
    struct conditional_streamer
    {
      static bool stream(std::ostream &os, const T& t)
      {
        os << t;
        return true;
      }
    };

    template <typename T>
    struct conditional_streamer<T, false>
    {
      static bool stream(std::ostream &, const T&)
      {
        return false;
      }
    };
  }

  template <typename T>
  bool conditionally_stream(std::ostream &os, const T& t)
  {
    return implementation::conditional_streamer<T>::stream(os, t);
  }

  template <typename T>
  bool stream_param(std::ostream &os,
                    const char *prefix,
                    const char *name, const T& t)
  {
    std::ostringstream tmp;
    bool v = conditionally_stream(tmp, t);
    if (v && tmp.str() != name)
      {
        static const char* oper[] = { " ", " = " };
        os << prefix << name << oper[v] << tmp.str();
        return true;
      }
    return false;
  }

  //// template and inline func implementations

  namespace datatypes {
    template <typename T, std::size_t N>
    inline array_v<T, N>::array_v()
      : num_elements(0)
    {
    }

    template <typename T, std::size_t N>
    inline typename array_v<T, N>::iterator
    array_v<T, N>::end()
    {
      return iterator(&operator[](size()));
    }

    template <typename T, std::size_t N>
    inline typename array_v<T, N>::const_iterator
    array_v<T, N>::end() const
    {
      return iterator(&operator[](size()));
    }

    template <typename T, std::size_t N>
    inline typename array_v<T, N>::reverse_iterator
    array_v<T, N>::rbegin()
    {
      return reverse_iterator(end());
    }

    template <typename T, std::size_t N>
    inline typename array_v<T, N>::const_reverse_iterator
    array_v<T, N>::rbegin() const
    {
      return reverse_iterator(end());
    }

    template <typename T, std::size_t N>
    inline typename array_v<T, N>::size_type
    array_v<T, N>::size() const
    {
      return num_elements;
    }

    template <typename T, std::size_t N>
    inline bool
    array_v<T, N>::empty() const
    {
      return size() == 0;
    }

    template <typename T, std::size_t N>
    inline typename array_v<T, N>::reference
    array_v<T, N>::at(size_type n)
    {
      if (n >= num_elements) throw std::out_of_range("array_v::at");
      return operator[](n);
    }

    template <typename T, std::size_t N>
    inline typename array_v<T, N>::const_reference
    array_v<T, N>::at(size_type n) const
    {
      if (n >= num_elements) throw std::out_of_range("array_v::at");
      return operator[](n);
    }

    template <typename T, std::size_t N>
    inline typename array_v<T, N>::reference
    array_v<T, N>::back()
    {
      return *(end() - !empty());
    }

    template <typename T, std::size_t N>
    inline typename array_v<T, N>::const_reference
    array_v<T, N>::back() const
    {
      *(end() - !empty());
    }

    template <typename T, std::size_t N>
    inline void
    array_v<T, N>::push_back(const value_type &x)
    {
      assert(num_elements <= N);
      operator[](num_elements++) = x;
    }

    template <typename T, std::size_t N>
    inline void
    array_v<T, N>::pop_back()
    {
      assert(num_elements);
      --num_elements;
    }
  } // namespace datatypes

  namespace policies {

    namespace deaths{

      inline bool
      none::is_expected_exit(int) const
      {
        return false;
      }

      inline bool
      none::is_expected_signal(int) const
      {
        return false;
      }

      template <int N>
      inline bool
      signal<N>::is_expected_signal(int code) const
      {
        return N == ANY_CODE || code == N;
      }

      template <int N>
      inline void
      signal<N>::expected_death(std::ostream &os)
      {
        if (N == ANY_CODE)
          {
            os << "any signal";
          }
        else
          {
            os << "signal " << N;
          }
      }

      template <int N>
      inline bool
      exit<N>::is_expected_exit(int code) const
      {
        return N == ANY_CODE || code == N;
      }

      template <int N>
      inline void
      exit<N>::expected_death(std::ostream &os)
      {
        if (N == ANY_CODE)
          {
            os << "exit with any code";
          }
        else
          {
            os << "exit with code " << N;
          }
      }

    } // namespace deaths

    namespace dependencies {

      inline
      base::base()
        : state(not_run),
          num(0),
          dependants(0)
      {
      }

      inline void
      base::add(basic_enforcer *other)
      {
        other->next = dependants;
        dependants = other;
      }

      inline void
      base::inc()
      {
        ++num;
      }

      inline bool
      base::can_run() const
      {
        return num == 0;
      }

      inline bool
      base::failed() const
      {
        return state == fail;
      }

      inline bool
      base::succeeded() const
      {
        return state == success;
      }

      inline
      basic_enforcer::basic_enforcer()
        : next(0)
      {
      }

      template <typename T>
      inline
      enforcer<T>::enforcer()
      {
        inc();
        T::crpcut_reg.add(this);
      }
    } // namespace dependencies

    namespace timeout {

      template <unsigned timeout_ms>
      inline
      enforcer<cputime, timeout_ms>::enforcer()
        : cputime_enforcer(timeout_ms)
      {
      }

      template <unsigned timeout_ms>
      inline
      enforcer<realtime, timeout_ms>::enforcer()
        : monotonic_enforcer(timeout_ms)
      {
      }
    } // namespace timeout

  } // namespace policies

  inline
  test_case_base::~test_case_base()
  {
  }

  inline void
  test_case_base::run()
  {
    run_test();
  }

  namespace comm {

    inline
    reporter::reporter()
      : write_fd(0),
        read_fd(0)
    {
    }

    inline void
    reporter::set_fds(int read, int write)
    {
      write_fd = write;
      read_fd = read;
    }

    inline void
    reporter::operator()(type t, std::ostringstream &os) const
    {
      const std::string &s = os.str();
      operator()(t, s.length(), s.c_str());
    }

    inline void
    reporter::operator()(type t, const char *msg) const
    {
      operator()(t, std::strlen(msg), msg);
    }

    template <typename T>
    void
    reporter::write(const T& t) const
    {
      const size_t len = sizeof(T);
      size_t bytes_written = 0;
      const char *p = static_cast<const char*>(static_cast<const void*>(&t));
      while (bytes_written < len)
        {
          int rv = ::write(write_fd,
                           p + bytes_written,
                           len - bytes_written);
          if (rv == -1 && errno == EINTR) continue;
          if (rv <= 0) throw "write failed";
          bytes_written += rv;
        }
    }

    template <typename T>
    void
    reporter::operator()(comm::type t, const T& data) const
    {
      assert(test_case_factory::tests_as_child_procs());
      write(t);
      size_t len = sizeof(data);
      write(len);
      write(data);
      read(len);
      assert(len == sizeof(data));
    }

    template <typename T>
    void
    reporter::read(T& t) const
    {
      const size_t len = sizeof(T);
      size_t bytes_read = 0;
      char *p = static_cast<char*>(static_cast<void*>(&t));
      while (bytes_read < len)
        {
          int rv = ::read(read_fd,
                          p + bytes_read,
                          len - bytes_read);
          if (rv == -1 && errno == EINTR) continue;
          if (rv <= 0) {
            throw "read failed";
          }
          bytes_read += rv;
        }
    }
  } // namespace comm

  namespace implementation {

    inline
    namespace_info::namespace_info(const char *n, namespace_info *p)
      : name(n),
        parent(p)
    {
    }


    inline bool
    fdreader::read()
    {
      return do_read(fd);
    }

    inline test_case_registrator *
    fdreader::get_registrator() const
    {
      return reg;
    }

    inline void
    fdreader::close()
    {
      if (fd)
        {
          ::close(fd);
        }
      unregister();
    }

    inline
    fdreader::fdreader(test_case_registrator *r, int fd_)
      : reg(r),
        fd(fd_)
    {
    }

    template <comm::type t>
    inline
    reader<t>::reader(test_case_registrator *r, int fd)
      : fdreader(r, fd)
    {
    }

    template <comm::type t>
    inline void
    reader<t>::set_fd(int fd)
    {
      fdreader::set_fd(fd);
    }

    inline
    report_reader::report_reader(test_case_registrator *r)
      : fdreader(r)
    {
    }

    inline void
    report_reader::set_fds(int in_fd, int out_fd)
    {
      fdreader::set_fd(in_fd);
      response_fd = out_fd;
    }

    inline test_case_base *
    test_case_registrator::instantiate_obj() const
    {
      return &func_();
    }

    inline test_case_registrator *
    test_case_registrator::unlink()
    {
      next->prev = prev;
      prev->next = next;
      return next;
    }

    inline bool
    test_case_registrator::deadline_is_set() const
    {
      return deadline_set;
    }

    inline bool
    test_case_registrator::timeout_compare(const test_case_registrator *lh,
                                           const test_case_registrator *rh)
    {
      assert(lh->deadline_set);
      assert(rh->deadline_set);

      int diff = lh->absolute_deadline_ms - rh->absolute_deadline_ms;
      return diff >= 0;
    }

    inline test_case_registrator *
    test_case_registrator::get_next() const
    {
      return next;
    }

    inline pid_t
    test_case_registrator::get_pid() const
    {
      return pid_;
    }

    inline bool
    test_case_registrator::has_active_readers() const
    {
      return active_readers > 0U;
    }

    inline void
    test_case_registrator::deactivate_reader()
    {
      --active_readers;
    }

    inline void
    test_case_registrator::activate_reader()
    {
      ++active_readers;
    }

    inline
    test_case_registrator::test_case_registrator()
      : next(this),
        prev(this),
        active_readers(0),
        death_note(false),
        deadline_set(false),
        rep_reader(0),
        stdout_reader(0),
        stderr_reader(0)
    {
    }

  } // implementation

  inline unsigned
  test_case_factory::run_test(int argc, const char *argv[],
                                              std::ostream &os)
  {
    return obj().do_run(argc, argv, os);
  }

  inline void
  test_case_factory::introduce_name(pid_t pid, const std::string &s)
  {
    obj().do_introduce_name(pid, s);
  }

  inline void
  test_case_factory::present(pid_t pid, comm::type t,
                             size_t len, const char *buff)
  {
    obj().do_present(pid, t, len, buff);
  }

  inline bool
  test_case_factory::tests_as_child_procs()
  {
    return obj().num_parallel > 0;
  }

  inline void
  test_case_factory::set_deadline(implementation::test_case_registrator *i)
  {
    obj().do_set_deadline(i);
  }

  inline void
  test_case_factory::clear_deadline(implementation::test_case_registrator *i)
  {
    obj().do_clear_deadline(i);
  }

  inline void
  test_case_factory::return_dir(int num)
  {
    obj().do_return_dir(num);
  }

  inline const char *
  test_case_factory::get_working_dir()
  {
    return obj().do_get_working_dir();
  }

  inline void
  test_case_factory::test_succeeded(implementation::test_case_registrator*)
  {
    ++obj().num_successful_tests;
  }

  inline test_case_factory &
  test_case_factory::obj()
  {
    static test_case_factory f;
    return f;
  }

  inline const char *
  test_case_factory::do_get_working_dir() const
  {
    return dirbase;
  }
} // namespace crpcut

#define CRPCUT_TEST_CASE_DEF(test_case_name, ...)                           \
  class test_case_name                                                      \
    : crpcut::test_case_base, __VA_ARGS__                                   \
  {                                                                         \
    friend class crpcut::implementation::test_wrapper<crpcut_run_wrapper, test_case_name>; \
    friend class crpcut::policies::dependencies::enforcer<test_case_name>;  \
  virtual void run_test()                                                   \
  {                                                                         \
      crpcut_timeout_enforcer obj;                                          \
      (void)obj; /* silence warning */                                      \
      using crpcut::implementation::test_wrapper;                           \
      test_wrapper<crpcut_run_wrapper, test_case_name>::run(this);          \
    }                                                                       \
    void test();                                                            \
    static crpcut::test_case_base& creator()                                \
    {                                                                       \
      static test_case_name obj;                                            \
      return obj;                                                           \
    }                                                                       \
    class registrator                                                       \
      : public crpcut::implementation::test_case_registrator,               \
        public virtual crpcut::policies::dependencies::base,                \
        public virtual test_case_name::crpcut_expected_death_cause,        \
        private virtual test_case_name::crpcut_dependency                   \
          {                                                                 \
            typedef crpcut::implementation::test_case_registrator           \
              registrator_base;                                             \
          public:                                                           \
            registrator()                                                   \
              : registrator_base(#test_case_name,                           \
                                 &test_case_name::creator)                  \
              {                                                             \
              }                                                             \
          private:                                                          \
            virtual bool match_name(const char *name_param) const           \
            {                                                               \
              const char *p = current_namespace.match_name(name_param);     \
              if (p)                                                        \
                {                                                           \
                  if (p != name_param || *p == ':')                         \
                    {                                                       \
                      if (!*p) return true; /* match for whole suites*/     \
                      if (!*p++ == ':') return false;                       \
                      if (!*p++ == ':') return false;                       \
                    }                                                       \
                }                                                           \
              else                                                          \
                {                                                           \
                  p = name_param;                                           \
                }                                                           \
              return !std::strcmp(p, #test_case_name);                      \
            }                                                               \
            virtual std::ostream &print_name(std::ostream &os) const        \
            {                                                               \
              os << current_namespace;                                      \
              return os << #test_case_name;                                 \
            }                                                               \
          };                                                                \
    static registrator crpcut_reg;                                          \
  };                                                                        \

#define TEST_DEF(test_case_name, ...)                                   \
  CRPCUT_TEST_CASE_DEF(test_case_name, __VA_ARGS__)                     \
  test_case_name :: registrator test_case_name::crpcut_reg;             \
  void test_case_name::test()

#define DISABLED_TEST_DEF(test_case_name, ...)                          \
  CRPCUT_TEST_CASE_DEF(test_case_name, __VA_ARGS__)                     \
  void test_case_name::test()

#define CRPCUT_CONCAT(a, b) a ## b

#define CRPCUT_CONCAT_(a, b) CRPCUT_CONCAT(a,b)

#define CRPCUT_LOCAL_NAME(prefix) \
  CRPCUT_CONCAT_(crpcut_local_  ## prefix ## _, __LINE__)

#define CRPCUT_STRINGIZE(a) #a
#define CRPCUT_STRINGIZE_(a) CRPCUT_STRINGIZE(a)

#define CRPCUT_REFTYPE(expr) \
  decltype(crpcut::datatypes::gettype<decltype(expr)>())


#define CRPCUT_BINARY_ASSERT(name, oper, lh, rh)                        \
  do {                                                                  \
    CRPCUT_REFTYPE(lh) CRPCUT_LOCAL_NAME(rl) = lh;                      \
    CRPCUT_REFTYPE(rh) CRPCUT_LOCAL_NAME(rr) = rh;                      \
    if (!(CRPCUT_LOCAL_NAME(rl) oper CRPCUT_LOCAL_NAME(rr)))            \
      {                                                                 \
        std::ostringstream CRPCUT_LOCAL_NAME(os);                       \
        CRPCUT_LOCAL_NAME(os) <<                                        \
          __FILE__ ":" CRPCUT_STRINGIZE_(__LINE__)                      \
          "\nASSERT_" #name "(" #lh ", " #rh ")";                       \
        static const char* CRPCUT_LOCAL_NAME(prefix)[] =                \
          { "\n  where ", "\n        " };                               \
        bool CRPCUT_LOCAL_NAME(printed) = false;                        \
        CRPCUT_LOCAL_NAME(printed) |=                                   \
          crpcut::stream_param(CRPCUT_LOCAL_NAME(os),                   \
                               CRPCUT_LOCAL_NAME(prefix)[CRPCUT_LOCAL_NAME(printed)], \
                               #lh,                                     \
                               CRPCUT_LOCAL_NAME(rl));                  \
        CRPCUT_LOCAL_NAME(printed) |=                                   \
          crpcut::stream_param(CRPCUT_LOCAL_NAME(os),                   \
                               CRPCUT_LOCAL_NAME(prefix)[CRPCUT_LOCAL_NAME(printed)], \
                               #rh,                                     \
                               CRPCUT_LOCAL_NAME(rr));                  \
        crpcut::comm::report(crpcut::comm::exit_fail, CRPCUT_LOCAL_NAME(os)); \
      }                                                                 \
  } while(0)

#define ASSERT_TRUE(a)                                                  \
  do {                                                                  \
    CRPCUT_REFTYPE(a) CRPCUT_LOCAL_NAME(ra) = a;                        \
    if (CRPCUT_LOCAL_NAME(ra))                                          \
      {                                                                 \
      }                                                                 \
    else                                                                \
      {                                                                 \
        std::ostringstream CRPCUT_LOCAL_NAME(os);                       \
        CRPCUT_LOCAL_NAME(os) <<                                        \
          __FILE__ ":" CRPCUT_STRINGIZE_(__LINE__)                      \
          "\nASSERT_TRUE(" #a ")\n";                                    \
        crpcut::stream_param(CRPCUT_LOCAL_NAME(os),                     \
                             "  where ",                                \
                             #a,                                        \
                             CRPCUT_LOCAL_NAME(ra));                    \
        crpcut::comm::report(crpcut::comm::exit_fail, CRPCUT_LOCAL_NAME(os)); \
      }                                                                 \
  } while(0)



#define ASSERT_FALSE(a)                                                 \
  do {                                                                  \
    CRPCUT_REFTYPE(a) CRPCUT_LOCAL_NAME(ra) = a;                        \
    if (CRPCUT_LOCAL_NAME(ra))                                          \
      {                                                                 \
        std::ostringstream CRPCUT_LOCAL_NAME(os);                       \
        CRPCUT_LOCAL_NAME(os) <<                                        \
          __FILE__ ":" CRPCUT_STRINGIZE_(__LINE__)                      \
          "\nASSERT_FALSE(" #a ")\n";                                   \
        crpcut::stream_param(CRPCUT_LOCAL_NAME(os),                     \
                             "  where ",                                \
                             #a,                                        \
                             CRPCUT_LOCAL_NAME(ra));                    \
        crpcut::comm::report(crpcut::comm::exit_fail, CRPCUT_LOCAL_NAME(os)); \
      }                                                                 \
  } while(0)

#define ASSERT_EQ(lh, rh) \
  CRPCUT_BINARY_ASSERT(EQ, ==, lh, rh)

#define ASSERT_NE(lh, rh) \
  CRPCUT_BINARY_ASSERT(NE, !=, lh, rh)

#define ASSERT_GE(lh, rh) \
  CRPCUT_BINARY_ASSERT(GE, >=, lh, rh)

#define ASSERT_GT(lh, rh) \
  CRPCUT_BINARY_ASSERT(GT, >, lh, rh)

#define ASSERT_LT(lh, rh) \
  CRPCUT_BINARY_ASSERT(LT, <, lh, rh)

#define ASSERT_LE(lh, rh) \
  CRPCUT_BINARY_ASSERT(LE, <=, lh, rh)


#define ASSERT_THROW(expr, exc)                                         \
  do {                                                                  \
    try {                                                               \
      expr;                                                             \
      std::ostringstream CRPCUT_LOCAL_NAME(os);                         \
      CRPCUT_LOCAL_NAME(os) <<                                          \
        __FILE__ ":" CRPCUT_STRINGIZE_(__LINE__)                        \
        "\nASSERT_THROW(" #expr ", " #exc ")\n"                         \
        "  Did not throw";                                              \
      crpcut::comm::report(crpcut::comm::exit_fail,                     \
                           CRPCUT_LOCAL_NAME(os));                      \
    }                                                                   \
    catch (exc) {                                                       \
    }                                                                   \
  } while (0)

#define ASSERT_NO_THROW(expr)                                           \
  do {                                                                  \
    try {                                                               \
      expr;                                                             \
    }                                                                   \
    catch (std::exception &CRPCUT_LOCAL_NAME(e)) {                      \
      std::ostringstream CRPCUT_LOCAL_NAME(os);                         \
      CRPCUT_LOCAL_NAME(os) <<                                          \
        __FILE__ ":" CRPCUT_STRINGIZE_(__LINE__)                        \
        "\nASSERT_NO_THROW(" #expr ")\n"                                \
        "  caught std::exception\n"                                     \
        "  what()=" << CRPCUT_LOCAL_NAME(e).what();                     \
      crpcut::comm::report(crpcut::comm::exit_fail, CRPCUT_LOCAL_NAME(os)); \
    }                                                                   \
    catch (...) {                                                       \
      std::ostringstream CRPCUT_LOCAL_NAME(os);                         \
      CRPCUT_LOCAL_NAME(os) <<                                          \
        __FILE__ ":" CRPCUT_STRINGIZE_(__LINE__)                        \
        "\nASSERT_NO_THROW(" #expr ")\n"                                \
        "  caught ...";                                                 \
      crpcut::comm::report(crpcut::comm::exit_fail,                     \
                           CRPCUT_LOCAL_NAME(os));                      \
    }                                                                   \
  } while (0)

extern crpcut::implementation::namespace_info current_namespace;

#define TEST(...) TEST_DEF(__VA_ARGS__, crpcut::none)
#define DISABLED_TEST(...) DISABLED_TEST_DEF(__VA_ARGS__, crpcut::none)

#define TESTSUITE(name)                                                 \
  namespace name {                                                      \
    namespace {                                                         \
      static crpcut::implementation::namespace_info *parent_namespace   \
      = &current_namespace;                                             \
    }                                                                   \
    static crpcut::implementation::namespace_info                       \
    current_namespace(#name, parent_namespace);                         \
  }                                                                     \
  namespace name

#endif // CRPCUT_HPP
