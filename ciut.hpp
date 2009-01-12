#ifndef CIUT_HPP
#define CIUT_HPP

#include <stdexcept>
#include <sstream>
#include <iostream>
#include <ostream>
#include <iomanip>
#include <cerrno>
#include <cassert>
#include <tr1/type_traits>
#include "array_v.hpp"
#include <queue>
#include <cmath>
#include <ctime>
extern "C"
{
#include <sys/resource.h>
#include <sys/time.h>
}
namespace std {
  using namespace std::tr1;
}

struct siginfo;

#define NO_CORE_FILE                                    \
  protected virtual ciut::policies::no_core_file

#define EXPECT_EXIT(num)                                \
  protected virtual ciut::policies::exit_death<num>

#define EXPECT_SIGNAL_DEATH(num)                        \
  protected virtual ciut::policies::signal_death<num>

#define EXPECT_EXCEPTION(type)                                  \
  protected virtual ciut::policies::exception_specifier<type>

#define DEPENDS_ON(...)                                                 \
  protected virtual ciut::policies::dependency_policy<ciut::policies::dependencies::tuple_maker<__VA_ARGS__>::type >

#define ANY_CODE -1

#if defined(CLOCK_PROCESS_CPUTIME_ID)
#define DEADLINE_CPU_MS(time) ciut::policies::timeout_policy<ciut::policies::timeout::cputime, time>
#endif
#if defined(CLOCK_MONOTONIC)
#define DEADLINE_REALTIME_MS(time) ciut::policies::timeout_policy<ciut::policies::timeout::realtime, time>
#endif


namespace ciut {

  namespace xml {
    class tag_t
    {
      template <typename T>
      struct attr
      {
        attr(const char *n, const T& v) : name(n), val(v) {}
        const char *name;
        const T&    val;
        friend std::ostream &operator<<(std::ostream &os, const attr& a)
        {
          os << " " << a.name << "=\"" << a.val << "\""; return os;
        }
      };
    public:
      operator void*() const { return 0; }
      tag_t(const char *name, std::ostream &os)
        : name_(name),
          state_(in_name),
          indent_(0),
          os_(os),
          parent_(0)
      {
        introduce();
      }
      tag_t(const char *name, tag_t &parent)
        : name_(name),
          state_(in_name),
          indent_(parent.indent_+1),
          os_(parent.os_),
          parent_(&parent)
      {
        introduce();
        parent.state_ = in_children;
      }
      template <typename T1>
      tag_t(const char *name, tag_t &parent, const attr<T1>& a1)
        : name_(name),
          state_(in_name),
          indent_(parent.indent_+1),
          os_(parent.os_),
          parent_(&parent)
      {
        introduce();
        parent.state_ = in_children;
        os_ << a1;
      }
      template <typename T1, typename T2>
      tag_t(const char *name, tag_t &parent, const attr<T1>& a1, const attr<T2> &a2)
        : name_(name),
          state_(in_name),
          indent_(parent.indent_+1),
          os_(parent.os_),
          parent_(&parent)
      {
        introduce();
        parent.state_ = in_children;
        os_ << a1 << a2;
      }
      template <typename T1, typename T2, typename T3>
      tag_t(const char *name, tag_t &parent, const attr<T1>& a1, const attr<T2> &a2, const attr<T3> &a3)
        : name_(name),
          state_(in_name),
          indent_(parent.indent_+1),
          os_(parent.os_),
          parent_(&parent)
      {
        introduce();
        parent.state_ = in_children;
        os_ << a1 << a2 << a3;
      }
      template <typename T1, typename T2, typename T3, typename T4>
      tag_t(const char *name, tag_t &parent, const attr<T1>& a1, const attr<T2> &a2, const attr<T3> &a3, const attr<T4> &a4)
        : name_(name),
          state_(in_name),
          indent_(parent.indent_+1),
          os_(parent.os_),
          parent_(&parent)
      {
        introduce();
        parent.state_ = in_children;
        os_ << a1 << a2 << a3 << a4;
      }
      ~tag_t()
      {
        if (state_ == in_name)
          {
            os_ << "/>\n";
          }
        else
          {
            if (state_ == in_children)
              {
                os_  << std::setw(indent_*2) << "";
              }
            os_ << "</" << name_ << ">\n";
          }
      }
      template <typename T>
      tag_t & operator<<(const T& t)
      {
        std::ostringstream o;
        if (conditionally_stream(o, t))
          {
            if (state_ != in_data)
              {
                os_ << ">";
                state_ = in_data;
              }
            const std::string &s = o.str();
            for (std::string::const_iterator i = s.begin(); i != s.end(); ++i)
              {
                unsigned char u = *i;
                switch (u)
                  {
                  case '&':
                    os_ << "&amp;"; break;
                  case '<':
                    os_ << "&lt;"; break;
                  case '>':
                    os_ << "&gt;"; break;
                  case '"':
                    os_ << "&quot;"; break;
                  case '\'':
                    os_ << "&apos;"; break;
                  default:
                    if (u < 128)
                      {
                    os_ << *i;
                      }
                    else
                      {
                        os_ << "&#" << int(u) << ';';
                      }
                  }
              }
          }
        return *this;
      }
    private:
      void introduce()
      {
        if (parent_ && parent_->state_ != in_data)
          {
            if (parent_->state_ == in_name)
              {
                os_ << ">\n";
              }
            parent_->state_ = in_children;
            os_ << std::setw(indent_ * 2) << "";
          }
        os_ << '<' << name_;
      }

