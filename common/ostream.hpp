// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#ifndef PSPELL_OSTREAM__HPP
#define PSPELL_OSTREAM__HPP

#include "parm_string.hpp"

namespace pcommon {

  class OStream {
  public:
    virtual void write (char c) = 0;
    virtual void write (ParmString) = 0;
    virtual void write (const char *, unsigned int) = 0;

    OStream & operator << (char c) {
      write(c);
      return *this;
    }

    OStream & operator << (ParmString in) {
      write(in);
      return *this;
    }

    virtual ~OStream() {}
  };
  
}

#endif
