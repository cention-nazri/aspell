// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#include "word_list.hpp"
#include "emulation.hpp"
#include "parm_string.hpp"

using namespace pcommon;

namespace aspell {

  class ManagerImpl;

  class SuggestionList : public WordList {
  public:
    typedef StringEmulation            VirEmul;
    typedef Emulation<VirEmul>         Emul;
    typedef const char *               Value;
    typedef unsigned int               Size;

    virtual SuggestionList * clone() const = 0;
    virtual void assign(const SuggestionList *) = 0;
    
    virtual bool empty() const = 0;
    virtual Size size() const = 0;
    virtual VirEmul * elements() const = 0;
    virtual ~SuggestionList() {}
  };

  class Suggest {
  public:
    virtual void set_mode(ParmString) = 0;
    virtual double score(const char * base, const char * other) = 0;
    virtual SuggestionList & suggest(const char * word) = 0;
    virtual ~Suggest() {}
  };
  
  Suggest * new_default_suggest(ManagerImpl *);
}


