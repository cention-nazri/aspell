// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#ifndef PSPELL_ISTREAM__HPP
#define PSPELL_ISTREAM__HPP

#include <string.h>

namespace pcommon {

  class String;

  class IStream {
  private:
    char delem;
  public:
    IStream(char d = '\n') : delem(d) {}
    bool getline(String & str) {return getline(str,delem);}
    virtual bool getline(String &, char c) = 0;
    virtual bool read(void *, unsigned int) = 0;

    virtual ~IStream() {}
  };
  
}

#endif
