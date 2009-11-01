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
#ifdef HAVE_VALGRIND
#include <valgrind/valgrind.h>
#define ALLOCATED_MEM(p, s) VALGRIND_MALLOCLIKE_BLOCK(p, s, 0UL, 0)
#define FREED_MEM(p) VALGRIND_FREELIKE_BLOCK(p, 0UL)
#else
#define ALLOCATED_MEM(p, s) do { } while (0)
#define FREED_MEM(p) do { } while (0)
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
    char *d = static_cast<char *>(p);
    while (n--) *d++ = 0;
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

    static void alloc_type_check(stats *p, alloc_type type) throw ()
    {
      if (p->type != type)
        {
          void *addr = p + 1;
          comm::direct_reporter<crpcut::comm::exit_fail>()
            << "DEALLOC FAIL\n"
            << free_name[type] << " " << addr << " was allocated using "
            << alloc_name[p->type];
        }
    }

    static void *alloc_mem(size_t s, alloc_type type) throw ()
    {
      if (!s) return 0;

      const size_t current_limit = limit;
      if (bytes + s > current_limit)
        {
          return 0;
        }

      const size_t blocks = (s + sizeof(stats) - 1)/sizeof(stats) + 1;

      if (current_offset + blocks < num_elems)
        {
          stats *p = &vector[current_offset];
          current_offset += blocks;
          bytes += s;
          p->mem = s;
          p->type = type;
          ++p;
          ALLOCATED_MEM((void*)p, s);
          ++objects;
          return p;
        }
      void *addr = crpcut::wrapped::malloc(s + sizeof(stats));
      stats *p = static_cast<stats*>(addr);
      if (p) { p->mem = s; p->type = type; ++p; bytes+= s; ++objects; }
      return p;
    }


    static void free_mem(void *addr, alloc_type expected) throw ()
    {
      if (!addr) return;

      using namespace crpcut::heap;

      stats *p = static_cast<stats*>(addr);
      alloc_type_check(p-1, expected);
      bytes-= p[-1].mem;
      --objects;
      if (addr >= vector && addr < &vector[num_elems])
        {
          FREED_MEM(addr);
          return;
        }

      crpcut::wrapped::free(p - 1);
    }

    size_t set_limit(size_t n)
    {
      if (n < bytes)
        {
          size_t now_bytes = bytes;
          comm::direct_reporter<crpcut::comm::exit_fail>()
            << "heap::set_limit(" << n
            << ") is below current use of " << now_bytes
            << " bytes";
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
    crpcut::heap::alloc_type_check(p-1, by_malloc);
    if (s <= p[-1].mem) return addr;

    void *new_addr = malloc(s);
    if (new_addr)
      {
        copymem(new_addr, p, p[-1].mem);
        free(addr);
      }
    return new_addr;
  }
}


void *operator new(size_t s) throw (std::bad_alloc)
{
  void *p = crpcut::heap::alloc_mem(s, by_new_elem);
  if (!p)
    {
      static std::bad_alloc exc;
      throw exc;
    }
  return p;
}

void *operator new(size_t s, const std::nothrow_t&) throw ()
{
  return crpcut::heap::alloc_mem(s, by_new_elem);
}

void *operator new[](size_t s) throw (std::bad_alloc)
{
  void *p = crpcut::heap::alloc_mem(s, by_new_array);
  if (!p)
    {
      static std::bad_alloc exc;
      throw exc;
    }
  return p;
}

void *operator new[](size_t s, const std::nothrow_t&) throw ()
{
  return crpcut::heap::alloc_mem(s, by_new_array);
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

