// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_GET_DATA__HPP
#define ASPELL_GET_DATA__HPP

#include <stddef.h>
#include <limits.h>
#include "mutable_string.hpp"

namespace acommon {

  class IStream;
  class String;

  // NOTE: getdata_pair WILL NOT unescape a string

  struct Buffer {
    char * data;
    size_t size;
    Buffer(char * d = 0, size_t s = 0) : data(d), size(s) {}
  };

  template <size_t S = 128>
  struct FixedBuffer : public Buffer
  {
    char buf[S];
    FixedBuffer() : Buffer(buf, S) {}
  };

  struct DataPair {
    MutableString key;
    MutableString value;
  };

  bool getdata_pair(IStream & in, DataPair & d, const Buffer & buf);
  static inline bool getdata_pair(IStream & in, DataPair & d, 
                                  char * buf, size_t len)
  {
    return getdata_pair(in, d, Buffer(buf, len));
  }

  void unescape(char * dest, const char * src);
  static inline void unescape(char * dest) {unescape(dest, dest);}

  // if limit is not given than dest should have enough space for 
  // twice the number of characters of src
  bool escape(char * dest, const char * src, 
	      size_t limit = INT_MAX, const char * others = 0);

  void to_lower(char *);

}
#endif
