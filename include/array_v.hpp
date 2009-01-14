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


#ifndef ARRAY_V_HPP
#define ARRAY_V_HPP

#include <tr1/array>
#include <cassert>

namespace std {
  using namespace tr1;
}

template <typename T, std::size_t N>
class array_v : private std::array<T, N>
{
  typedef std::array<T, N> array;
public:
  typedef typename array::value_type             value_type;
  typedef typename array::reference              reference;
  typedef typename array::const_reference        const_reference;
  typedef typename array::iterator               iterator;
  typedef typename array::const_iterator         const_iterator;
  typedef typename array::size_type              size_type;
  typedef typename array::difference_type        difference_type;
  typedef typename array::reverse_iterator       reverse_iterator;
  typedef typename array::const_reverse_iterator const_reverse_iterator;

  array_v() : num_elements(0) {}
  using array::assign;
  void swap(array_v &other)
  {
    array &ta = *this;
    array &to = other;
    ta.swap(to);
    std::swap(num_elements, other.num_elements);
  }
  using array::begin;
  iterator end() { return iterator(&operator[](size())); }
  const_iterator end() const { return iterator(&operator[](size())); }
  reverse_iterator rbegin() { return reverse_iterator(end()); }
  const reverse_iterator rbegin() const { return reverse_iterator(end()); }
  using array::rend;
  size_type size() const { return num_elements; }
  using array::max_size;
  bool empty() const { return size() == 0; }

  using array::operator[];
  reference at(size_type n)
  {
    if (n >= num_elements) throw std::out_of_range("array_v::at");
    return operator[](n);
  }
  const_reference at(size_type n) const
  {
    if (n >= num_elements) throw std::out_of_range("array_v::at");
    return operator[](n);
  }
  using array::front;
  reference back() { return *(end() - !empty()); }
  using array::data;
  const_reference back() const { *(end() - !empty()); }
  void push_back(const value_type &x)
  {
    assert(num_elements <= N);
    operator[](num_elements++) = x;
  }
  void pop_back() { assert(num_elements); --num_elements; }
private:
  size_t num_elements;
};
#endif // ARRAY_V_HPP
