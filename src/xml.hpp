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

#ifndef XML_HPP
#define XML_HPP

#include <sstream>

namespace xml {

  class tag_t
  {
  public:
    operator void*() const { return 0; }
    tag_t(const char *name, std::ostream &os)
      : name_(name),
        state_(in_name),
        indent_(0),
        os_(os),
        parent_(0)
    {
      introduce();
    }
    tag_t(const char *name, tag_t &parent)
      : name_(name),
        state_(in_name),
        indent_(parent.indent_+1),
        os_(parent.os_),
        parent_(&parent)
    {
      introduce();
      parent.state_ = in_children;
    }
    ~tag_t();
    template <typename T>
    tag_t & operator<<(const T& t)
    {
      std::ostringstream o;
      o << t;
      output_data(o.str().c_str());
      return *this;
    }
    tag_t &operator<<(const char *p) { output_data(p); return *this; }
  private:
    void output_data(const char *p);
    void introduce();

    const char *name_;
    enum { in_name, in_children, in_data } state_;
    int indent_;
    std::ostream &os_;
    tag_t *parent_;
  };
}

#define CRPCUT_XML_TAG(name, ...)                                       \
  if (xml::tag_t name = xml::tag_t(#name, __VA_ARGS__))                 \
    {                                                                   \
    }                                                                   \
  else

#endif // XML_HPP
