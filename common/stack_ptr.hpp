// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef stack_ptr
#define stack_ptr

namespace acommon {
  
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

