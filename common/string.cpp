#include "string.hpp"

namespace pcommon {
  
  void String::write(char c)
  {
    *this += c;
  }

  void String::write(ParmString str)
  {
    append(str);
  }

  void String::write(const char * str, unsigned int size)
  {
    append(str, size);
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
  
}
