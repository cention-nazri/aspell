// Copyright (c) 2000
// Kevin Atkinson
//
// Permission to use, copy, modify, distribute and sell this software
// and its documentation for any purpose is hereby granted without
// fee, provided that the above copyright notice appear in all copies
// and that both that copyright notice and this permission notice
// appear in supporting documentation. Kevin Atkinson makes no
// representations about the suitability of this software for any
// purpose.  It is provided "as is" without express or implied
// warranty.

#ifndef autil__copy_ptr_t
#define autil__copy_ptr_t

#include "copy_ptr.hpp"
#include "generic_copy_ptr-t.hpp"

namespace pcommon {

  template <typename T>
  T * CopyPtr<T>::Parms::clone(const T * ptr) const {
    return new T(*ptr);
  }

  template <typename T>
  void CopyPtr<T>::Parms::assign(T * & rhs, const T * lhs) const {
    *rhs = *lhs;
  }

  template <typename T>
  void CopyPtr<T>::Parms::del(T * ptr) {
    delete ptr;
  }
}

#endif
