/* This file is part of The New Aspell
 * Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#ifndef ASPELL_STRING_LIST__HPP
#define ASPELL_STRING_LIST__HPP

#include "mutable_container.hpp"
#include "parm_string.hpp"
#include "posib_err.hpp"

namespace acommon {

class StringEnumeration;
class StringList;

class StringList : public MutableContainer {
 public:
  virtual bool empty() const = 0;
  virtual unsigned int size() const = 0;
  virtual StringEnumeration * elements() const = 0;
  virtual PosibErr<bool> add(ParmString to_add) = 0;
  virtual PosibErr<bool> remove(ParmString to_rem) = 0;
  virtual PosibErr<void> clear() = 0;
  virtual StringList * clone() const = 0;
  virtual void assign(const StringList * other) = 0;
  StringList() {}
  virtual ~StringList() {}
};
StringList * new_string_list();


}

#endif /* ASPELL_STRING_LIST__HPP */
