#ifndef PCOMMON_VECTOR__HPP
#define PCOMMON_VECTOR__HPP

#include <vector>
#include <string.h>

namespace pcommon 
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
