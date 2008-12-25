#ifndef CIUT_HPP
#define CIUT_HPP

#include <stdexcept>
#include <sstream>
#include <iostream>
#include <ostream>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <tr1/type_traits>
namespace std {
  using namespace std::tr1;
}


#define NO_CORE_FILE protected virtual ciut::policies::no_core_file
#define EXPECT_EXIT(num) protected virtual ciut::policies::exit_death<num>
#define EXPECT_SIGNAL_DEATH(num) protected virtual ciut::policies::signal_death<num>
#define EXPECT_EXCEPTION(type) protected virtual ciut::policies::exception_specifier<type>
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

    }
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
  }

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

  namespace implementation {
    struct namespace_info
    {
    public:
      namespace_info(const char *n, namespace_info *p) : name(n), parent(p) {}
      const char* match_name(const char *n) const;
      // returns 0 on mismatch, otherwise a pointer into n where namespace name
      // ended.              if (!*p++ == ':') return false;

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
      virtual bool match_name(const char *name) const = 0;
      test_case_base *instantiate_obj() const { return &func_(); }
    protected:
      const char *name_;
    private:
      virtual std::ostream &print_name(std::ostream &) const = 0;
      test_case_registrator() : next(this), prev(this) {}
      test_case_registrator *next;
      test_case_registrator *prev;
      test_case_creator func_;
      friend class ciut::test_case_factory;
    };
  }
  class test_case_factory
  {
  public:
    static void run_test(const char *name = 0) { obj().do_run(name); }
    static test_case_factory& obj() { static test_case_factory f; return f; }
  private:
    void do_run(const char *name);
    friend class implementation::test_case_registrator;
    class registrator_list : public implementation::test_case_registrator
    {
      virtual bool match_name(const char *) const { return false; }
      virtual std::ostream& print_name(std::ostream &os) const { return os; }
    };
    registrator_list reg;
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



  }
}

namespace comm {
  typedef enum { exit_ok, exit_fail, info, violation } type;

  // protocol is type -> size_t(length) -> char[length]. length may be 0.
  // reader acknowledges with length.

  template <int N>
  class chararray
  {
    typedef const char type[N];
    typedef type &ref;
  };

  class pipe {
  private:
    typedef int pair[2];
    pair pipes[2];
  public:
    typedef enum { read_op, write_op } end;
    typedef enum { c2p, p2c } direction;
    template <end e, direction dir>
    int populate(fd_set &set, int nfds)
    {
      FD_SET(pipes[dir][e], &set);
      return std::max(pipes[dir][e] + 1, nfds);
    }
    template <end e, direction dir>
    bool ok_to(fd_set &set)
    {
      return FD_ISSET(pipes[dir][e], &set);
    }
    pipe() {
      if (::pipe(pipes[c2p])) throw "dammit";
      if (::pipe(pipes[p2c])) throw "dammit again";
    }
    ~pipe() {
      ::close(pipes[c2p][read_op]);
      ::close(pipes[c2p][write_op]);
      ::close(pipes[p2c][read_op]);
      ::close(pipes[p2c][write_op]);
    }
    const pipe& report(type t, const std::ostringstream &os)
    {
      write<c2p>(t);
      const std::string &s = os.str();
      const size_t len = s.length();
      write<c2p>(len);
      const char *p = s.c_str();
      size_t bytes_written = 0;
      while (bytes_written < len)
        {
          int rv = ::write(pipes[c2p][write_op],
                           p + bytes_written,
                           len - bytes_written);
          if (rv == -1 && errno == EINTR) continue;
          if (rv <= 0) throw "report failed";
          bytes_written += rv;
        }
      read<p2c>(bytes_written);
      assert(len == bytes_written);
      return *this;
    }
    const pipe& report(type t, const char *msg) const
    {
      const std::size_t len = std::strlen(msg);
      write<c2p>(t);
      write<c2p>(len);
      const char *p = msg;
      size_t bytes_written = 0;
      while (bytes_written < len)
        {
          int rv = ::write(pipes[c2p][write_op],
                           p + bytes_written,
                           len - bytes_written);
          if (rv == -1 && errno == EINTR) continue;
          if (rv <= 0) throw "report failed";
          bytes_written += rv;
        }
      read<p2c>(bytes_written);
      assert(bytes_written == len);
      return *this;
    }
    const pipe& report(type t) const
    {
      write<c2p>(t);
      write<c2p>(size_t());
      size_t len;
      read<p2c>(len);
      assert(len == 0);
      return *this;
    }
    template <direction dir>
    void read(type &t) const
    {
      char *p = static_cast<char*>(static_cast<void*>(&t));
      size_t bytes_read = 0;
      while (bytes_read < sizeof(t))
        {
          int rv = ::read(pipes[dir][read_op],
                          p + bytes_read,
                          sizeof(t) - bytes_read);
          if (rv == -1 && errno == EINTR) continue;
          if (rv <= 0) throw "read info type failed";
          bytes_read += rv;
        }
    }
    template <direction dir>
    void read(size_t &t) const
    {
      char *p = static_cast<char*>(static_cast<void*>(&t));
      size_t bytes_read = 0;
      while (bytes_read < sizeof(t))
        {
          int rv = ::read(pipes[dir][read_op],
                          p + bytes_read,
                          sizeof(t) - bytes_read);
          if (rv == -1 && errno == EINTR) continue;
          if (rv <= 0) throw "read info type failed";
          bytes_read += rv;
        }
    }
    template <direction dir>
    ssize_t read(char *p, size_t len) const
    {
      return ::read(pipes[dir][read_op], p, len);
    }
    template <direction dir, typename T>
    void write(const T& t) const
    {
      const size_t len = sizeof(T);
      size_t bytes_written = 0;
      const char *p = static_cast<const char*>(static_cast<const void*>(&t));
      while (bytes_written < len)
        {
          int rv = ::write(pipes[dir][write_op],
                           p + bytes_written,
                           len - bytes_written);
          if (rv == -1 && errno == EINTR) continue;
          if (rv <= 0) throw "write failed";
          bytes_written += rv;
        }
    }
  private:
    template <direction dir, typename T>
    void read(T& t) const
    {
      const size_t len = sizeof(T);
      size_t bytes_read = 0;
      char *p = static_cast<char*>(static_cast<void*>(&t));
      while (bytes_read < len)
        {
          int rv = ::read(pipes[dir][read_op],
                          p + bytes_read,
                          len - bytes_read);
          if (rv == -1 && errno == EINTR) continue;
          if (rv <= 0) throw "read failed";
          bytes_read += rv;
        }
    }
  };
}
extern comm::pipe testcase_pipe;

