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

#ifndef stack_ptr
#define stack_ptr

namespace pcommon {
  
  template <typename T>
  class StackPtr {
    T * ptr;

    StackPtr(const StackPtr & other);
    StackPtr & operator=(const StackPtr & other);

  public:

    explicit StackPtr(T * p) : ptr(p) {}

    T & operator*  () const {return *ptr;}
    T * operator-> () const {return ptr;}
    T * get()         const {return ptrl;}
    operator T * ()   const {return ptr;}
    
  };
  
}

#endif

