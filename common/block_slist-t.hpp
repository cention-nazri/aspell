#ifndef autil__block_slist_t_hh
#define autil__block_slist_t_hh

#include <cstdlib>
#include <cassert>

#include "block_slist.hpp"

namespace pcommon {

  template <typename T>
  void BlockSList<T>::add_block(unsigned int num) 
  {
    assert (reinterpret_cast<const char *>(&(first_available->next))
	    == 
	    reinterpret_cast<const char *>(first_available));
    const unsigned int ptr_offset = 
      reinterpret_cast<const char *>(&(first_available->data)) 
      - reinterpret_cast<const char *>(&(first_available->next));
    void * block = malloc( ptr_offset + sizeof(Node) * num );
    *reinterpret_cast<void **>(block) = first_block;
    first_block = block;
    Node * first = reinterpret_cast<Node *>(reinterpret_cast<char *>(block) + ptr_offset);
    Node * i = first;
    Node * last = i + num;
    while (i + 1 != last) {
      i->next = i + 1;
      i = i + 1;
    }
    i->next = 0;
    first_available = first;  
  }

  template <typename T>
  void BlockSList<T>::clear()
  {
    void * i = first_block;
    while (i != 0) {
      void * n = *reinterpret_cast<void **>(i);
      free(i);
      i = n;
    }
    first_block = 0;
    first_available = 0;
  }

}

#endif
