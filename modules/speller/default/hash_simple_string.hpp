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

#ifndef __autil_hash_simple_string_hh__
#define __autil_hash_simple_string_hh__

#include "hash_fun.hpp"
#include "simple_string.hpp"

namespace acommon {

  template<> struct hash<aspeller::SimpleString> 
    : public hash<const char *> {
    size_t operator() (aspeller::SimpleString s) const 
    {
      return hash<const char *>::operator()(s.c_str());
    }
  };

}

#endif
