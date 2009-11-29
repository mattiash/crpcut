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

extern "C"
{
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
}

#include <stdexcept>
#include <string>
#include <cassert>

class work
{
public:
  class fork_exception : public std::exception  { };
  class pipe_exception : public std::exception  { };
  work() :pid(-1)
  {
    int rv = pipe(fd);
    if (rv == -1) throw pipe_exception();
    rv = ::fork();
    if (rv == -1) throw fork_exception();
    if (rv == 0) { close(fd[0]); fd[0] = -1; do_work(); return; } // child
    close(fd[1]);
    fd[1] = -1;
    pid = rv;
  }
  ~work() {
    assert(pid == -1 && "Hmm, child still alive, I think");
    if (fd[0] != -1) close(fd[0]);
    if (fd[1] != -1) close(fd[1]);
  }
  std::string get_data() // empty data signals that child is done
  {
    size_t len;
    ::read(fd[0], &len, sizeof(len));
    std::string data(len > 0 ? len : 0, '_');
    if (len > 0)
      {
        ::read(fd[0], &data[0], len);
      }
    return data;
  }
  void wait()
  {
    int status;
    int rv = ::wait(&status);
    assert(rv != 0 || status == pid);
    pid = -1;
  }
private:
  void do_work() { /* very hard stuff */  }
  int fd[2];
  int pid;
};
