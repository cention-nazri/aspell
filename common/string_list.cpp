// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#include "string_list_impl.hpp"

namespace pcommon {

  void StringListImpl::copy(const StringListImpl & other)
  {
    StringListNode * * cur = &first;
    StringListNode * other_cur = other.first;
    while (other_cur != 0) {
      *cur = new StringListNode(other_cur->data.c_str());
      cur = &(*cur)->next;
      other_cur = other_cur->next;
    }
    *cur = 0;
  }

  void StringListImpl::destroy()
  {
    while (first != 0) {
      StringListNode * next = first->next;
      delete first;
      first = next;
    }
  }

  bool operator==(const StringListImpl & rhs, 
		  const StringListImpl & lhs)
  {
    StringListNode * rhs_cur = rhs.first;
    StringListNode * lhs_cur = lhs.first;
    while (rhs_cur != 0 && lhs_cur != 0 && rhs_cur->data == lhs_cur->data) {
      rhs_cur = rhs_cur->next;
      lhs_cur = lhs_cur->next;
    }
    return rhs_cur == 0 && lhs_cur == 0;
  }

  StringEmulation * StringListEmulation::clone() const
  {
    return new StringListEmulation(*this);
  }

  void StringListEmulation::assign(const StringEmulation * other)
  {
    *this = *(const StringListEmulation *)other;
  }


  StringList * StringListImpl::clone() const
  {
    return new StringListImpl(*this);
  }

  void StringListImpl::assign(const StringList * other)
  {
    *this = *(const StringListImpl *)other;
  }

  bool StringListImpl::add(ParmString str)
  {
    StringListNode * * cur = & first;
    while (*cur != 0 && strcmp((*cur)->data.c_str(), str) != 0)
      cur = &(*cur)->next;
    if (*cur == 0) {
      *cur = new StringListNode(str);
      return true;
    } else {
      return false;
    }
  }

  bool StringListImpl::remove(ParmString str)
  {
    StringListNode * * prev = 0;
    StringListNode * * cur  = & first;
    while (*cur != 0 && strcmp((*cur)->data.c_str(), str)!=0 )  {
      prev = cur;
      cur = &(*cur)->next;
    }
    if (*cur == 0) {
      return false;
    } else {
      *prev = (*cur)->next;
      delete *cur;
      return true;
    }
  }

  void StringListImpl::clear()
  {
    StringListNode * temp;
    while (first != 0) {
      temp = first;
      first = temp->next;
      delete temp;
    }
    first = 0;
  }

  StringEmulation * StringListImpl::elements() const
  {
    return new StringListEmulation(first);
  }

  StringList * new_string_list() {
    return new StringListImpl;
  }

}
