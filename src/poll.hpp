/*
 * Copyright 2009-2011 Bjorn Fahller <bjorn@fahller.se>
 * All rights reserved
 *
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


#ifndef POLL_HPP
#define POLL_HPP

extern "C" void perror(const char*);
#include <cassert>
#include <algorithm>
namespace crpcut {
  namespace wrapped {
    int close(int fd);
  }
}

extern "C"
{
#include <sys/select.h>
}

namespace crpcut {
  namespace wrapped {
    int select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
  }
  template <size_t N>
  struct polldata

  {
    polldata()
      : num_subscribers(0U),
        pending_fds(0U)
    {
      FD_ZERO(&rset);
      FD_ZERO(&wset);
      FD_ZERO(&xset);
    }
    struct fdinfo
    {
      struct has_fd
      {
        has_fd(int num) : fd_(num) {}
        bool operator()(const fdinfo& i) const { return fd_ == i.fd; }
      private:
        int fd_;
      };
      fdinfo(int fd_ = 0, int mode_ = 0, void *ptr_ = 0)
        : fd(fd_), mode(mode_), ptr(ptr_)
      {
      }
      int   fd;
      int   mode;
      void *ptr;
    };
    fdinfo access[N];
    size_t num_subscribers;
    size_t pending_fds;
    fd_set rset;
    fd_set wset;
    fd_set xset;


    static const int readbit  = 1;
    static const int writebit = 2;
    static const int hupbit   = 4;
  };
}


namespace crpcut {
  template <typename T, size_t N>
  class poll : private polldata<N>
  {
  public:
    struct polltype
    {
      typedef enum { r = 1, w = 2, rw = 3 } type;
    };
    class descriptor
    {
    public:
      T* operator->() const { return data; }
      T* get() const { return data; }
      bool read() const;
      bool write() const;
      bool hup() const;
      bool timeout() const { return mode == 0; }
    private:
      descriptor(T* t, int m) : data(t), mode(m) {}

      T* data;
      int mode;
      friend class poll<T, N>;
    };
    poll();
    ~poll();
    void add_fd(int fd, T* data, int flags = polltype::r);
    void del_fd(int fd);
    descriptor wait(int timeout_ms = -1);
    size_t num_fds() const;
  };
}

namespace crpcut {
  template <typename T, size_t N>
  inline bool poll<T, N>::descriptor::read() const
  {
    return mode & polldata<N>::readbit;
  }

  template <typename T, size_t N>
  inline bool poll<T, N>::descriptor::write() const
  {
    return mode & polldata<N>::writebit;
  }

  template <typename T, size_t N>
  inline bool poll<T, N>::descriptor::hup() const
  {
    return mode & polldata<N>::hupbit;
  }

  template <typename T, size_t N>
  inline poll<T, N>::poll()
  {
  }

  template <typename T, size_t N>
  inline poll<T, N>::~poll()
  {
  }

  template <typename T, size_t N>
  inline void poll<T, N>::add_fd(int fd, T* data, int flags)
  {
    this->access[this->num_subscribers++] = typename polldata<N>::fdinfo(fd, flags, data);
  }

  template <typename T, size_t N>
  inline void poll<T, N>::del_fd(int fd)
  {
    typedef typename polldata<N>::fdinfo info;
    info *i = std::find_if(this->access,
                           this->access + this->num_subscribers,
                           typename info::has_fd(fd));
    assert(i != this->access + this->num_subscribers && "fd not found");
    *i = this->access[--this->num_subscribers];
    if (   FD_ISSET(fd, &this->xset)
           || FD_ISSET(fd, &this->rset)
           || FD_ISSET(fd, &this->wset))
      {
        FD_CLR(fd, &this->rset);
        FD_CLR(fd, &this->wset);
        FD_CLR(fd, &this->xset);
        --this->pending_fds;
      }
  }

  template <typename T, size_t N>
  inline typename poll<T, N>::descriptor poll<T, N>::wait(int timeout_ms)
  {
    if (this->pending_fds == 0)
      {
        int maxfd = 0;
        for (size_t i = 0; i < this->num_subscribers; ++i)
          {
            int fd = this->access[i].fd;
            if (fd > maxfd) maxfd = fd;
            if (this->access[i].mode & polltype::r) FD_SET(fd, &this->rset);
            if (this->access[i].mode & polltype::w) FD_SET(fd, &this->wset);
            FD_SET(fd, &this->xset);
          }
        struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
        for (;;)
          {
            int rv = wrapped::select(maxfd + 1,
                                     &this->rset,
                                     &this->wset,
                                     &this->xset,
                                     timeout_ms == -1 ? 0 : &tv);
            if (rv == -1 && errno == EINTR) continue;
            assert(rv >= 0);
            if (rv == 0) return descriptor(0,0); // timeout
            this->pending_fds = size_t(rv);
            break;
          }
      }
    for (size_t j = 0; j < this->num_subscribers; ++j)
      {
        int fd = this->access[j].fd;
        int mode = 0;
        if (FD_ISSET(fd, &this->rset))
          {
            mode|= polldata<N>::readbit;
            FD_CLR(fd, &this->rset);
          }
        if (FD_ISSET(fd, &this->wset))
          {
            mode|= polldata<N>::writebit;
            FD_CLR(fd, &this->wset);
          }
        if (FD_ISSET(fd, &this->xset))
          {
            mode|= polldata<N>::hupbit;
            FD_CLR(fd, &this->xset);
          }
        if (mode)
          {
            --this->pending_fds;
            return descriptor(static_cast<T*>(this->access[j].ptr), mode);
          }
      }
    assert("no matching fd" == 0);
    return descriptor(0, 0);
  }

  template <typename T, size_t N>
  inline size_t poll<T, N>::num_fds() const
  {
    return this->num_subscribers;
  }
}

#endif // POLL_HPP
