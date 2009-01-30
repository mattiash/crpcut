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
extern "C" {
#include <sys/time.h> // gettimeofday
}

TESTSUITE(timeouts)
{
  TEST(should_succeed_slow_realtime_deadline,
       DEADLINE_REALTIME_MS(200),
       NO_CORE_FILE)
  {
    usleep(50000);
  }
  TEST(should_fail_slow_realtime_deadline,
       DEADLINE_REALTIME_MS(100),
       NO_CORE_FILE)
  {
    usleep(200000);
  }

  TEST(should_succeed_slow_cputime_deadline, DEADLINE_CPU_MS(100), NO_CORE_FILE)
  {
    usleep(300000); // should usleep busy-wait, this test would fail miserably
  }

  TEST(should_fail_slow_cputime_deadline, DEADLINE_CPU_MS(500), NO_CORE_FILE)
  {
    // Note! This test case will fail for the wrong reason if NTP makes an
    // unfortunate time adjustment here, or in the unlikely event that
    // gettimeofday() consumes lots and lots of real time and very little
    // cpu time.

    struct timeval deadline;
    gettimeofday(&deadline, 0);
    deadline.tv_sec+=1;
    for (;;)
      {
        for (volatile int n = 0; n < 100000; ++n)
          ;
        struct timeval now;
        gettimeofday(&now, 0);
        if (now.tv_sec > deadline.tv_sec) break;
        if (now.tv_sec == deadline.tv_sec && now.tv_usec >= deadline.tv_usec)
          break;
      }
  }

  TEST(should_fail_slow_cputime_deadline_by_death,
       DEADLINE_CPU_MS(100),
       NO_CORE_FILE)
  {
    for (;;)
      {
      }
  }

  TEST(should_fail_slow_realtime_deadline_by_death,
       DEADLINE_REALTIME_MS(100),
       NO_CORE_FILE)
  {
    sleep(2);
  }
}
