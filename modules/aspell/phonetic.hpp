// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#ifndef __aspell_phonetic__
#define __aspell_phonetic__

#include "string.hpp"

using namespace pcommon;

namespace aspell {

  class Language;
 
  class Soundslike {
  public:
    virtual Soundslike * clone() const = 0;
    virtual void assign(const Soundslike *) = 0;
    virtual String soundslike_chars() const = 0;
    virtual String to_soundslike(ParmString) const = 0;
    virtual const char * name() const = 0;
    virtual const char * version() const = 0;
    virtual ~Soundslike() {}
  };

  Soundslike * new_soundslike(ParmString name,
			      ParmString dir1,
			      ParmString dir2,
			      const Language * lang);
};

#endif
