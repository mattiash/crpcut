/*
 * Copyright 2009-2010 Bjorn Fahller <bjorn@fahller.se>
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
#ifdef HAVE_VALGRIND
#include <valgrind/valgrind.h>
#include <valgrind/memcheck.h>
#else
#  define VALGRIND_CREATE_MEMPOOL(a,b,c)        do {} while (0)
#  define VALGRIND_MAKE_MEM_NOACCESS(a, b)      do {} while (0)
#  define VALGRIND_MAKE_MEM_UNDEFINED(a, b)     do {} while (0)
#  define VALGRIND_MALLOCLIKE_BLOCK(a, b, c, d) do {} while (0)
#  define VALGRIND_MAKE_MEM_DEFINED(a, b)       do {} while (0)
#  define VALGRIND_FREELIKE_BLOCK(a, b)         do {} while (0)
#  define VALGRIND_MEMPOOL_FREE(a, b)           do {} while (0)
#  define VALGRIND_MEMPOOL_ALLOC(a, b, c)       do {} while (0)
#endif

namespace {
  typedef enum { by_malloc, by_new_elem, by_new_array } alloc_type;
  static const char *alloc_name[] = {
    "malloc", "new", "new[]"
  };
  static const char *free_name[] = {
    "free", "delete", "delete[]"
  };

  inline void *zeromem(void *p, size_t n)
  {
    if (p)
      {
        char *d = static_cast<char *>(p);
        while (n--) *d++ = 0;
      }
    return p;
  }

  inline void *copymem(void *d, const void *s, size_t n)
  {
    char *dc = static_cast<char *>(d);
    const char *sc = static_cast<const char *>(s);
    while (n--) *dc++ = *sc++;
    return d;
  }
}
namespace crpcut
{

  namespace wrapped
  {
    CRPCUT_WRAP_FUNC(libc, malloc, void*, (size_t s), (s))
    CRPCUT_WRAP_V_FUNC(libc, free, void, (const void *p), (p))
    CRPCUT_WRAP_FUNC(libc, calloc, void *, (size_t n, size_t s), (n, s))
    CRPCUT_WRAP_FUNC(libc, realloc, void *, (void *m, size_t n), (m, n))

  }


  namespace heap
  {
    class new_handler_caller
    {
      typedef void (*bool_type)();
    public:
      new_handler_caller() throw () : handler(std::set_new_handler(0)) {}
      ~new_handler_caller() throw () { std::set_new_handler(handler); }
      void operator()() const { handler(); }
      operator bool_type () const throw () { return handler; }
    private:
      std::new_handler handler;
    };

    namespace {

      struct stats {
        size_t mem;
        alloc_type type;
      };

      const size_t num_elems = 50000;
      stats vector[num_elems];
      size_t current_offset = 0;

      size_t limit = heap::system;
      size_t bytes;
      size_t objects;
    }
    bool control::enabled;

    static void alloc_type_check(stats *p, alloc_type type) throw ()
    {
      if (p->type != type)
        {
          void *addr = p + 1;
          if (control::is_enabled())
            {
              comm::direct_reporter<crpcut::comm::exit_fail>()
                << "DEALLOC FAIL\n"
                << free_name[type] << " " << addr << " was allocated using "
                << alloc_name[p->type];
            }
          else
            {
              static const char msg[]="alloc/dealloc type mismatch\n";
              wrapped::write(2, msg, sizeof(msg) - 1);
              wrapped::abort();
            }
        }
    }

    static void *alloc_mem(size_t s, alloc_type type) throw ()
    {
      const size_t current_limit = limit;
      if (bytes + s > current_limit)
        {
          return 0;
        }

      const size_t blocks = (s + sizeof(stats) - 1)/sizeof(stats) + 1;

      if (current_offset + blocks < num_elems)
        {
          if (current_offset == 0)
            {
              VALGRIND_CREATE_MEMPOOL(vector, sizeof(stats), 0);
              VALGRIND_MAKE_MEM_NOACCESS(vector, sizeof(vector));
            }
          stats *p = &vector[current_offset];
          current_offset += blocks + 1;
          bytes += s;
          VALGRIND_MAKE_MEM_UNDEFINED(p, sizeof(stats));
          p->mem = s;
          p->type = type;
          VALGRIND_MAKE_MEM_NOACCESS(p, sizeof(stats));
          ++p;
          VALGRIND_MEMPOOL_ALLOC(vector, p, s);
          VALGRIND_MALLOCLIKE_BLOCK(p, s, sizeof(stats), 0);
          ++objects;
          return p;
        }
      const size_t size = s + 2*sizeof(stats);
      void *addr = crpcut::wrapped::malloc(size);
      stats *p = static_cast<stats*>(addr);
      if (p)
        {
          p->mem = s;
          p->type = type;
          VALGRIND_MAKE_MEM_NOACCESS(p, size);
          ++p;
          VALGRIND_MALLOCLIKE_BLOCK(p, s, sizeof(stats), 0);
          bytes+= s;
          ++objects;
        }
      return p;
    }


    static void free_mem(void *addr, alloc_type expected) throw ()
    {
      if (!addr) return;

      using namespace crpcut::heap;

      stats *p = static_cast<stats*>(addr);
      VALGRIND_MAKE_MEM_DEFINED(p-1, sizeof(stats));
      alloc_type_check(p-1, expected);
      bytes-= p[-1].mem;
      VALGRIND_FREELIKE_BLOCK(p, sizeof(stats));
      --objects;
      if (p >= vector && p < &vector[num_elems])
        {
          VALGRIND_MEMPOOL_FREE(vector, addr);
          return;
        }

      crpcut::wrapped::free(p - 1);
    }


    void *alloc_new_mem(size_t s, alloc_type type) throw (std::bad_alloc)
    {
      static std::bad_alloc exc;
      for (;;)
        {
          new_handler_caller handler;
          void *p = crpcut::heap::alloc_mem(s, type);
          if (p) return p;
          if (!handler) throw exc;
          handler();
        }
    }

    size_t set_limit(size_t n)
    {
      if (n < bytes)
        {
          if (control::is_enabled())
            {
              size_t now_bytes = bytes;
              comm::direct_reporter<crpcut::comm::exit_fail>()
                << "heap::set_limit(" << n
                << ") is below current use of " << now_bytes
                << " bytes";
            }
          else
            {
              static const char msg[]
                = "heap::set_limit() below current use\n";
              wrapped::write(2, msg, sizeof(msg) - 1);
              wrapped::abort();
            }
        }
      size_t rv = limit;
      limit = n;
      return rv;
    }
    size_t allocated_bytes()
    {
      return bytes;
    }
    size_t allocated_objects()
    {
      return objects;
    }
  }
}

extern "C"
{

  void *malloc(size_t s) throw ()
  {
    return crpcut::heap::alloc_mem(s, by_malloc);
  }

  void free(void *addr) throw ()
  {
    crpcut::heap::free_mem(addr, by_malloc);
  }

  void *calloc(size_t n, size_t s) throw ()
  {
    return zeromem(malloc(n*s), n*s);
  }

  void *realloc(void *addr, size_t s) throw ()
  {
    if (addr == 0) return malloc(s);
    if (s == 0)
      {
        free(addr);
        return 0;
      }
    crpcut::heap::stats *p = static_cast<crpcut::heap::stats*>(addr);
    VALGRIND_MAKE_MEM_DEFINED(p - 1, sizeof(crpcut::heap::stats));
    crpcut::heap::alloc_type_check(p-1, by_malloc);
    const size_t block_size = p[-1].mem;
    VALGRIND_MAKE_MEM_NOACCESS(p - 1, sizeof(crpcut::heap::stats));
    if (s <= block_size) return addr;

    void *new_addr = malloc(s);
    if (new_addr)
      {
        copymem(new_addr, p, block_size);
        free(addr);
      }
    return new_addr;
  }

}


void *operator new(size_t s) throw (std::bad_alloc)
{
  return crpcut::heap::alloc_new_mem(s, by_new_elem);
}

void *operator new(size_t s, const std::nothrow_t&) throw ()
{
  try {
    return crpcut::heap::alloc_new_mem(s, by_new_elem);
  }
  catch (std::bad_alloc&)  {
  }
  return 0;
}

void *operator new[](size_t s) throw (std::bad_alloc)
{
  return crpcut::heap::alloc_new_mem(s, by_new_array);
}

void *operator new[](size_t s, const std::nothrow_t&) throw ()
{
  try {
    return crpcut::heap::alloc_new_mem(s, by_new_array);
  }
  catch (std::bad_alloc&)  {
  }
  return 0;
}

void operator delete(void *p) throw ()
{
  crpcut::heap::free_mem(p, by_new_elem);
}

void operator delete(void *p, const std::nothrow_t&) throw ()
{
  crpcut::heap::free_mem(p, by_new_elem);
}

void operator delete[](void *p) throw ()
{
  crpcut::heap::free_mem(p, by_new_array);
}

void operator delete[](void *p, const std::nothrow_t&) throw ()
{
  crpcut::heap::free_mem(p, by_new_array);
}

