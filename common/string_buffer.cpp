
#include "string_buffer.hpp"

namespace pcommon {

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
