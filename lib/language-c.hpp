/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#ifndef ASPELL_LANGUAGE_C__HPP
#define ASPELL_LANGUAGE_C__HPP

#include "language.hpp"

namespace acommon {

class MunchWordList;
class ExpandWordList;

class Language : public CanHaveError 
{
public:
  const LangBase * real;
  Language (const LangBase * r) : real(r) {}

  String temp_str_0;
  String temp_str_1;
  ClonePtr<FullConvert> to_internal_;
  ClonePtr<FullConvert> from_internal_;
  
  MunchWordList * munch_wl;
  ExpandWordList * expand_wl;

  Language() : munch_wl(0), expand_wl(0) {}
  ~Language();
};

}

#endif

