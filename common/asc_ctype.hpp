// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASC_CTYPE__HPP
#define ASC_CTYPE__HPP

#include "parm_string.hpp"
#include "posib_err.hpp"

namespace acommon {

  static inline bool asc_isspace(char c) 
  {
    return c==' ' || c=='\n' || c=='\r' || c=='\t' || c=='\f' || c=='\v';
  }

  static inline bool asc_isdigit(char c)
  {
    return '0' <= c && c <= '9';
  }
  static inline bool asc_islower(char c)
  {
    return 'a' <= c && c <= 'z';
  }
  static inline bool asc_isupper(char c)
  {
    return 'A' <= c && c <= 'Z';
  }
  static inline bool asc_isalpha(char c)
  {
    return asc_islower(c) || asc_isupper(c);
  }
  static inline char asc_tolower(char c)
  {
    return asc_isupper(c) ? c + 0x20 : c;
  }
  static inline char asc_toupper(char c)
  {
    return asc_islower(c) ? c - 0x20 : c;
  }
  
}

#endif
