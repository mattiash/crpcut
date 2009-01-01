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
  iterator end() { return iterator(&operator[](num_elements)); }
  const_iterator end() const { return iterator(&operator[](num_elements)); }
  reverse_iterator rbegin() { return reverse_iterator(end()); }
  const reverse_iterator rbegin() const { return reverse_iterator(end()); }
  using array::rend;
  size_type size() const { return num_elements; }
  using array::max_size;
  bool empty() const { return size() == 0; }

  using array::operator[];
  reference at(size_type n)
  {
    if (n > N) throw std::out_of_range("array_v::at");
    return operator[](n);
  }
  const_reference at(size_type n) const
  {
    if (n > N) throw std::out_of_range("array_v::at");
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
