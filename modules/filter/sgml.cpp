// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "config.hpp"
#include "indiv_filter.hpp"
#include "mutable_container.hpp"
#include "copy_ptr-t.hpp"

namespace acommon {

  // FIXME: Write me

  class SgmlFilter : public IndividualFilter 
  {
    
  public:
    PosibErr<void> setup(Config *);
    void reset();
    void process(char *, unsigned int size);
  };

  PosibErr<void> SgmlFilter::setup(Config * opts) 
  {
    return no_err;
  }
  
  void SgmlFilter::reset() 
  {
  }

  void SgmlFilter::process(char * str, unsigned int size)
  {
  }
  
  IndividualFilter * new_sgml_filter() 
  {
    return new SgmlFilter();
  }
  
  static const KeyInfo sgml_options[] = {
    {"sgml-check", KeyInfoList, "alt", "sgml tags to always check."},
    {"sgml-extension", KeyInfoList, "html,htm,php,sgml", "sgml file extensions"}
  };
  const KeyInfo * sgml_options_begin = sgml_options;
  const KeyInfo * sgml_options_end = sgml_options + 2;

}
