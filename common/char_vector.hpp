// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ACOMMON_CHAR_VECTOR__HPP
#define ACOMMON_CHAR_VECTOR__HPP

#include "vector.hpp"
#include "ostream.hpp"

namespace acommon 
{
  class CharVector : public Vector<char>, public OStream
  {
  public:
    void write (char c) {append(c);}
    void write (ParmString str) {append(str, str.size());}
    void write (const char * str, unsigned int size) {append(str, size);}
  };
}

#endif
