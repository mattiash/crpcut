/*
 * Copyright 2009 Bjorn Fahller <bjorn@fahller.se>
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


#include <crpcut.hpp>

extern "C"
{
#include <unistd.h>
}

class raw_dump
{
public:
  raw_dump(const char *filename) : fd(::open(filename, O_CREAT | O_EXCL))
  {
    if (fd < 0) throw std::exception("fail");
  }
  ~raw_dump()
  {
    int rv = ::close(fd);
    if (rv < 0) throw std::exception("close");
  }
  void dump(const void *buffer, size_t len)
  {
    const char *p = static_cast<const char*>(buffer);
    size_t bytes_written = 0;
    while (bytes_written < len)
      {
        int rv = ::write(fd, p + bytes_written, len - bytes_written);
        if (rv < 0) throw std::exception("write");
        if (rv == 0) return;
        bytes_written+= rv;
      }
  }
private:
  raw_dump(const raw_dump&);
  raw_dump& operator=(const raw_dump&);
  int fd;
};

namespace original {
  CRPCUT_WRAP_FUNC(libc, close, int, (int fd), (fd));
}

int *set_errno = 0;
int close(int fd)
{
  int rv = original::close(fd);
  return set_errno ? *set_errno : rv;
}

template <const char *(&fname)>
struct cleaner
{
  ~cleaner() { ::unlink(&fname); }
};

const char *filename = "dump";

TEST(failed_close, cleaner<filename>)
{
  (void)read_data("/dev/zero");
}

TEST(wrapped_with_wrong_expectations)
{
  expected_filename = "/dev/random";
  (void)read_data("/dev/zero");
}

int main(int argc, char *argv[])
{
  return crpcut::run(argc, argv);
}
