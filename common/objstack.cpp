
#include "objstack.hpp"

namespace acommon {

using namespace std;

ObjStack::ObjStack(size_t chunk_s, size_t align)
  : chunk_size(chunk_s), min_align(align)
{
  first_free = first = (Node *)malloc(chunk_size);
  first->next = 0;
  bottom = first->data;
  top    = (byte *)first + chunk_size;
}

ObjStack::~ObjStack()
{
  while (first) {
    Node * tmp = first->next;
    free(first);
    first = tmp;
  }
}

void ObjStack::new_chunk()
{
  first_free->next = (Node *)malloc(chunk_size);
  first_free = first_free->next;
  first_free->next = 0;
  bottom = first_free->data;
  align_bottom(min_align);
  top    = (byte *)first_free + chunk_size;
  align_top(min_align);
}

}
