/* This file is part of The New Aspell
 * Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#include "config.hpp"
#include "filter.hpp"
#include "speller.hpp"
#include "indiv_filter.hpp"

#include "iostream.hpp"

namespace acommon {

  PosibErr<void> Filter::setup(Speller * speller, Config * config) 
  {
    config_.reset(speller->config()->clone());
    config_->merge(*config_);
    return no_err;
  }

  void Filter::add_filter(IndividualFilter * filter)
  {
    filter->setup(config_);
    Filters::iterator cur, end;
    cur = filters_.begin();
    end = filters_.end();
    while (cur != end && filter->order_num() < (*cur)->order_num())
      ++cur;
    filters_.insert(cur, filter);
  }

  void Filter::reset()
  {
    Filters::iterator cur, end;
    cur = filters_.begin();
    end = filters_.end();
    for (; cur != end; ++cur)
      (*cur)->reset();
  }

  void Filter::process(char * str, unsigned int size)
  {
    Filters::iterator cur, end;
    cur = filters_.begin();
    end = filters_.end();
    COUT << "BEFORE: " << str;
    for (; cur != end; ++cur)
      (*cur)->process(str, size);
    COUT << "AFTER: " << str;
  }

  Filter::~Filter() 
  {
    Filters::iterator cur, end;
    cur = filters_.begin();
    end = filters_.end();
    for (; cur != end; ++cur)
      delete *cur;
  }

}

