/* This file is part of The New Aspell
 * Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#ifndef ASPELL_STRING_MAP__HPP
#define ASPELL_STRING_MAP__HPP

#include "mutable_container.hpp"
#include "parm_string.hpp"
#include "posib_err.hpp"
#include "string_pair.hpp"

namespace acommon {

class StringPairEnumeration;
  
class StringMapNode {
  // private data structure
public:
  StringPair      data;
  StringMapNode * next;
  StringMapNode() : next(0) {}
  StringMapNode(const StringMapNode &);
  ~StringMapNode();
private:
  StringMapNode & operator=(const StringMapNode &);
};
  
typedef StringMapNode * StringMapNodePtr;

class StringMap : public MutableContainer {
  // copy and destructor provided
public:
  StringMap();
  StringMap(const StringMap &);
  StringMap & operator= (const StringMap &);
  ~StringMap();
  
  StringMap * clone() const {
    return new StringMap(*this);
  }
  void assign(const StringMap * other) {
    *this = *(const StringMap *)(other);
  }
  
  StringPairEnumeration * elements() const;
  
  // insert a new element.   Will NOT overright an existing entry.
  // returns false if the element already exists.
  bool insert(ParmString key, ParmString value) {
    return insert(key, value, false);
  }
  PosibErr<bool> add(ParmString key) {
    return insert(key, 0, false);
  }
  // insert a new element. WILL overight an exitsing entry
  // always returns true
  bool replace(ParmString key, ParmString value) {
    return insert(key, value, true);
  }
  
  // removes an element.  Returnes true if the element existed.
  PosibErr<bool> remove(ParmString key) ;
  
  PosibErr<void> clear();
  
  // looks up an element.  Returns null if the element did not exist.
  // returns an empty string if the element exists but has a null value
  // otherwise returns the value
  const char * lookup(ParmString key) const;
  
  bool have(ParmString key) const {return lookup(key) != 0;}
  
  unsigned int size() const {return size_;}
  bool empty() const {return size_ == 0;}

private:
  void resize(const unsigned int *);
  
  // inserts an element the last paramerts conters if an
  // existing element will be overwritten.
  bool insert(ParmString key, ParmString value, bool);
  
  // clears the hash table, does NOT delete the old one
  void clear_table(const unsigned int * size);
  
  void copy(const StringMap &);
  
  // destroys the hash table, assumes it exists
  void destroy();
  
  StringMapNode * * find(ParmString);
  unsigned int size_;
  StringMapNodePtr * data;
  const unsigned int * buckets;
};

StringMap * new_string_map();


}

#endif /* ASPELL_STRING_MAP__HPP */
