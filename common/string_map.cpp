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

  class StringMapImplNode {
    // private data structure
  public:
    StringPair      data;
    StringMapImplNode * next;
    StringMapImplNode() : next(0) {}
    StringMapImplNode(const StringMapImplNode &);
    ~StringMapImplNode();
  private:
    StringMapImplNode & operator=(const StringMapImplNode &);
  };

  typedef StringMapImplNode * StringMapImplNodePtr;

  class StringMapImpl : public StringMap {
    // copy and destructor provided
  public:
    StringMapImpl();
    StringMapImpl(const StringMapImpl &);
    StringMapImpl & operator= (const StringMapImpl &);
    ~StringMapImpl();

    StringMap * clone() const {
      return new StringMapImpl(*this);
    }
    void assign(const StringMap * other) {
      *this = *(const StringMapImpl *)(other);
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

    void copy(const StringMapImpl &);

    // destroys the hash table, assumes it exists
    void destroy();

    StringMapImplNode * * find(ParmString);
    unsigned int size_;
    StringMapImplNodePtr * data;
    const unsigned int * buckets;
  };

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

  StringMapImplNode * * StringMapImpl::find(ParmString key) {
    StringMapImplNode * * i = &data[hash_string(key) % *buckets];
    while (*i != 0 && strcmp((*i)->data.first, key) != 0)
      i = &(*i)->next;
    return i;
  }

  const char * StringMapImpl::lookup(ParmString key) const 
  {
    const StringMapImplNode * i 
      = *((StringMapImpl *)this)->find(key);
    if (i == 0) {
      return 0;
    } else {
      if (i->data.second == 0) return "";
      else return (i->data.second);
    }
  }

  bool StringMapImpl::insert(ParmString key, ParmString val, 
				   bool replace) 
  {
    StringMapImplNode * * i = find(key);
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

	*i = new StringMapImplNode();
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

  PosibErr<bool> StringMapImpl::remove(ParmString key) {
    StringMapImplNode * * i = find(key);
    if (*i == 0) {
      return false;
    } else {
      --size_;
      StringMapImplNode * temp = *i;
      *i = (*i)->next;
      delete temp;
      return true;
    }
  }

  void StringMapImpl::resize(const unsigned int * new_buckets) {
    assert (*new_buckets != 0);
    StringMapImplNode * * old_data = data;
    unsigned int old_buckets = *buckets;
    clear_table(new_buckets);
    unsigned int i = 0;
    for(;i != old_buckets; ++i) {
      StringMapImplNode * j = old_data[i];
      while (j != 0) {
	StringMapImplNode * * k = find(j->data.first);
	*k = j;
	j = j->next;
	(*k)->next = 0;
      }
    }
    delete[] old_data;
  }

  StringMapImpl::StringMapImpl() {
    clear_table(primes);
    size_ = 0;
  }

  PosibErr<void> StringMapImpl::clear() {
    destroy();
    clear_table(primes);
    size_ = 0;
    return no_err;
  }

  StringMapImpl::StringMapImpl(const StringMapImpl & other) {
    copy(other);
  }

  StringMapImpl & 
  StringMapImpl::operator= (const StringMapImpl & other) 
  {
    destroy();
    copy(other);
    return *this;
  }

  StringMapImpl::~StringMapImpl() {
    destroy();
  }

  void StringMapImpl::clear_table(const unsigned int * size) {
    buckets = size;
    data = new StringMapImplNodePtr[*buckets];
    memset(data, 0, sizeof(StringMapImplNodePtr) * (*buckets));
  }

  void StringMapImpl::copy(const StringMapImpl & other) {
    clear_table(other.buckets);
    size_ = other.size_;
    unsigned int i = 0;
    for (; i != *buckets; ++i) {
      StringMapImplNode * * j0 = &other.data[i];
      StringMapImplNode * * j1 = &data[i];
      while(*j0 != 0) {
	*j1 = new StringMapImplNode(**j0);
	j0 = &(*j0)->next;
	j1 = &(*j1)->next;
      }
      *j1 = 0;
    }
  }

  void StringMapImpl::destroy() {
    unsigned int i = 0;
    for (; i != *buckets; ++i) {
      StringMapImplNode * j = data[i];
      while(j != 0) {
	StringMapImplNode * k = j;
	j = j->next;
	delete k;
      }
    }
    delete[] data;
    data = 0;
  }

  //
  // StringMapImplNode methods
  //

  StringMapImplNode::StringMapImplNode
  (const StringMapImplNode & other) 
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

  StringMapImplNode::~StringMapImplNode() {
    delete[] (char *)(data.first);
    if (data.second != 0) delete[] (char *)(data.second);
  }


  //
  //
  //

  class StringMapImplEnumeration : public StringPairEnumeration {
    unsigned int i;
    const StringMapImplNode    * j;
    const StringMapImplNodePtr * data;
    unsigned int size;
  public:
    StringMapImplEnumeration(const StringMapImplNodePtr * d, 
				 unsigned int s);    
    StringPairEnumeration * clone() const;
    void assign(const StringPairEnumeration *);
    bool at_end() const;
    StringPair next();
  };

  StringMapImplEnumeration
  ::StringMapImplEnumeration(const StringMapImplNodePtr * d, 
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

  StringPairEnumeration * StringMapImplEnumeration::clone() const {
    return new StringMapImplEnumeration(*this);
  }

  void 
  StringMapImplEnumeration::assign
  (const StringPairEnumeration * other)
  {
    *this = *(const StringMapImplEnumeration *)(other);
  }

  bool StringMapImplEnumeration::at_end() const {
    return i == size;
  }

  StringPair StringMapImplEnumeration::next() {
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

  StringPairEnumeration * StringMapImpl::elements() const {
    return new StringMapImplEnumeration(data, *buckets);
  }

  StringMap * new_string_map() 
  {
    return new StringMapImpl();
  }
}
