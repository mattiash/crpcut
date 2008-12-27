#ifndef POLL_HPP
#define POLL_HPP

#include <cassert>

#ifdef POLL_USE_EPOLL
extern "C"
{
#include <sys/epoll.h>
}
typedef int polldata;
#endif

#ifdef POLL_USE_SELECT
extern "C"
{
#include <sys/select.h>
}
#include <vector>
struct polldata
{
  polldata() : pending_fds(0U) { FD_ZERO(&rset); FD_ZERO(&xset); }
  std::vector<std::pair<int, void*> > access;
  fd_set rset;
  fd_set xset;
  size_t pending_fds;

  static const int readbit = 1;
  static const int hupbit = 2;
};
#endif

#ifdef POLL_USE_POLL
#endif

template <typename T>
class poll
{
public:
  class descriptor
  {
  public:
    T* operator->() const { return data; }
    bool read() const;
    bool hup() const;
    bool timeout() const { return mode == 0; }
  private:
    descriptor(T* t, int m) : data(t), mode(m) {}

    T* data;
    int mode;

    friend class poll<T>;
  };
  poll();
  ~poll();
  void add_fd(int fd, T* data);
  void del_fd(int fd);
  descriptor wait(int timeout_ms = -1);
private:
  polldata data;
};

#ifdef POLL_USE_SELECT
template <typename T>
inline bool poll<T>::descriptor::read() const
{
  return mode & polldata::readbit;
}
template <typename T>
inline bool poll<T>::descriptor::hup() const
{
  return mode & polldata::hupbit;
}
template <typename T>
inline poll<T>::poll()
{
}
template <typename T>
inline poll<T>::~poll()
{
}

template <typename T>
inline void poll<T>::add_fd(int fd, T* data)
{
  this->data.access.push_back(std::make_pair(fd, data));
}
template <typename T>
inline void poll<T>::del_fd(int fd)
{
  for (size_t i = 0; i < data.access.size(); ++i)
    {
      if (data.access[i].first == fd)
        {
          data.access[i] = data.access.back();
          data.access.pop_back();
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
template <typename T>
inline typename poll<T>::descriptor poll<T>::wait(int timeout_ms)
{
  if (data.pending_fds == 0)
    {
      int maxfd = 0;
      for (size_t i = 0; i < data.access.size(); ++i)
        {
          int fd = data.access[i].first;
          if (fd > maxfd) maxfd = fd;
          FD_SET(fd, &data.rset);
          FD_SET(fd, &data.xset);
        }
      struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
      int rv = select(maxfd + 1,
                      &data.rset,
                      0,
                      &data.xset,
                      timeout_ms == -1 ? 0 : &tv);
      assert(rv != -1);
      if (rv == 0) return descriptor(0,0); // timeout
      data.pending_fds = rv;
    }
  for (size_t j = 0; j < data.access.size(); ++j)
    {
      int fd = data.access[j].first;
      int mode = 0;
      if (FD_ISSET(fd, &data.rset))
        {
          mode|= polldata::readbit;
          FD_CLR(fd, &data.rset);
        }
      if (FD_ISSET(fd, &data.xset))
        {
          mode|= polldata::hupbit;
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
template <typename T>
inline bool poll<T>::descriptor::read() const
{
  return mode & EPOLLIN;
}
template <typename T>
inline bool poll<T>::descriptor::hup() const
{
  return mode & EPOLLHUP;
}

template <typename T>
inline poll<T>::poll()
{
  data = epoll_create(1);
  assert(data != -1);
}
template <typename T>
inline poll<T>::~poll()
{
  ::close(data);
}
template <typename T>
inline void poll<T>::add_fd(int fd, T* data)
{
  epoll_event ev;
  ev.events = EPOLLIN | EPOLLHUP;
  ev.data.ptr = data;
  int rv = epoll_ctl(this->data, EPOLL_CTL_ADD, fd, &ev);
  assert(rv == 0);
}

template <typename T>
inline void poll<T>::del_fd(int fd)
{
  int rv = epoll_ctl(data, EPOLL_CTL_DEL, fd, 0);
  assert(rv == 0);
}

template <typename T>
inline typename poll<T>::descriptor poll<T>::wait(int timeout_ms)
{
  epoll_event ev;
  for (;;)
    {
      int rv = epoll_wait(data, &ev, 1, timeout_ms);
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
