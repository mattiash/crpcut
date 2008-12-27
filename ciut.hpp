#ifndef CIUT_HPP
#define CIUT_HPP

#include <stdexcept>
#include <sstream>
#include <iostream>
#include <ostream>
#include <cerrno>
#include <cassert>
#include <tr1/type_traits>

namespace std {
  using namespace std::tr1;
}

struct siginfo;

#define NO_CORE_FILE \
  protected virtual ciut::policies::no_core_file

#define EXPECT_EXIT(num) \
  protected virtual ciut::policies::exit_death<num>

#define EXPECT_SIGNAL_DEATH(num) \
  protected virtual ciut::policies::signal_death<num>

#define EXPECT_EXCEPTION(type) \
  protected virtual ciut::policies::exception_specifier<type>

#define ANY_CODE -1

namespace ciut {

  namespace policies {

    namespace deaths {
      class none;
    }

    class default_policy
    {
    protected:
      typedef void run_wrapper;
      typedef deaths::none expected_death_cause;
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
      typedef deaths::wrapper run_wrapper;
      typedef deaths::signal<N> expected_death_cause;
    };

    template <int N>
    class exit_death : protected virtual default_policy
    {
    public:
      typedef deaths::wrapper  run_wrapper;
      typedef deaths::exit<N>  expected_death_cause;
    };

    template <typename exc>
    class exception_wrapper;

    template <typename T>
    class exception_specifier : protected virtual default_policy
    {
    public:
      typedef exception_wrapper<T> run_wrapper;
    };

    class no_core_file : protected virtual default_policy
    {
    protected:
      no_core_file();
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

    typedef enum { exit_ok, exit_fail, info, violation } type;

    // protocol is type -> size_t(length) -> char[length]. length may be 0.
    // reader acknowledges with length.

    class reporter
    {
      int write_fd;
      int read_fd;
    public:
      reporter() : write_fd(0), read_fd(0) {}
      void set_fds(int read, int write) { write_fd = write, read_fd = read; }
      void operator()(type t, std::ostringstream &os) const;
      void operator()(type t, const char *msg = "") const
      {
        std::ostringstream os;
        os << msg;
        operator()(t, os);
      }
    private:
      template <typename T>
      void write(const T& t) const;

      template <typename T>
      void read(T& t) const;
    };

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

    extern reporter report;

  } // namespace comm

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

    class test_case_registrator : public virtual policies::deaths::none
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
      bool failed() const { return !successful; }
      virtual bool match_name(const char *name) const = 0;
      test_case_base *instantiate_obj() const { return &func_(); }
      void setup(pid_t pid, int in_fd, int out_fd);
      void manage_death();
      void unlink() {
        next->prev = prev;
        prev->next = next;
      }
      bool read_report(); // true if read succeeded
    protected:
      const char *name_;
    private:
      void unregister_fds();
      virtual std::ostream &print_name(std::ostream &) const = 0;
      test_case_registrator() : next(this), prev(this) {}
      test_case_registrator *next;
      test_case_registrator *prev;
      test_case_creator func_;
      bool death_note;
      int in_fd_;
      int out_fd_;
      pid_t pid_;
      bool successful;
      friend class ciut::test_case_factory;
    };

  } // namespace implementation

  class test_case_factory
  {
  public:
    static const unsigned max_parallel = 8;

    static unsigned run_test(int argc, const char *argv[])
    {
      return obj().do_run(argc, argv);
    }
    static void introduce_name(pid_t, const std::string &s);
    static void present(pid_t pid, comm::type t, size_t len, const char *buff);
    static bool tests_as_child_procs()
    {
      return obj().num_parallel > 0;
    }
  private:
    static test_case_factory& obj() { static test_case_factory f; return f; }
    test_case_factory()
      : pending_children(0),
        verbose_mode(false),
        num_parallel(1),
        num_tests(0),
        num_tests_run(0),
        num_failed_tests(0)
    {
    }
    void start_presenter_process();
    void kill_presenter_process();
    void manage_children(unsigned max_pending_children);
    void run_test_case(implementation::test_case_registrator *i) const;
    void start_test(implementation::test_case_registrator *i);

    unsigned do_run(int argc, const char *argv[]);

    friend class implementation::test_case_registrator;

    class registrator_list : public implementation::test_case_registrator
    {
      virtual bool match_name(const char *) const { return false; }
      virtual std::ostream& print_name(std::ostream &os) const { return os; }
    };

    registrator_list reg;
    unsigned         pending_children;
    bool             verbose_mode;
    unsigned         num_parallel;
    bool             single_process;
    unsigned         num_tests;
    unsigned         num_tests_run;
    unsigned         num_failed_tests;
    pid_t            presenter_pid;
    int              presenter_pipe;
  };

  namespace implementation {

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
        comm::report(comm::exit_fail, "threw wrong exception");
      }
      comm::report(comm::exit_fail, "did not throw exception");
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
      comm::report(comm::exit_fail, "Unexpectedly survived");
    }

  } // namespace implementation



} // namespace ciut

