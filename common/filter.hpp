/* This file is part of The New Aspell
 * Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#ifndef ASPELL_FILTER__HPP
#define ASPELL_FILTER__HPP

#include "posib_err.hpp"
#include "copy_ptr.hpp"
#include "vector.hpp"
#include "can_have_error.hpp"

namespace acommon {

  class Config;
  class Speller;
  class IndividualFilter;

  class Filter : public CanHaveError {
  public:
    PosibErr<void> setup(Speller * speller, Config * config);
    void reset();
    void process(char * str, unsigned int size);
    void add_filter(IndividualFilter * filter);
    Config * config() {return config_;} 
    Filter();
    ~Filter();
  private:
    CopyPtr<Config> config_;
    typedef Vector<IndividualFilter *> Filters;
    Filters filters_;
  };
  
  PosibErr<Filter *> new_filter(Speller *, Config *);

}

#endif /* ASPELL_FILTER__HPP */