      const char *name_;
      enum { in_name, in_children, in_data } state_;
      int indent_;
      std::ostream &os_;
      tag_t *parent_;

      template <typename T>
      friend tag_t::attr<T> attr(const char *name, const T& t)
      {
        tag_t::attr<T> a(name, t);
        return a;
      }
    };
  }
  namespace policies {

    namespace deaths {
      class none;
    }

    namespace dependencies {
      class none {};
    }

    namespace timeout {
      class no_enforcer
      {
      };
    }

    class default_policy
    {
    protected:
      typedef void ciut_run_wrapper;
      typedef deaths::none ciut_expected_death_cause;
      typedef dependencies::none ciut_dependency;
      typedef timeout::no_enforcer ciut_timeout_enforcer;
    };

    namespace deaths {

      class none
      {
      public:
        virtual ~none() {}
        virtual bool is_expected_exit(int) const { return false; }
        virtual bool is_expected_signal(int) const { return false; }
      };

      template <int N>
      class signal : public virtual none
      {
      public:
        virtual bool is_expected_signal(int code) const {
          return N == ANY_CODE || code == N;
        }
      };

      template <int N>
      class exit : public virtual none
      {
      public:
        virtual bool is_expected_exit(int code) const {
          return N == ANY_CODE || code == N;
        }
      };

      class wrapper;

    } // namespace deaths

    template <int N>
    class signal_death : protected virtual default_policy
    {
    public:
      typedef deaths::wrapper ciut_run_wrapper;
      typedef deaths::signal<N> ciut_expected_death_cause;
    };

    template <int N>
    class exit_death : protected virtual default_policy
    {
    public:
      typedef deaths::wrapper  ciut_run_wrapper;
      typedef deaths::exit<N>  ciut_expected_death_cause;
    };

    template <typename exc>
    class exception_wrapper;

    template <typename T>
    class exception_specifier : protected virtual default_policy
    {
    public:
      typedef exception_wrapper<T> ciut_run_wrapper;
    };

    class no_core_file : protected virtual default_policy
    {
    protected:
      no_core_file();
    };

    namespace dependencies {

      class none;
      class basic_enforcer;
      class base
      {
      protected:
        void inc() { ++num; }
      public:
        base() : successful(false), num(0), dependants(0) {};
        void add(basic_enforcer * other);
        bool can_run() const { return num == 0; }
        bool failed() const { return !successful; }
        void register_success();
      private:
        bool successful;
        int num;
        basic_enforcer *dependants;
      };

      class basic_enforcer : public virtual base
      {
        friend class base;
        basic_enforcer *next;
      protected:
        basic_enforcer() : next(0) {}
      };

      inline void base::add(basic_enforcer *other)
      {
        other->next = dependants;
        dependants = other;
      }

