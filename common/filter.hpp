/* This file is part of The New Aspell
 * Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#ifndef ASPELL_FILTER__HPP
#define ASPELL_FILTER__HPP

#include "can_have_error.hpp"
#include "copy_ptr.hpp"
#include "filter_char.hpp"
#include "posib_err.hpp"
#include "vector.hpp"
#include <stdio.h>
#include <cstdio>

namespace acommon {

  class Config;
  class Speller;
  class IndividualFilter;
  class StringList;
  class Notifier;
  struct ConfigModule;

  class Filter : public CanHaveError {
  public:
    
    enum FilterType {
      DECODER=-1,
      FILTER=0,
      ENCODER=1
    };

    bool empty() const { return (filters_.empty() && 
                                decoders_.empty() && 
                                encoders_.empty());} 
    void clear();
    void reset();
    void decode(FilterChar * & start, FilterChar * & stop);
    void process(FilterChar * & start, FilterChar * & stop);
    void encode(FilterChar * & start, FilterChar * & stop);
    void add_filter(IndividualFilter * filter,void* handles=NULL,
                    int type=FILTER);
    // setup the filter where the string list is the list of 
    // filters to use.
    Filter();
    ~Filter();
  private:
    typedef Vector<IndividualFilter *> Filters;
    Filters decoders_;
    Filters filters_;
    Filters encoders_;
    typedef Vector<void*> Handled;
    Handled handledDecoder;
    Handled handledFilter;
    Handled handledEncoder;
  };

  void set_mode_from_extension(Config * config,
			       ParmString filename);
  
  PosibErr<void> setup_filter(Filter &, Config *, 
			      bool use_decoder, 
			      bool use_filter, 
			      bool use_encoder);
  void activate_dynamic_filteroptions(Config *c);

}

#endif /* ASPELL_FILTER__HPP */