namespace ciut {
  namespace implementation {
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
        testcase_pipe.report(comm::exit_fail, "threw wrong exception");
        std::_Exit(1);
      }
      testcase_pipe.report(comm::exit_fail, "did not throw exception");
      std::_Exit(1);
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
      testcase_pipe.report(comm::exit_fail, "Unexpectedly survived");
      _Exit(0);
    }
  }
}

#define decltype typeof

#define TEST_DEF(test_case_name, ...)                                   \
  class test_case_name                                                  \
    : ciut::test_case_base, __VA_ARGS__                                 \
  {                                                                     \
    friend class ciut::implementation::test_wrapper<run_wrapper, test_case_name>; \
    virtual void run_test()                                             \
    {                                                                   \
      ciut::implementation::test_wrapper<run_wrapper, test_case_name>::run(this); \
    }                                                                  \
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
            typedef ciut::implementation::test_case_registrator registrator_base; \
          public:                                                       \
            registrator()                                               \
              : registrator_base(#test_case_name, &test_case_name::creator) \
              {                                                         \
              }                                                         \
          private:                                                      \
            virtual bool match_name(const char *name_param) const       \
            {                                                           \
              const char *p = current_namespace.match_name(name_param); \
              if (!p) return false;                                     \
              if (!*p) return true; /* match for whole suites*/         \
              if (!*p++ == ':') return false;                           \
              if (!*p++ == ':') return false;                           \
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
  test_case_name :: registrator test_case_name::reg;                    \
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
        testcase_pipe.report(comm::exit_fail, os);                      \
        std::_Exit(1);                                                  \
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
        testcase_pipe.report(comm::exit_fail, os);                      \
        std::_Exit(1);                                                  \
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
        testcase_pipe.report(comm::exit_fail, os);                      \
        std::_Exit(1);                                                  \
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
        testcase_pipe.report(comm::exit_fail, os);                      \
        std::_Exit(1);                                                  \
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
      testcase_pipe.report(comm::exit_fail, os);                        \
      std::_Exit(1);                                                    \
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
      testcase_pipe.report(comm::exit_fail, os);                        \
      std::_Exit(1);                                                    \
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
      testcase_pipe.report(comm::exit_fail, os);                        \
      std::_Exit(1);                                                    \
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
      testcase_pipe.report(comm::exit_fail, os);                        \
      std::_Exit(1);                                                    \
    }                                                                   \
  } while(0)

#define ASSERT_THROW(expr, exc)                                         \
  do {                                                                  \
    try {                                                               \
      expr;                                                             \
      testcase_pipe.report(comm::exit_fail,                             \
                           "ASSERT_THROW(" #expr ", " #exc ") did not throw"); \
      std::_Exit(1);                                                    \
        }                                                               \
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
      testcase_pipe.report(comm::exit_fail, out);                       \
      std::_Exit(1);                                                    \
    }                                                                   \
    catch (...) {                                                       \
      testcase_pipe.report(comm::exit_fail,                             \
                           "ASSERT_NO_THROW(" #expr ") threw something");  \
      std::_Exit(1);                                                    \
    }                                                                   \
  } while (0)


class none {};



extern ciut::implementation::namespace_info current_namespace;

#define TEST(...) TEST_DEF(__VA_ARGS__, none)

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
