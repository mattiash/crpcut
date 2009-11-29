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
#include "ilist_element.hpp"

struct elem : public ilist_element<elem>
{
  elem(int i) : n(i) {}
  int n;
};

TEST(create_and_destroy_empty)
{
  ilist_element<elem> list;
  ASSERT_TRUE(list.is_empty());
}

TEST(insert_and_traverse_one_element)
{
  ilist_element<elem> list;
  elem obj(1);
  list.insert_after(obj);
  ASSERT_FALSE(list.is_empty());
  ASSERT_TRUE(list.next()->n == 1);
}

TEST(several_elements)
{
  ilist_element<elem> list;

  elem obj1(1);
  list.insert_after(obj1);
  elem *p2 = new elem(2);
  list.insert_after(*p2);
  elem obj3(3);
  list.insert_after(obj3);

  elem *i = list.next();
  ASSERT_EQ(i->n, 3);
  ASSERT_EQ((i=i->next())->n, 2);
  ASSERT_EQ((i=i->next())->n, 1);
  ASSERT_EQ(i, &list);
  delete p2;
  ASSERT_EQ((i=i->prev())->n, 1);
  ASSERT_EQ((i=i->prev())->n, 3);
  ASSERT_EQ(i->prev(), &list);
}

TEST(unlink)
{
  ilist_element<elem> list;
  elem obj1(1);
  list.insert_after(obj1);
  elem obj2(2);
  list.insert_after(obj2);
  elem obj3(3);
  list.insert_after(obj3);
  obj2.unlink();
  elem *i = list.prev();
  ASSERT_EQ(i->n, 1);
  i = i->prev();
  ASSERT_EQ(i->n, 3);
}

int main(int argc, char *argv[])
{
  return crpcut::run(argc, argv);
}