      inline void base::register_success()
      {
        if (!successful)
          {
            successful = true;
            for (basic_enforcer *p = dependants; p; p = p->next)
              {
                --p->num;
              }
          }
      }
      template <typename T>
      class enforcer : private basic_enforcer
      {
      public:
        enforcer() { inc(); T::reg.add(this); }
      };

      template <typename T1 = none, typename T2 = none>
      class tuple : public T1,
                    public T2
      {
      public:
        typedef T1 head;
        typedef T2 tail;
      };

      template <typename T>
      class tuple<none, T>
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
      struct tuple_maker
      {
        typedef tuple<
          T1,
          tuple<
            T2,
            tuple<
              T3,
              tuple<
                T4,
                tuple<
                  T5,
                  tuple<
                    T6,
                    tuple<
                      T7,
                      tuple<
                        T8,
                        tuple<
                          T9,
                          tuple<
                            T10,
                            tuple<
                              T11,
                              tuple<
                                T12,
                                tuple<
                                  T13,
                                  tuple<
                                    T14,
                                    tuple<
                                      T15,
                                      tuple<
                                        T16,
                                        tuple<
                                          T17,
                                          tuple<T18>
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
        typedef tuple<envelope<typename T::head>, typename wrap<envelope, typename T::tail>::type> type;
      };

      template <template <typename> class envelope, typename T>
      class wrap<envelope, tuple<none, T> >
      {
      public:
        typedef tuple<> type;
      };
    } // namespace dependencies

    template <typename T>
    class dependency_policy : protected virtual default_policy
    {
    public:
      typedef typename dependencies::wrap<dependencies::enforcer, T>::type ciut_dependency;
    };

    namespace timeout {

      typedef enum { realtime, cputime } type;

      class basic_enforcer
      {
      protected:
        basic_enforcer(type t, unsigned timeout_ms);
        void check(type t, unsigned timeout_ms);
        timespec ts;
      };
      template <type t, unsigned timeout_ms>
      class enforcer : private basic_enforcer
      {
      public:
        enforcer();
      };
#if defined(CLOCK_PROCESS_CPUTIME_ID)
      template <unsigned timeout_ms>
      class enforcer<cputime, timeout_ms> : public basic_enforcer
      {
      public:
        enforcer()  : basic_enforcer(cputime, timeout_ms)
        {
          rlimit r = { (timeout_ms + 1500) / 1000, (timeout_ms + 2500) / 1000 };
          setrlimit(RLIMIT_CPU, &r);
        }
        ~enforcer() { basic_enforcer::check(cputime, timeout_ms); }
      };
#endif
#if defined(CLOCK_MONOTONIC)
      template <unsigned timeout_ms>
      class enforcer<realtime, timeout_ms> : public basic_enforcer
      {
      public:
        enforcer();
        ~enforcer();
      };
#endif
    } // namespace timeout


    template <timeout::type t, unsigned timeout_ms>
    class timeout_policy : protected virtual default_policy
    {
    public:
      typedef timeout::enforcer<t, timeout_ms> ciut_timeout_enforcer;
    };


  } // namespace policies


  class test_case_base : protected virtual policies::default_policy
  {
  public:
    virtual ~test_case_base() {}
    void run() {
      run_test();
    }
  private:
    virtual void run_test() = 0;
  };

  class test_case_factory;

  namespace comm {

