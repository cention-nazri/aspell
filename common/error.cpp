// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "error.hpp"

namespace acommon {
  bool Error::is_a(ErrorInfo const * to_find) const 
  {
    const ErrorInfo * e = err;
    while (e) {
      if (e == to_find) return true;
      e = e->isa;
    }
    return false;
  }
}
