// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ACOMMON_VECTOR__HPP
#define ACOMMON_VECTOR__HPP

#include <vector>

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
      push_back(t);
    }
    void append(const T * begin, unsigned int size) {
      insert(end(), begin, begin+size);
    }
    T * data() {
      return &front();
    }

    T * pbegin() {
      return &*begin();
    }
    T * pend() {
      return &*end();
    }
  };
}

#endif
