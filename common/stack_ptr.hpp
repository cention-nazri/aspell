// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef stack_ptr
#define stack_ptr

#include <assert.h>

namespace acommon {

  template<class T>
  struct StackPtrRef {
    T * ptr;
    StackPtrRef (T * rhs) : ptr(rhs) {}
  };
  
  template <typename T>
  class StackPtr {
    T * ptr;


  public:

    explicit StackPtr(T * p = 0) : ptr(p) {}

    StackPtr(StackPtr & other) : ptr (other.release()) {}
    ~StackPtr() {del();}

    StackPtr & operator=(StackPtr & other) 
      {reset(other.release()); return *this;}

    T & operator*  () const {return *ptr;}
    T * operator-> () const {return ptr;}
    T * get()         const {return ptr;}
    operator T * ()   const {return ptr;}

    T * release () {T * p = ptr; ptr = 0; return p;}

    void del() {delete ptr; ptr = 0;}
    void reset(T * p) {assert(ptr==0); ptr = p;}
    StackPtr & operator=(T * p) {reset(p); return *this;}
    
    StackPtr(StackPtrRef<T> rhs) : ptr(rhs.ptr) {}

    StackPtr& operator= (StackPtrRef<T> rhs) {reset(rhs.ptr); return *this;}

    template<class Y>
    operator StackPtrRef<Y>() {return StackPtrRef<Y>(release());}

    template<class Y>
    operator StackPtr<Y>()  {return StackPtr<Y>(release());}
    
  };
}

#endif

