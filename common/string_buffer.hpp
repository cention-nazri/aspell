// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "basic_list.hpp"

namespace acommon {

  class StringBuffer {
  public: // but dont use
    static const unsigned int   buf_size = 1024 - 16;
    struct Buf {
      char buf[buf_size];
    };
  private:
    static const Buf sbuf;
    BasicList<Buf> bufs;
    unsigned int fill;
  public:
    StringBuffer();
    char * alloc(unsigned int size);
  };

}
