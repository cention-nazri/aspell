// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ACOMMON_FILTER__HPP
#define ACOMMON_FILTER__HPP

#include "string.hpp"
#include "posib_err.hpp"
#include "filter_char.hpp"

namespace acommon {

  class Config;

  class IndividualFilter {
  public:

    // sets up the filter 
    //
    // any options effecting this filter should start with the filter
    // name followed by a dash
    virtual PosibErr<void> setup(Config *) = 0;

    // reset the internal state of the filter
    //
    // should be called whenever a new document is being filtered
    virtual void reset() = 0;

    // process the string
    //
    // expected to modify the string by blanking out parts 
    // of the string that are to be skipped with spaces (' ', ASCII 32)
    // modifying the string in any way could lead to undefined results
    //
    // The string passed in should only be split on white space
    // characters.  Furthermore, between calles to reset, each string
    // should be passed in exactly once and in the order they appeared
    // in the document.  Passing in stings out of order, skipping
    // strings or passing them in more than once may lead to undefined
    // results.
    virtual void process(FilterChar * start, FilterChar * stop) = 0;

    virtual ~IndividualFilter() {}

    const char * name() const {return name_;}
    double order_num() const {return order_num_;}

  protected:

    IndividualFilter() : name_(0), order_num_(0.50) {}
    
    const char * name_; // must consist of 'a-z|-|0-9'
    double order_num_; // between 0 and 1 exclusive
  };

}

#endif
