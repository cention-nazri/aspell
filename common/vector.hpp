// This file is part of The New Aspell
// Copyright (C) 2001-2003 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_VECTOR__HPP
#define ASPELL_VECTOR__HPP

#include <vector>
#include <string.h>

namespace acommon
{
  template <typename T>
  class Vector : public std::vector<T>
  {
  public:

    Vector() {}
    Vector(unsigned int s) : std::vector<T>(s) {}
    Vector(unsigned int s, const T & val) : std::vector<T>(s, val) {}

    void append(T t) {
      this->push_back(t);
    }
    void append(const T * begin, unsigned int size) {
      insert(this->end(), begin, begin+size);
    }
    int alloc(int s) {
      int pos = size();
      this->resize(pos + s);
      return pos;
    }
    T * data() {return &*this->begin();}
    T * data(int pos) {return &*this->begin() + pos;}
    T * data_end() {return &*this->end();}

    T * pbegin() {return &*this->begin();}
    T * pend()   {return &*this->end();}

    const T * pbegin() const {return &*this->begin();}
    const T * pend()   const {return &*this->end();}

    template <typename U>
    U * datap() { 
      return reinterpret_cast<U * >(&front());
    }
    template <typename U>
    U * datap(int pos) {
      return reinterpret_cast<U * >(&front() + pos);
    }
  };
}

#endif