    typedef enum {
      exit_ok, exit_fail,
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
      reporter() : write_fd(0), read_fd(0) {}
      void set_fds(int read, int write) { write_fd = write, read_fd = read; }
      void operator()(type t, std::ostringstream &os) const
      {
        const std::string &s = os.str();
        operator()(t, s.length(), s.c_str());
      }
      void operator()(type t, const char *msg) const
      {
        operator()(t, std::strlen(msg), msg);
      }
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

  namespace policies
  {
    namespace timeout
    {
      template <unsigned timeout_ms>
      enforcer<realtime, timeout_ms>::enforcer()
        : basic_enforcer(realtime, timeout_ms)
      {
        timespec deadline = ts;
        deadline.tv_nsec += (timeout_ms % 1000) * 1000000;
        deadline.tv_sec += deadline.tv_nsec / 1000000000;
        deadline.tv_nsec %= 1000000000;
        deadline.tv_sec += timeout_ms / 1000 + 1;
        // calculated deadline + 1 sec should give plenty of slack
        report(comm::set_timeout, deadline);
      }
      template <unsigned timeout_ms>
      enforcer<realtime, timeout_ms>::~enforcer()
      {
        report(comm::cancel_timeout, 0, 0);
        basic_enforcer::check(realtime, timeout_ms);
      }
    }
  }
  namespace implementation {

    struct namespace_info
    {
    public:
      namespace_info(const char *n, namespace_info *p) : name(n), parent(p) {}
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
      bool read() { return do_read(fd); }
      test_case_registrator *get_registrator() const { return reg; }
      void close() { if (fd) { ::close(fd); } unregister(); }
      void unregister();
      virtual ~fdreader() {} // silence gcc, it's really not needed
    protected:
      fdreader(test_case_registrator *r, int fd_ = 0) : reg(r), fd(fd_) {}
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
      reader(test_case_registrator *r, int fd = 0) : fdreader(r, fd) {}
      void set_fd(int fd)
      {
        fdreader::set_fd(fd);
      }

    private:
      virtual bool do_read(int fd);
    };

    class report_reader : public fdreader
    {
    public:
      report_reader(test_case_registrator *r) : fdreader(r) {}
      void set_fds(int in_fd, int out_fd)
      {
        fdreader::set_fd(in_fd);
        response_fd = out_fd;
      }
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
      bool has_obituary() const { return death_note; }
      virtual bool match_name(const char *name) const = 0;
      test_case_base *instantiate_obj() const { return &func_(); }
      void setup(pid_t pid,
                 int in_fd, int out_fd,
                 int stdout_fd,
                 int stderr_fd);
      void manage_death();
      test_case_registrator *unlink() {
        next->prev = prev;
        prev->next = next;
        return next;
      }
      void kill();
      int ms_until_deadline() const
      {
        struct timespec now;
        ::clock_gettime(CLOCK_MONOTONIC, &now);
        int ms = (deadline.tv_sec - now.tv_sec)*1000;
        if (ms < 0) return 0;
        ms+= (deadline.tv_nsec - now.tv_nsec) / 1000000;
        if (ms < 0) return 0;
        return ms;
      }
      void clear_deadline();
      bool deadline_is_set() const { return deadline.tv_sec > 0; }
      static bool timeout_compare(const test_case_registrator *lh,
                                  const test_case_registrator *rh)
      {
        if (lh->deadline.tv_sec == rh->deadline.tv_sec)
          return lh->deadline.tv_nsec > rh->deadline.tv_nsec;
        return lh->deadline.tv_sec > rh->deadline.tv_sec;
      }
      void unregister_fds();
      test_case_registrator *get_next() const { return next; }
      void set_wd(int n);
      void goto_wd() const;
      pid_t get_pid() const { return pid_; }
      bool has_active_readers() const { return active_readers > 0U; }
      void deactivate_reader() { --active_readers; }
      void activate_reader()   { ++active_readers; }
    protected:
      const char *name_;
      test_case_registrator()
        : next(this),
          prev(this),
          active_readers(0),
          death_note(false),
          rep_reader(0),
          stdout_reader(0),
          stderr_reader(0)
      {
        deadline.tv_sec = 0;
      }
    private:
      virtual std::ostream &print_name(std::ostream &) const = 0;

      test_case_registrator *next;
      test_case_registrator *prev;
      test_case_creator      func_;
      unsigned               active_readers;
      bool                   death_note;
      pid_t                  pid_;
      timespec               deadline;
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

    static unsigned run_test(int argc, const char *argv[], std::ostream &os = std::cerr)
    {
      return obj().do_run(argc, argv, os);
    }
    static void introduce_name(pid_t pid, const std::string &s)
    {
      obj().do_introduce_name(pid, s);
    };
    static void present(pid_t pid, comm::type t, size_t len, const char *buff)
    {
      obj().do_present(pid, t, len, buff);
    }
    static bool tests_as_child_procs()
    {
      return obj().num_parallel > 0;
    }
    static void set_deadline(implementation::test_case_registrator *i)
    {
      obj().do_set_deadline(i);
    }
    static void clear_deadline(implementation::test_case_registrator *i)
    {
      obj().do_clear_deadline(i);
    }
    static void return_dir(int num)
    {
      obj().do_return_dir(num);
    }
    static const char *get_working_dir()
    {
      return obj().do_get_working_dir();
    }
  private:
    static test_case_factory& obj() { static test_case_factory f; return f; }
    test_case_factory()
      : pending_children(0),
        verbose_mode(false),
        nodeps(false),
        num_parallel(1),
        num_registered_tests(0),
        num_tests_run(0),
        num_successful_tests(0),
        first_free_working_dir(0)
    {
      std::strcpy(dirbase, "/tmp/ciutXXXXXX");
      for (unsigned n = 0; n < max_parallel; ++n)
        {
          working_dirs[n] = n+1;
        }
    }
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
    const char *do_get_working_dir() const { return dirbase; }
    friend class implementation::test_case_registrator;

    class registrator_list : public implementation::test_case_registrator
    {
      virtual bool match_name(const char *) const { return false; }
      virtual std::ostream& print_name(std::ostream &os) const { return os; }
    };

    typedef array_v<implementation::test_case_registrator*,
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
        comm::report(comm::exit_fail, "<exception>\n  <caught type=\"...\"/>\n</exception>\n");
      }
      comm::report(comm::exit_fail, "<missing>\n  <exception/>\n</missing>\n");
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
      comm::report(comm::exit_fail, "<missing>\n  <exit/>\n</missing>\n");
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
    template <typename T, bool b = stream_checker::is_output_streamable<T>::value>
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
    if (tmp.str() != name)
      {
        static const char* oper[] = { " ", " = " };
        os << prefix << name << oper[v] << tmp.str();
        return true;
      }
    return false;
  }

  //// template func implementations


  namespace comm
  {
    template <typename T>
    void reporter::write(const T& t) const
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
    void reporter::operator()(comm::type t, const T& data) const
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
    void reporter::read(T& t) const
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
  }

} // namespace ciut

#define decltype typeof

#define CIUT_XML_TAG(name, ...) \
  if (ciut::xml::tag_t name = ciut::xml::tag_t(#name, __VA_ARGS__)) \
    {                                                               \
    }                                                               \
  else

#define CIUT_TEST_CASE_DEF(test_case_name, ...)                         \
  class test_case_name                                                  \
    : ciut::test_case_base, __VA_ARGS__                                 \
  {                                                                     \
    friend class ciut::implementation::test_wrapper<ciut_run_wrapper, test_case_name>; \
  friend class ciut::policies::dependencies::enforcer<test_case_name>;  \
  virtual void run_test()                                               \
  {                                                                     \
      ciut_timeout_enforcer obj;                                        \
      (void)obj; /* silence warning */                                  \
      using ciut::implementation::test_wrapper;                         \
      test_wrapper<ciut_run_wrapper, test_case_name>::run(this);        \
    }                                                                   \
    void test();                                                        \
    static ciut::test_case_base& creator()                              \
    {                                                                   \
      static test_case_name obj;                                        \
      return obj;                                                       \
    }                                                                   \
    class registrator                                                   \
      : public ciut::implementation::test_case_registrator,             \
        public virtual ciut::policies::dependencies::base,              \
        private virtual test_case_name::ciut_expected_death_cause,      \
        private virtual test_case_name::ciut_dependency                 \
          {                                                             \
            typedef ciut::implementation::test_case_registrator         \
              registrator_base;                                         \
          public:                                                       \
            registrator()                                               \
              : registrator_base(#test_case_name,                       \
                                 &test_case_name::creator)              \
              {                                                         \
              }                                                         \
          private:                                                      \
            virtual bool match_name(const char *name_param) const       \
            {                                                           \
              const char *p = current_namespace.match_name(name_param); \
              if (p)                                                    \
                {                                                       \
                  if (p != name_param || *p == ':')                     \
                    {                                                   \
                      if (!*p) return true; /* match for whole suites*/ \
                      if (!*p++ == ':') return false;                   \
                      if (!*p++ == ':') return false;                   \
                    }                                                   \
                }                                                       \
              else                                                      \
                {                                                       \
                  p = name_param;                                       \
                }                                                       \
              return !std::strcmp(p, #test_case_name);                  \
            }                                                           \
            virtual std::ostream &print_name(std::ostream &os) const    \
            {                                                           \
              os << current_namespace;                                  \
              return os << #test_case_name;                             \
            }                                                           \
          };                                                            \
    static registrator reg;                                             \
  };                                                                    \

#define TEST_DEF(test_case_name, ...)                                   \
  CIUT_TEST_CASE_DEF(test_case_name, __VA_ARGS__)                       \
  test_case_name :: registrator test_case_name::reg;                    \
  void test_case_name::test()

#define DISABLED_TEST_DEF(test_case_name, ...)                          \
  CIUT_TEST_CASE_DEF(test_case_name, __VA_ARGS__)                       \
  void test_case_name::test()

#define CIUT_CONCAT(a, b) a ## b

#define CIUT_CONCAT_(a, b) CIUT_CONCAT(a,b)

#define CIUT_LOCAL_NAME(prefix) \
  CIUT_CONCAT_(ciut_local_  ## prefix ## _, __LINE__)

namespace ciut {
  namespace implementation {
    template <typename T>
    const typename std::remove_cv<typename std::remove_reference<T>::type>::type&
    gettype();
  }
}

#define CIUT_REFTYPE(expr) \
  decltype(ciut::implementation::gettype<decltype(expr)>())




#define CIUT_BINARY_ASSERT(name, oper, lh, rh)                          \
  do {                                                                  \
  CIUT_REFTYPE(lh) CIUT_LOCAL_NAME(rl) = lh;                            \
  CIUT_REFTYPE(rh) CIUT_LOCAL_NAME(rr) = rh;                            \
  if (!(CIUT_LOCAL_NAME(rl) oper CIUT_LOCAL_NAME(rr)))                  \
    {                                                                   \
      std::ostringstream CIUT_LOCAL_NAME(os);                           \
      CIUT_XML_TAG(ASSERT_ ## name, CIUT_LOCAL_NAME(os))                \
        {                                                               \
          CIUT_XML_TAG(param, ASSERT_ ## name, ciut::xml::attr("name", #lh)) \
            {                                                           \
              param << CIUT_LOCAL_NAME(rl);                             \
            }                                                           \
          CIUT_XML_TAG(param, ASSERT_ ## name, ciut::xml::attr("name", #rh)) \
            {                                                           \
              param << CIUT_LOCAL_NAME(rr);                             \
            }                                                           \
        }                                                               \
      ciut::comm::report(ciut::comm::exit_fail, CIUT_LOCAL_NAME(os));   \
    }                                                                   \
  } while(0)

#define ASSERT_TRUE(a)                                                  \
  do {                                                                  \
    CIUT_REFTYPE(a) CIUT_LOCAL_NAME(ra) = a;                            \
    if (CIUT_LOCAL_NAME(ra))                                            \
      {                                                                 \
      }                                                                 \
    else                                                                \
      {                                                                 \
        std::ostringstream CIUT_LOCAL_NAME(os);                         \
        CIUT_XML_TAG(ASSERT_TRUE, CIUT_LOCAL_NAME(os))                  \
          {                                                             \
            CIUT_XML_TAG(expr, ASSERT_TRUE)                             \
              {                                                         \
                expr << CIUT_LOCAL_NAME(ra);                            \
              }                                                         \
          }                                                             \
        ciut::comm::report(ciut::comm::exit_fail, CIUT_LOCAL_NAME(os)); \
      }                                                                 \
  } while(0)



#define ASSERT_FALSE(a)                                                 \
  do {                                                                  \
    CIUT_REFTYPE(a) CIUT_LOCAL_NAME(ra) = a;                            \
    if (CIUT_LOCAL_NAME(ra))                                            \
      {                                                                 \
        std::ostringstream CIUT_LOCAL_NAME(os);                         \
        CIUT_XML_TAG(ASSERT_FALSE, CIUT_LOCAL_NAME(os))                 \
          {                                                             \
            CIUT_XML_TAG(expr, ASSERT_FALSE)                            \
              {                                                         \
                expr << CIUT_LOCAL_NAME(ra);                            \
              }                                                         \
          }                                                             \
        ciut::comm::report(ciut::comm::exit_fail, CIUT_LOCAL_NAME(os)); \
      }                                                                 \
  } while(0)

#define ASSERT_EQ(lh, rh)                       \
  CIUT_BINARY_ASSERT(EQ, ==, lh, rh)

#define ASSERT_NE(lh, rh)                       \
  CIUT_BINARY_ASSERT(NE, !=, lh, rh)

#define ASSERT_GE(lh, rh)                       \
  CIUT_BINARY_ASSERT(GE, >=, lh, rh)

#define ASSERT_GT(lh, rh)                       \
  CIUT_BINARY_ASSERT(GT, >, lh, rh)

#define ASSERT_LT(lh, rh)                         \
  CIUT_BINARY_ASSERT(LT, <, lh, rh)

#define ASSERT_LE(lh, rh)                       \
  CIUT_BINARY_ASSERT(LE, <=, lh, rh)


#define ASSERT_THROW(expr, exc)                                         \
  do {                                                                  \
    try {                                                               \
      expr;                                                             \
      std::ostringstream CIUT_LOCAL_NAME(os);                           \
      CIUT_XML_TAG(ASSERT_THROW, CIUT_LOCAL_NAME(os))                   \
      {                                                                 \
        CIUT_XML_TAG(expression, ASSERT_THROW)                          \
        {                                                               \
          expression << #expr;                                          \
        }                                                               \
        CIUT_XML_TAG(missing, ASSERT_THROW)                             \
        {                                                               \
          CIUT_XML_TAG(exception, missing)                              \
          {                                                             \
            exception << #exc;                                          \
          }                                                             \
        }                                                               \
      }                                                                 \
      ciut::comm::report(ciut::comm::exit_fail,                         \
                         CIUT_LOCAL_NAME(os));                          \
    }                                                                   \
    catch (exc) {                                                       \
    }                                                                   \
  } while (0)

#define ASSERT_NO_THROW(expr)                                           \
  do {                                                                  \
    try {                                                               \
      expr;                                                             \
    }                                                                   \
    catch (std::exception &CIUT_LOCAL_NAME(e)) {                        \
      std::ostringstream CIUT_LOCAL_NAME(os);                           \
      CIUT_XML_TAG(ASSERT_NO_THROW, CIUT_LOCAL_NAME(os))                \
        {                                                               \
          CIUT_XML_TAG(expression, ASSERT_NO_THROW)                     \
          {                                                             \
            expression << #expr;                                        \
          }                                                             \
          CIUT_XML_TAG(caught,  ASSERT_NO_THROW,                        \
                       ciut::xml::attr("type", "std::exception"),       \
                       ciut::xml::attr("what", CIUT_LOCAL_NAME(e).what()));  \
        }                                                               \
      ciut::comm::report(ciut::comm::exit_fail, CIUT_LOCAL_NAME(os));   \
    }                                                                   \
    catch (...) {                                                       \
      std::ostringstream CIUT_LOCAL_NAME(os);                           \
      CIUT_XML_TAG(ASSERT_NO_THROW, CIUT_LOCAL_NAME(os))                \
      {                                                                 \
        CIUT_XML_TAG(expression, ASSERT_NO_THROW)                       \
          {                                                             \
            expression << #expr;                                        \
          }                                                             \
        CIUT_XML_TAG(caught, ASSERT_NO_THROW,                           \
                     ciut::xml::attr("type", "..."));                   \
        }                                                               \
      ciut::comm::report(ciut::comm::exit_fail,                         \
                         CIUT_LOCAL_NAME(os));                          \
    }                                                                   \
  } while (0)

namespace ciut {
  class none {};
}

extern ciut::implementation::namespace_info current_namespace;

#define TEST(...) TEST_DEF(__VA_ARGS__, ciut::none)
#define DISABLED_TEST(...) DISABLED_TEST_DEF(__VA_ARGS__, ciut::none)

#define TESTSUITE(name)                                                 \
  namespace name {                                                      \
    namespace {                                                         \
      static ciut::implementation::namespace_info *parent_namespace     \
      = &current_namespace;                                             \
    }                                                                   \
    static ciut::implementation::namespace_info                         \
    current_namespace(#name, parent_namespace);                         \
  }                                                                     \
  namespace name

#endif // CIUT_HPP
