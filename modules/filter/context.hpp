// This file is part of The New Aspell
// Copyright (C) 2002 by Christoph Hintermüller under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.
//
// Example for Filter loadable via Aspell's extended filter interface
// Added to Aspell by Christoph Hintermüller
// The main filter header file like this file has to contain the following
// parts:
// at least one function called new_<filtertype> where <filter_type> is one
// of the following: encoder
//                   decoder
//                   filter
// all these functions have to be exported as extern "C" in order to ease loading
// by dlopen(Unix/Linux) or the proper windows function
// Use the FILTER_API_EXPORTS macro defined by loadable-filter-API.hpp as shown in
// order to ease porting your filter to windows
// 
// Each filter has to derived from Aspell's IndividualFilter Class see Aspell's manual
// upon filter design
//  
// each filer class has to contain the the DESCRIPTION macro as this declares and
// sets the static char* name member of the filter which stores the name of the filter.
//
// enclose all the above as shown within the acommon namespace or set using to acommon

#define FILTER_PROGRESS_CONTROL ""
#include "loadable-filter-API.hpp"
#include "config.hpp"
#include "posib_err.hpp"
#include "can_have_error.hpp"
#include "indiv_filter.hpp"
#include "filter_char.hpp"

#include "string.hpp"
#include "vector.hpp"

#include "stdio.h"


namespace acommon{
using namespace std;

  typedef enum filterstate_{hidden=0, visible=1}filterstate;
  
  class FILTER_API_EXPORTS ContextFilter : public IndividualFilter {
    filterstate state;
    vector<String> opening;
    vector<String> closing;
    int correspond;
    String filterversion;
    FILE * debugoutput;
  
    PosibErr<bool> hidecode(FilterChar * begin,FilterChar * end);
  public:
    ContextFilter(void);
    virtual void reset(void);
    void process(FilterChar *& start,FilterChar *& stop);
    virtual PosibErr<bool> setup(Config * config);
    virtual ~ContextFilter();
  };
  
  ACTIVATE_FILTER(acommon,ContextFilter,context);
}
