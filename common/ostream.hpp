// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_OSTREAM__HPP
#define ASPELL_OSTREAM__HPP

#include "parm_string.hpp"

namespace acommon {

  // FIXME: Add Print Method compatable with printf and friends.
  //   Than avoid code bloat by using it in many places instead of
  //   out << "Bla " << something << " djdkdk " << something else << "\n"

  class OStream {
  public:
    virtual void write (char c) = 0;
    virtual void write (ParmString) = 0;
    virtual void write (const void *, unsigned int) = 0;

    void write16(unsigned short v) {write(&v, 2);}
    void write32(unsigned int v) {write(&v, 4);}

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
