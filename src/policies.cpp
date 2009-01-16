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
#include <sys/resource.h>
}
namespace crpcut {

  namespace policies {

    no_core_file::no_core_file()
    {
      rlimit r = { 0, 0};
      ::setrlimit(RLIMIT_CORE, &r);
    }

    namespace deaths {
      void none::expected_death(std::ostream &os)
      {
        os << "normal exit";
      }
    }

    namespace dependencies {
      void base::register_success(bool value)
      {
        if (value)
          {
            if (state != not_run) return;
            state = success;
            for (basic_enforcer *p = dependants; p; p = p->next)
              {
                --p->num;
              }
          }
        else
          {
            state = fail;
          }
      }

    } // namespace dependencies

    namespace timeout {

      basic_enforcer::basic_enforcer(type t, unsigned ms)
        : duration_ms(ms)
      {
        ::clock_gettime(t == realtime
                        ? CLOCK_MONOTONIC
                        : CLOCK_PROCESS_CPUTIME_ID,
                        &ts);
      }

      void basic_enforcer::check(type t)
      {
        timespec now;
        ::clock_gettime(t == realtime
                        ? CLOCK_MONOTONIC
                        : CLOCK_PROCESS_CPUTIME_ID,
                        &now);
        now.tv_sec -= ts.tv_sec;
        if (now.tv_nsec < ts.tv_nsec)
          {
            now.tv_nsec += 1000000000;
            now.tv_sec -= 1;
          }
        now.tv_nsec -= ts.tv_nsec;
        unsigned long ms = now.tv_sec*1000 + now.tv_nsec / 1000000;
        if (ms > duration_ms)
          {
            std::ostringstream os;
            os << (t == realtime ? "Realtime" : "Cputime")
               << " timeout " << duration_ms
               << "ms exceeded.\n  Actual time to completion was "
               << ms << "ms";
            report(comm::exit_fail, os);
          }
      }

      cputime_enforcer::cputime_enforcer(unsigned timeout_ms)
        : basic_enforcer(cputime, timeout_ms)
      {
        rlimit r = { (timeout_ms + 1500) / 1000, (timeout_ms + 2500) / 1000 };
        setrlimit(RLIMIT_CPU, &r);
      }

      cputime_enforcer::~cputime_enforcer()
      {
        basic_enforcer::check(cputime);
      }

      monotonic_enforcer::monotonic_enforcer(unsigned timeout_ms)
        : basic_enforcer(realtime, timeout_ms)
      {
        timespec deadline = ts;
        deadline.tv_nsec += (timeout_ms % 1000) * 1000000;
        deadline.tv_sec += deadline.tv_nsec / 1000000000;
        deadline.tv_nsec %= 1000000000;
        deadline.tv_sec += timeout_ms / 1000 + 1;
        // calculated deadline + 1 sec should give plenty of slack
        report(comm::set_timeout, deadline);
      }
      monotonic_enforcer::~monotonic_enforcer()
      {
        report(comm::cancel_timeout, 0, 0);
        basic_enforcer::check(realtime);
      }

    } // namespace timeout

  } // namespace policien
}
