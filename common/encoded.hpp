// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef PCOMMON_ENCODED__HPP
#define PCOMMON_ENCODED__HPP

#include "parm_string.hpp"
#include "vector.hpp"

namespace acommon
{
  struct Encoded {
    Vector<char> temp_str_1;
    Vector<char> temp_str_2;
    void (from_encoded_ *)(ParmString, Vector<char> &);
    void (to_encoded_ *)(ParmString, Vector<char> &);
    Encoded() : from_encoded_(0), to_encoded_(0) {}
  };

}
