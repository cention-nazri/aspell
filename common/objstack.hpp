
#ifndef ACOMMON_OBJSTACK__HPP
#define ACOMMON_OBJSTACK__HPP

#include "parm_string.hpp"
#include <stdlib.h>

namespace acommon {

class ObjStack
{
  typedef unsigned char byte;
  struct Node
  {
    Node * next;
    byte data[];
  };
  size_t chunk_size;
  size_t min_align;
  Node * first;
  Node * first_free;
  byte * top;
  byte * bottom;
  void new_chunk();

  ObjStack(const ObjStack &);
  void operator=(const ObjStack &);

  void align_bottom(size_t align) {
    size_t a = (size_t)bottom % align;
    if (a != 0) bottom += align - a;
  }
  void align_top(size_t align) {
    top -= (size_t)top % align;
  }
public:
  // The alignment here is the guaranteed alignment that memory in
  // new chunks will be aligned to.   It does NOT guarantee that
  // every object is aligned as such unless all objects inserted
  // are a multiple of align.
  ObjStack(size_t chunk_s = 1024, size_t align = sizeof(void *));
  ~ObjStack();
  
  // This alloc_bottom does NOT check alignment.  However, if you always
  // insert objects with a multiple of min_align than it will always
  // me aligned as such.
  void * alloc_bottom(size_t size) 
  {loop:
    byte * tmp = bottom;
    bottom += size;
    if (bottom > top) {new_chunk(); goto loop;}
    return tmp;
  }
  // This alloc_bottom will insure that the object is aligned based on the
  // alignment given.
  void * alloc_bottom(size_t size, size_t align) 
  {loop:
    align_bottom(align);
    byte * tmp = bottom;
    bottom += size;
    if (bottom > top) {new_chunk(); goto loop;}
    return tmp;
  }
  char * dup_bottom(ParmString str) {
    return (char *)memcpy(alloc_bottom(str.size() + 1), 
                          str.str(), str.size() + 1);
  }

  // This alloc_bottom does NOT check alignment.  However, if you
  // always insert objects with a multiple of min_align than it will
  // always be aligned as such.
  void * alloc_top(size_t size)
  {loop:
    top -= size;
    if (top < bottom) {new_chunk(); goto loop;}
    return top;
  }
  // This alloc_top will insure that the object is aligned based on
  // the alignment given.
  void * alloc_top(size_t size, size_t align) 
  {loop:
    top -= size;
    align_top(align);
    if (top < bottom) {new_chunk(); goto loop;}
    return top;
  }
  char * dup_top(ParmString str) {
    return (char *)memcpy(alloc_top(str.size() + 1), 
                          str.str(), str.size() + 1);
  }

  // By default objects are allocated from the top since that is sligtly
  // more efficient
  void * alloc(size_t size) {return alloc_top(size);}
  void * alloc(size_t size, size_t align) {return alloc_top(size,align);}
  char * dup(ParmString str) {return dup_top(str);}
};

typedef ObjStack StringBuffer;

}

#endif
