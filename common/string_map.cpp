#include <string.h>
#include <assert.h>

#include "parm_string.hpp"
#include "string_map.hpp"
#include "string_pair.hpp"
#include "string_pair_enumeration.hpp"

// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

// prime list and hash_string taken from SGI STL with the following 
// copyright:

/*
 * Copyright (c) 1996-1998
 * Silicon Graphics Computer Systems, Inc.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Silicon Graphics makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 *
 * Copyright (c) 1994
 * Hewlett-Packard Company
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Hewlett-Packard Company makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 */


namespace acommon {

  static const unsigned int primes[] =
    {
      53,         97,         193,       389,       769,
      1543,       3079,       6151,      12289,     24593,
      49157,      98317,      196613,    393241,    786433,
      1572869,    3145739,    6291469,   12582917,  25165843,
      50331653,   100663319,  201326611, 402653189, 805306457, 
      0
    };

  static unsigned int hash_string(const char * s) {
    unsigned int h = 0; 
    for ( ; *s; ++s)
      h = 5*h + *s;
    return h;
  }

  StringMapNode * * StringMap::find(ParmString key) {
    StringMapNode * * i = &data[hash_string(key) % *buckets];
    while (*i != 0 && strcmp((*i)->data.first, key) != 0)
      i = &(*i)->next;
    return i;
  }

  const char * StringMap::lookup(ParmString key) const 
  {
    const StringMapNode * i 
      = *((StringMap *)this)->find(key);
    if (i == 0) {
      return 0;
    } else {
      if (i->data.second == 0) return "";
      else return (i->data.second);
    }
  }

  bool StringMap::insert(ParmString key, ParmString val, 
				   bool replace) 
  {
    StringMapNode * * i = find(key);
    char * temp;
    if (*i != 0) {

      if (replace) {

	if (val == 0 || *val == '\0') {
	  temp = 0;
	} else {
	  temp = new char[strlen(val) + 1];
	  strcpy(temp, val);
	}
	if ((*i)->data.second != 0) delete[] (char *)((*i)->data.second);
	(*i)->data.second = temp;
	return true;

      } else {
	return false;

      }

    } else {

      ++size_;
      if (size_ > *buckets) {

	resize(buckets+1);
	return insert(key, val, replace);

      } else {

	*i = new StringMapNode();
	char * temp = new char[strlen(key) + 1];
	strcpy(temp, key);
	(*i)->data.first = temp;
	if (val.empty()) {
	  temp = 0;
	} else {
	  temp = new char[strlen(val) + 1];
	  strcpy(temp, val);
	}
	(*i)->data.second = temp;
	return true;

      }
    }
  }

  PosibErr<bool> StringMap::remove(ParmString key) {
    StringMapNode * * i = find(key);
    if (*i == 0) {
      return false;
    } else {
      --size_;
      StringMapNode * temp = *i;
      *i = (*i)->next;
      delete temp;
      return true;
    }
  }

  void StringMap::resize(const unsigned int * new_buckets) {
    assert (*new_buckets != 0);
    StringMapNode * * old_data = data;
    unsigned int old_buckets = *buckets;
    clear_table(new_buckets);
    unsigned int i = 0;
    for(;i != old_buckets; ++i) {
      StringMapNode * j = old_data[i];
      while (j != 0) {
	StringMapNode * * k = find(j->data.first);
	*k = j;
	j = j->next;
	(*k)->next = 0;
      }
    }
    delete[] old_data;
  }

  StringMap::StringMap() {
    clear_table(primes);
    size_ = 0;
  }

  PosibErr<void> StringMap::clear() {
    destroy();
    clear_table(primes);
    size_ = 0;
    return no_err;
  }

  StringMap::StringMap(const StringMap & other) {
    copy(other);
  }

  StringMap & 
  StringMap::operator= (const StringMap & other) 
  {
    destroy();
    copy(other);
    return *this;
  }

  StringMap::~StringMap() {
    destroy();
  }

  void StringMap::clear_table(const unsigned int * size) {
    buckets = size;
    data = new StringMapNodePtr[*buckets];
    memset(data, 0, sizeof(StringMapNodePtr) * (*buckets));
  }

  void StringMap::copy(const StringMap & other) {
    clear_table(other.buckets);
    size_ = other.size_;
    unsigned int i = 0;
    for (; i != *buckets; ++i) {
      StringMapNode * * j0 = &other.data[i];
      StringMapNode * * j1 = &data[i];
      while(*j0 != 0) {
	*j1 = new StringMapNode(**j0);
	j0 = &(*j0)->next;
	j1 = &(*j1)->next;
      }
      *j1 = 0;
    }
  }

  void StringMap::destroy() {
    unsigned int i = 0;
    for (; i != *buckets; ++i) {
      StringMapNode * j = data[i];
      while(j != 0) {
	StringMapNode * k = j;
	j = j->next;
	delete k;
      }
    }
    delete[] data;
    data = 0;
  }

  //
  // StringMapNode methods
  //

  StringMapNode::StringMapNode
  (const StringMapNode & other) 
  {
    data.first = new char[strlen(other.data.first) + 1];
    strcpy((char *)data.first, other.data.first);
    if (other.data.second == 0) {
      data.second = 0;
    } else {
      data.second = new char[strlen(other.data.second) + 1];
      strcpy((char *)data.second, other.data.second);
    }
  }

  StringMapNode::~StringMapNode() {
    delete[] (char *)(data.first);
    if (data.second != 0) delete[] (char *)(data.second);
  }


  //
  //
  //

  class StringMapEnumeration : public StringPairEnumeration {
    unsigned int i;
    const StringMapNode    * j;
    const StringMapNodePtr * data;
    unsigned int size;
  public:
    StringMapEnumeration(const StringMapNodePtr * d, 
			 unsigned int s);    
    StringPairEnumeration * clone() const;
    void assign(const StringPairEnumeration *);
    bool at_end() const;
    StringPair next();
  };

  StringMapEnumeration
  ::StringMapEnumeration(const StringMapNodePtr * d, 
				 unsigned int s) 
  {
    data = d;
    size = s;
    i = 0;
    while (i != size && data[i] == 0)
      ++i;
    if (i != size)
      j = data[i];
  }

  StringPairEnumeration * StringMapEnumeration::clone() const {
    return new StringMapEnumeration(*this);
  }

  void 
  StringMapEnumeration::assign
  (const StringPairEnumeration * other)
  {
    *this = *(const StringMapEnumeration *)(other);
  }

  bool StringMapEnumeration::at_end() const {
    return i == size;
  }

  StringPair StringMapEnumeration::next() {
    StringPair temp;
    if (i == size)
      return temp;
    temp = j->data;
    j = j->next;
    if (j == 0) {
      do ++i;
      while (i != size && data[i] == 0);
      if (i != size)
	j = data[i];
    }
    return temp;
  }

  StringPairEnumeration * StringMap::elements() const {
    return new StringMapEnumeration(data, *buckets);
  }

  StringMap * new_string_map() 
  {
    return new StringMap();
  }
}
