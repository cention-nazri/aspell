// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "string_buffer.hpp"

namespace acommon {

  const StringBuffer::Buf StringBuffer::sbuf = {{0}};

  StringBuffer::StringBuffer() 
    : fill(1) 
  {
    bufs.push_front(sbuf);
  }

  char * StringBuffer::alloc(unsigned int size) {
    if (fill + size > buf_size) {
      fill = 1;
      bufs.push_front(sbuf);
    }
    char * s = bufs.front().buf + fill;
    fill += size;
    return s;
  }

}
