#ifndef FILTER_ENTRY_HEADER
#define FILTER_ENTRY_HEADER
#include "indiv_filter.hpp"

namespace acommon {
  struct FilterEntry
  {
    const char * name;
    IndividualFilter * (* decoder) ();
    IndividualFilter * (* filter) ();
    IndividualFilter * (* encoder) ();
  };
};
#endif
  
