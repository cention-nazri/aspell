// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "string.hpp"
#include <asc_ctype.hpp>

namespace acommon {
  
  void String::write(char c)
  {
    *this += c;
  }

  void String::write(ParmString str)
  {
    append(str);
  }

  void String::write(const void * str, unsigned int size)
  {
    append(static_cast<const char *>(str), size);
  }

  bool StringIStream::getline(String & str, char delem)
  {
    if (in_str[0] == '\0') return false;
    const char * end = in_str;
    bool prev_slash = false;
    while ((prev_slash || *end != delem) && *end != '\0') {
      prev_slash = *end == '\\';
      ++end;
    }
    str.assign(in_str, end - in_str);
    in_str = end;
    if (*in_str == delem) ++in_str;
    return true;
  }

  bool StringIStream::read(void * data, unsigned int size)
  {
    char * str = static_cast<char *>(data);
    while (*in_str != '\0' && size != 0) {
      *str = *in_str;
      ++in_str;
      ++str;
      ++size;
    }
    return size == 0;
  }

  String & String::no_case() {
    unsigned int countcase=0;
    for (countcase = 0; countcase < length(); countcase++) {
      (*this)[countcase] = asc_tolower((*this)[countcase]);
    }
    return *this;
  }
  
}
