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
#include "clocks.hpp"
extern "C" {
#include <sys/resource.h>
}
namespace crpcut {

  namespace policies {

    no_core_file::no_core_file()
    {
      rlimit r = { 0, 0};
      crpcut::setrlimit(RLIMIT_CORE, &r);
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

      cputime_enforcer::cputime_enforcer(unsigned timeout_ms)
        : duration_ms(timeout_ms),
          start_timestamp_ms(clocks::cputime::timestamp_ms_absolute())
      {
        rlimit r = { (duration_ms + 1500) / 1000, (duration_ms + 2500) / 1000 };
        crpcut::setrlimit(RLIMIT_CPU, &r);
      }

      cputime_enforcer::~cputime_enforcer()
      {
        unsigned now = clocks::cputime::timestamp_ms_absolute();
        int diff = now - start_timestamp_ms;
        if  (diff > int(duration_ms))
          {
            stream::toastream<128> os;
            os << "CPU-time timeout " << duration_ms
               << "ms exceeded.\n  Actual time to completion was " << diff
               << "ms";
            report(comm::exit_fail, os.size(), os.begin());
          }
      }

      monotonic_enforcer::monotonic_enforcer(unsigned timeout_ms)
        : duration_ms(timeout_ms),
          start_timestamp_ms(clocks::monotonic::timestamp_ms_absolute())
      {

        // calculated deadline + 1 sec should give plenty of slack
        unsigned deadline = duration_ms + 1000;
        report(comm::set_timeout, deadline);
      }

      monotonic_enforcer::~monotonic_enforcer()
      {
        unsigned now = clocks::monotonic::timestamp_ms_absolute();
        report(comm::cancel_timeout, 0, 0);
        int diff = now - start_timestamp_ms;
        if (diff > int(duration_ms))
          {
            stream::toastream<128> os;
            os << "Realtime timeout " << duration_ms
               << "ms exceeded.\n  Actual time to completion was " << diff
               << "ms";
            report(comm::exit_fail, os.size(), os.begin());
          }
      }

    } // namespace timeout

  } // namespace policien
}
