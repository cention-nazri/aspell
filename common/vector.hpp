// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef PCOMMON_VECTOR__HPP
#define PCOMMON_VECTOR__HPP

#include <vector>
#include <string.h>

namespace acommon 
{
  template <typename T>
  class Vector : public std::vector<T>
  {
  public:
    void append(T t) {
      push_back(t);
    }
    void append(const T * begin, unsigned int size) {
      insert(end(), begin, begin+size);
    }
    T * data() {
      return &front();
    }
  };
}

#endif