#define decltype typeof

#define TEST_CASE_DEF(test_case_name, ...)                              \
  class test_case_name                                                  \
    : ciut::test_case_base, __VA_ARGS__                                 \
  {                                                                     \
    friend class ciut::implementation::test_wrapper<run_wrapper, test_case_name>; \
    virtual void run_test()                                             \
    {                                                                   \
      ciut::implementation::test_wrapper<run_wrapper, test_case_name>::run(this); \
    }                                                                   \
    void test();                                                        \
    static ciut::test_case_base& creator()                              \
    {                                                                   \
      static test_case_name obj;                                        \
      return obj;                                                       \
    }                                                                   \
    class registrator                                                   \
      : public ciut::implementation::test_case_registrator,             \
        public virtual test_case_name::expected_death_cause             \
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
  TEST_CASE_DEF(test_case_name, __VA_ARGS__)                            \
  test_case_name :: registrator test_case_name::reg;                    \
  void test_case_name::test()

#define DISABLED_TEST_DEF(test_case_name, ...)                          \
  TEST_CASE_DEF(test_case_name, __VA_ARGS__)                            \
  void test_case_name::test()

#define ASSERT_TRUE(a)                                                  \
  do {                                                                  \
    const std::remove_cv<std::remove_reference<decltype(a)>::type>::type &ar = a; \
    if (ar)                                                             \
      {                                                                 \
      }                                                                 \
    else                                                                \
      {                                                                 \
        std::ostringstream os;                                          \
        os << "ASSERT_TRUE(a)\n  where a="                              \
           << ar;                                                       \
        ciut::comm::report(ciut::comm::exit_fail, os);                  \
      }                                                                 \
  } while(0)

#define ASSERT_FALSE(a)                                                 \
  do {                                                                  \
    const std::remove_cv<std::remove_reference<decltype(a)>::type>::type &ar = a; \
    if (ar)                                                             \
      {                                                                 \
        std::ostringstream os;                                          \
        os << "ASSERT_TRUE(a)\n  where a="                              \
           << ar;                                                       \
        ciut::comm::report(ciut::comm::exit_fail, os);                  \
      }                                                                 \
  } while(0)

#define ASSERT_EQ(a, b)                                                 \
  do {                                                                  \
    const std::remove_cv<std::remove_reference<decltype(a)>::type>::type &ar = a; \
    const std::remove_cv<std::remove_reference<decltype(a)>::type>::type &br = b; \
    if (!(ar == br))                                                    \
      {                                                                 \
        std::ostringstream os;                                          \
        os << "ASSERT_EQ(" #a ", " #b ")\n  where " #a "="              \
           << ar                                                        \
           << "\n        " #b "="                                       \
           << br;                                                       \
        ciut::comm::report(ciut::comm::exit_fail, os);                  \
      }                                                                 \
  } while(0)

#define ASSERT_NE(a, b)                                                 \
  do {                                                                  \
    const std::remove_cv<std::remove_reference<decltype(a)>::type>::type &ar = a; \
    const std::remove_cv<std::remove_reference<decltype(a)>::type>::type &br = b; \
    if (!(ar != br))                                                    \
      {                                                                 \
        std::ostringstream os;                                          \
        os << "ASSERT_NE(a,b)\n  where a="                              \
           << ar                                                        \
           << "\n        b="                                            \
           << br;                                                       \
        ciut::comm::report(ciut::comm::exit_fail, os);                  \
      }                                                                 \
  } while(0)

#define ASSERT_GE(a, b)                                                 \
  do {                                                                  \
    const std::remove_cv<std::remove_reference<decltype(a)>::type>::type &ar = a; \
    const std::remove_cv<std::remove_reference<decltype(a)>::type>::type &br = b; \
    if (!(ar >= br))                                                    \
    {                                                                   \
      std::ostringstream os;                                            \
      os << "ASSERT_GE(a,b)\n  where a="                                \
         << ar                                                          \
         << "\n        b="                                              \
         << br;                                                          \
      ciut::comm::report(ciut::comm::exit_fail, os);                    \
    }                                                                   \
  } while(0)

#define ASSERT_GT(a, b)                         \
  do {                                                                  \
    const std::remove_cv<std::remove_reference<decltype(a)>::type>::type &ar = a; \
    const std::remove_cv<std::remove_reference<decltype(a)>::type>::type &br = b; \
    if (!(ar > br))                                                     \
    {                                                                   \
      std::ostringstream os;                                            \
      os << "ASSERT_GT(a,b)\n  where a="                                \
         << ar                                                           \
         << "\n        b="                                              \
         << br;                                                          \
      ciut::comm::report(ciut::comm::exit_fail, os);                    \
    }                                                                   \
  } while(0)

#define ASSERT_LT(a, b)                         \
  do {                                                                  \
    const std::remove_cv<std::remove_reference<decltype(a)>::type>::type &ar = a; \
    const std::remove_cv<std::remove_reference<decltype(a)>::type>::type &br = b; \
    if (!(ar < br))                                                     \
    {                                                                   \
      std::ostringstream os;                                            \
      os << "ASSERT_LT(a,b)\n  where a="                                \
         << ar                                                           \
         << "\n        b="                                              \
         << br;                                                          \
      ciut::comm::report(ciut::comm::exit_fail, os);                    \
    }                                                                   \
  } while(0)

#define ASSERT_LE(a, b)                                                 \
  do {                                                                  \
    const std::remove_cv<std::remove_reference<decltype(a)>::type>::type &ar = a; \
    const std::remove_cv<std::remove_reference<decltype(a)>::type>::type &br = b; \
    if (!(ar <= br))                                                     \
    {                                                                   \
      std::ostringstream os;                                            \
      os << "ASSERT_LE(a,b)\n  where a="                                \
         << ar                                                           \
         << "\n        b="                                               \
         << br;                                                          \
      ciut::comm::report(ciut::comm::exit_fail, os);                    \
    }                                                                   \
  } while(0)

#define ASSERT_THROW(expr, exc)                                         \
  do {                                                                  \
    try {                                                               \
      expr;                                                             \
      ciut::comm::report(ciut::comm::exit_fail,                         \
                         "ASSERT_THROW(" #expr ", " #exc ") did not throw"); \
    }                                                                   \
    catch (exc) {                                                       \
    }                                                                   \
  } while (0)

#define ASSERT_NO_THROW(expr)                                           \
  do {                                                                  \
    try {                                                               \
      expr;                                                             \
    }                                                                   \
    catch (std::exception &e) {                                         \
      std::ostringstream out;                                           \
      out << "ASSERT_NO_THROW(" #expr ") threw std exception\n"         \
        "  what() is \"" << e.what() << "\"";                           \
      ciut::comm::report(ciut::comm::exit_fail, out);                   \
    }                                                                   \
    catch (...) {                                                       \
      ciut::comm::report(ciut::comm::exit_fail,                         \
                         "ASSERT_NO_THROW(" #expr ") threw something"); \
    }                                                                   \
  } while (0)


class none {};



extern ciut::implementation::namespace_info current_namespace;

#define TEST(...) TEST_DEF(__VA_ARGS__, none)
#define DISABLED_TEST(...) DISABLED_TEST_DEF(__VA_ARGS__, none)

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
