// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_GET_DATA__HPP
#define ASPELL_GET_DATA__HPP

#include <stddef.h>
#include "mutable_string.hpp"

namespace acommon {

  class IStream;
  class String;

  // NOTE: getdata_pair WILL NOT unescape a string

  struct DataPair {
    MutableString key;
    MutableString value;
  };

  bool getdata_pair(IStream & in, DataPair & d, char * buf, size_t len);

  void unescape(char *);

  void to_lower(char *);

}
#endif
