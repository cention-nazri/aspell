// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_ISTREAM__HPP
#define ASPELL_ISTREAM__HPP

#include <string.h>

namespace acommon {

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
