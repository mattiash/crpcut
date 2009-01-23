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

#include "clocks.hpp"
#include <cassert>

#if   !defined(HAVE_MACH_ABSOLUTE_TIME)                                 \
  &&  !defined(HAVE_CLOCK_GETTIME)                                      \
  &&  !defined(HAVE_ITIMER)                                             \
  &&  !defined(HAVE_GETTIMEOFDAY)
  #error "You can't be serious, no clocks available at all?"
#endif
extern "C"
{
#if defined(HAVE_MACH_ABSOLUTE_TIME)
#include <stdint.h>
#include <mach/mach_time.h>
#endif
#if defined(HAVE_CLOCK_GETTIME)
#include <time.h>
#endif
#if defined(HAVE_ITIMER) || defined(HAVE_GETTIMEOFDAY)
#include <sys/time.h>
#endif
}

#if defined(HAVE_MACH_ABSOLUTE_TIME)

namespace {
  double getconv()
  {
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    return ((double)info.numer/(double)info.denom)/1000;
  }
  unsigned get_mach_high_res_timestamp()
  {
    static double conv = getconv();
    return static_cast<unsigned>(mach_absolute_time()*conv);
  }
}
namespace clocks {
  inline monotonic::timestamp_func monotonic::try_mach_high_res_timer()
  {
    return &get_mach_high_res_timestamp;
  }
}
#else
namespace clocks {
  inline monotonic::timestamp_func monotonic::try_mach_high_res_timer()
  {
    return 0;
  }
}
#endif

#if defined(HAVE_CLOCK_GETTIME)&& defined(CLOCK_MONOTONIC)
namespace {

  unsigned get_clock_gettime_monotonic_timestamp()
  {
    struct timespec ts;
    int rv = clock_gettime(CLOCK_MONOTONIC, &ts);
    assert(rv == 0);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
  }
}
namespace clocks {
  inline monotonic::timestamp_func monotonic::try_clock_gettime_monotonic()
  {
    struct timespec ts;
    int rv = clock_gettime(CLOCK_MONOTONIC, &ts);
    return rv == 0 ? &get_clock_gettime_monotonic_timestamp : 0;
  }
}
#else
namespace clocks {
  inline monotonic::timestamp_func monotonic::try_clock_gettime_monotonic()
  {
    return 0;
  }
}
#endif


#if defined(HAVE_CLOCK_GETTIME)&& defined(CLOCK_PROCESS_CPUTIME_ID)
namespace {
  unsigned get_clock_gettime_cputime_timestamp()
  {
    struct timespec ts;
    int rv = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    assert(rv == 0);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
  }
}
namespace clocks {
  inline cputime::timestamp_func cputime::try_clock_gettime_cputime()
  {
    struct timespec ts;
    int rv = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return rv == 0 ? &get_clock_gettime_cputime_timestamp : 0;
  }
}
#else
namespace clocks {
  inline cputime::timestamp_func cputime::try_clock_gettime_cputime()
  {
    return 0;
  }
}
#endif

#if defined (HAVE_ITIMER) && defined(ITIMER_REAL)
#include <iostream>
#include <ostream>
namespace {
  unsigned get_itimer_real_timestamp()
  {
    static pid_t initialized = 0;
    pid_t pid = getpid();
    if (initialized != pid)
      {
        struct itimerval v = { { 0, 0 }, { 99999, 0 } };
        int rv = setitimer(ITIMER_REAL, &v, 0);
        assert(rv == 0);
        initialized = pid;
      }
    struct itimerval v;
    int rv = getitimer(ITIMER_REAL, &v);
    assert(rv == 0);
    return (99999 - v.it_value.tv_sec)*1000 + 1000 - v.it_value.tv_usec/1000;
  }
}
namespace clocks {
  inline monotonic::timestamp_func monotonic::try_getitimer_real()
  {
    struct itimerval v;
    int rv = getitimer(ITIMER_REAL, &v);
    return rv == 0 ? &get_itimer_real_timestamp : 0;
  }
}
#else
namespace clocks {
  inline monotonic::timestamp_func monotonic::try_getitimer_real()
  {
    return 0;
  }
}
#endif


