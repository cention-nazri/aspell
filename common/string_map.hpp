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

namespace acommon {

class StringMap;
class StringPairEnumeration;

class StringMap : public MutableContainer {
 public:
  virtual PosibErr<bool> add(ParmString to_add) = 0;
  virtual PosibErr<bool> remove(ParmString to_rem) = 0;
  virtual PosibErr<void> clear() = 0;
  virtual StringMap * clone() const = 0;
  virtual void assign(const StringMap * other) = 0;
  virtual bool empty() const = 0;
  virtual unsigned int size() const = 0;
  virtual StringPairEnumeration * elements() const = 0;
  virtual bool insert(ParmString key, ParmString value) = 0;
  virtual bool replace(ParmString key, ParmString value) = 0;
  virtual const char * lookup(ParmString key) const = 0;
  StringMap() {}
  virtual ~StringMap() {}
};
StringMap * new_string_map();


}

#endif /* ASPELL_STRING_MAP__HPP */
