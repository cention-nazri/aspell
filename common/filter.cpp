/* This file is part of The New Aspell
 * Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#include "config.hpp"
#include "filter.hpp"
#include "speller.hpp"
#include "indiv_filter.hpp"
#include "copy_ptr-t.hpp"

namespace acommon {

  Filter::Filter() {}

  void Filter::add_filter(IndividualFilter * filter)
  {
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

  void Filter::process(FilterChar * & start, FilterChar * & stop)
  {
    Filters::iterator cur, end;
    cur = filters_.begin();
    end = filters_.end();
    for (; cur != end; ++cur)
      (*cur)->process(start, stop);
  }

  void Filter::clear()
  {
    Filters::iterator cur, end;
    cur = filters_.begin();
    end = filters_.end();
    for (; cur != end; ++cur)
      delete *cur;
    filters_.clear();
  }

  Filter::~Filter() 
  {
    clear();
  }

}

