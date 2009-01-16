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
#include <ostream>
#include <iomanip>

namespace crpcut {

  namespace xml {

    tag_t::~tag_t()
    {
      if (state_ == in_name)
        {
          os_ << "/>\n";
        }
      else
        {
          if (state_ == in_children)
            {
              os_  << std::setw(indent_*2) << "";
            }
          os_ << "</" << name_ << ">\n";
        }
    }

    void tag_t::output_data(const char *i)
    {
      if (state_ != in_data)
        {
          os_ << ">";
          state_ = in_data;
        }
      for (; *i; ++i)
        {
          unsigned char u = *i;
          switch (u)
            {
            case '&':
              os_ << "&amp;"; break;
            case '<':
              os_ << "&lt;"; break;
            case '>':
              os_ << "&gt;"; break;
            case '"':
              os_ << "&quot;"; break;
            case '\'':
              os_ << "&apos;"; break;
            default:
              if (u < 128)
                {
                  os_ << *i;
                }
              else
                {
                  os_ << "&#" << int(u) << ';';
                }
            }
        }
    }

    void tag_t::introduce()
    {
      if (parent_ && parent_->state_ != in_data)
        {
          if (parent_->state_ == in_name)
            {
              os_ << ">\n";
            }
          parent_->state_ = in_children;
          os_ << std::setw(indent_ * 2) << "";
        }
      os_ << '<' << name_;
    }

  } // namespace xml

}
