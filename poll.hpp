#ifndef POLL_HPP
#define POLL_HPP

#include <cassert>

#ifdef POLL_USE_EPOLL
extern "C"
{
#include <sys/epoll.h>
}
template <size_t N>
struct polldata
{
  int epoll_fd;
};
#endif

#ifdef POLL_USE_SELECT
extern "C"
{
#include <sys/select.h>
}


template <size_t N>
struct polldata

{
  polldata()
    : num_subscribers(0U),
      pending_fds(0U)
  {
    FD_ZERO(&rset);
    FD_ZERO(&xset);
  }
  std::pair<int, void*> access[N];
  size_t num_subscribers;
  fd_set rset;
  fd_set xset;
  size_t pending_fds;

  static const int readbit = 1;
  static const int hupbit = 2;
};
#endif

#ifdef POLL_USE_POLL
#endif

template <typename T, size_t N>
class poll
{
public:
  class descriptor
  {
  public:
    T* operator->() const { return data; }
    T* get() const { return data; }
    bool read() const;
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
  void add_fd(int fd, T* data);
  void del_fd(int fd);
  descriptor wait(int timeout_ms = -1);
private:
  polldata<N> data;
};

#ifdef POLL_USE_SELECT
template <typename T, size_t N>
inline bool poll<T, N>::descriptor::read() const
{
  return mode & polldata<N>::readbit;
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
inline void poll<T, N>::add_fd(int fd, T* data)
{
  this->data.access[this->data.num_subscribers++] = std::make_pair(fd, data);
}
template <typename T, size_t N>
inline void poll<T, N>::del_fd(int fd)
{
  for (size_t i = 0; i < data.num_subscribers; ++i)
    {
      if (data.access[i].first == fd)
        {
          data.access[i] = data.access[--data.num_subscribers];
          if (FD_ISSET(fd, &data.rset) && FD_ISSET(fd, &data.xset))
            {
              FD_CLR(fd, &data.rset);
              FD_CLR(fd, &data.xset);
              --data.pending_fds;
            }
          return;
        }
    }
  assert("fd not found" == 0);
}
template <typename T, size_t N>
inline typename poll<T, N>::descriptor poll<T, N>::wait(int timeout_ms)
{
  if (data.pending_fds == 0)
    {
      int maxfd = 0;
      for (size_t i = 0; i < data.num_subscribers; ++i)
        {
          int fd = data.access[i].first;
          if (fd > maxfd) maxfd = fd;
          FD_SET(fd, &data.rset);
          FD_SET(fd, &data.xset);
        }
      struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
      for (;;)
        {
          int rv = select(maxfd + 1,
                          &data.rset,
                          0,
                          &data.xset,
                          timeout_ms == -1 ? 0 : &tv);
          if (rv == -1 && errno == EINTR) continue;
          assert(rv != -1);
          if (rv == 0) return descriptor(0,0); // timeout
          data.pending_fds = rv;
          break;
        }
    }
  for (size_t j = 0; j < data.num_subscribers; ++j)
    {
      int fd = data.access[j].first;
      int mode = 0;
      if (FD_ISSET(fd, &data.rset))
        {
          mode|= polldata<N>::readbit;
          FD_CLR(fd, &data.rset);
        }
      if (FD_ISSET(fd, &data.xset))
        {
          mode|= polldata<N>::hupbit;
          FD_CLR(fd, &data.xset);
        }
      if (mode)
        {
          --data.pending_fds;
          return descriptor(static_cast<T*>(data.access[j].second),
                            mode);
        }
    }
  assert("no matching fd" == 0);
}
#endif
#ifdef POLL_USE_EPOLL
template <typename T, size_t N>
inline bool poll<T, N>::descriptor::read() const
{
  return mode & EPOLLIN;
}
template <typename T, size_t N>
inline bool poll<T, N>::descriptor::hup() const
{
  return mode & EPOLLHUP;
}

template <typename T, size_t N>
inline poll<T, N>::poll()
{
  data.epoll_fd = epoll_create(N);
  assert(data.epoll_fd != -1);
}
template <typename T, size_t N>
inline poll<T, N>::~poll()
{
  ::close(data.epoll_fd);
}
template <typename T, size_t N>
inline void poll<T, N>::add_fd(int fd, T* data)
{
  epoll_event ev;
  ev.events = EPOLLIN | EPOLLHUP;
  ev.data.ptr = data;
  int rv = epoll_ctl(this->data.epoll_fd, EPOLL_CTL_ADD, fd, &ev);
  assert(rv == 0);
}

template <typename T, size_t N>
inline void poll<T, N>::del_fd(int fd)
{
  int rv = epoll_ctl(data.epoll_fd, EPOLL_CTL_DEL, fd, 0);
  assert(rv == 0);
}

template <typename T, size_t N>
inline typename poll<T, N>::descriptor poll<T, N>::wait(int timeout_ms)
{
  epoll_event ev;
  for (;;)
    {
      int rv = epoll_wait(data.epoll_fd, &ev, 1, timeout_ms);
      if (rv == 0)
        {
          return descriptor(0,0); // timeout
        }
      if (rv == 1)
        {
          return descriptor(static_cast<T*>(ev.data.ptr), ev.events);
        }
    }
}
#endif

#endif // POLL_HPP