#if defined (HAVE_ITIMER) && defined(ITIMER_VIRTUAL)
namespace {
  unsigned get_itimer_virtual_timestamp()
  {
    static bool initialized = false;
    if (!initialized)
      {
        struct itimerval v = { { 0, 0 }, { 99999, 0 } };
        int rv = setitimer(ITIMER_VIRTUAL, &v, 0);
        assert(rv == 0);
        initialized = true;
      }
    struct itimerval v;
    int rv = getitimer(ITIMER_VIRTUAL, &v);
    assert(rv == 0);
    return (99999 - v.it_value.tv_sec)*1000 + 1000 - v.it_value.tv_usec/1000;
  }
}
namespace clocks {
  inline cputime::timestamp_func cputime::try_getitimer_virtual()
  {
    struct itimerval v;
    int rv = getitimer(ITIMER_VIRTUAL, &v);
    return rv == 0 ? &get_itimer_virtual_timestamp : 0;
  }
}
#else
namespace clocks {
  inline cputime::timestamp_func cputime::try_getitimer_virtual()
  {
    return 0;
  }
}
#endif

#if defined (HAVE_ITIMER) && defined(ITIMER_PROF)
namespace {
  unsigned get_itimer_prof_timestamp()
  {
    static bool initialized = false;
    if (!initialized)
      {
        struct itimerval v = { { 0, 0 }, { 99999, 0 } };
        int rv = setitimer(ITIMER_PROF, &v, 0);
        assert(rv == 0);
        initialized = true;
      }
    struct itimerval v;
    int rv = getitimer(ITIMER_PROF, &v);
    assert(rv == 0);
    return (99999 - v.it_value.tv_sec)*1000 + 1000 - v.it_value.tv_usec/1000;
  }
}
namespace clocks {
  inline cputime::timestamp_func cputime::try_getitimer_prof()
  {
    struct itimerval v;
    int rv = getitimer(ITIMER_PROF, &v);
    return rv == 0 ? &get_itimer_prof_timestamp : 0;
  }
}
#else
namespace clocks {
  inline cputime::timestamp_func cputime::try_getitimer_prof()
  {
    return 0;
  }
}
#endif



#if defined(HAVE_GETTIMEOFDAY)
namespace {
  unsigned get_gettimeofday_timestamp()
  {
    struct timeval tv;
    int rv = gettimeofday(&tv, 0);
    assert(rv == 0);
    return tv.tv_sec*1000 + tv.tv_usec/1000;
  }
}
namespace clocks {
  inline monotonic::timestamp_func monotonic::try_gettimeofday()
  {
    struct timeval tv;
    int rv = gettimeofday(&tv, 0);
    return rv == 0 ? &get_gettimeofday_timestamp : 0;
  }
}
#else
namespace clocks {
  inline monotonic::timestamp_func monotonic::try_gettimeofday()
  {
    return 0;
  }
}
#endif

namespace clocks {
  monotonic::timestamp_func monotonic::func;
  const char *monotonic::name = "<no clock>";
  monotonic::initializer monotonic::bootstrap;

  monotonic::initializer::initializer()
  {
    if ((func = try_mach_high_res_timer()) != 0)
      {
        name = "mach high res timer";
      }
    else if ((func = try_clock_gettime_monotonic()) != 0)
      {
        name = "monotonic posix clock";
      }
    else if ((func = try_getitimer_real()) != 0)
      {
        name = "getitimer realtime";
      }
    else if ((func = try_gettimeofday()) != 0)
      {
        name = "gettimeofday";
      }
    else
      {
        assert("Don't you have monotonic or realtime clocks on this system?" == 0);
      }
  }


  cputime::timestamp_func cputime::func;
  const char *cputime::name = "<no clock>";
  cputime::initializer cputime::bootstrap;

  cputime::initializer::initializer()
  {
    if ((func = try_clock_gettime_cputime()) != 0)
      {
        name = "process cputime posix clock";
      }
    else if ((func = try_getitimer_prof()) != 0)
      {
        name = "getitimer prof";
      }
    else if ((func = try_getitimer_virtual()) != 0)
      {
        name = "getitimer virtual";
      }
    else
      {
        assert("Don't you have *any* cpu-time clocks on this system?" == 0);
      }
  }
}
