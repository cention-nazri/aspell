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

  class TexFilter : public IndividualFilter 
  {
    
  public:
    PosibErr<void> setup(Config *);
    void reset();
    void process(char *, unsigned int size);
  };

  PosibErr<void> TexFilter::setup(Config * opts) 
  {
    return no_err;
  }
  
  void TexFilter::reset() 
  {
  }

  void TexFilter::process(char * str, unsigned int size)
  {
  }
  
  IndividualFilter * new_tex_filter() 
  {
    return new TexFilter();
  }

  static const KeyInfo tex_options[] = {
    {"tex-command", KeyInfoList, 
       // counters
       "addtocounter pp,"
       "addtolength pp,"
       "alpha p,"
       "arabic p,"
       "fnsymbol p,"
       "roman p,"
       "stepcounter p,"
       "setcounter pp,"
       "usecounter p,"
       "value p,"
       "newcounter po,"
       "refstepcounter p,"
       // cross ref
       "label p,"
       "pageref p,"
       "ref p,"
       // Definitions
       "newcommand poOP,"
       "renewcommand poOP,"
       "newenvironment poOPP,"
       "renewenvironment poOPP,"
       "newtheorem poPo,"
       "newfont pp,"
       // Document Classes
       "documentclass op,"
       "usepackage op,"
       // Environments
       "begin po,"
       "end p,"
       // Lengths
       "setlength pp,"
       "addtolength pp,"
       "settowidth pp,"
       "settodepth pp,"
       "settoheight pp,"
       // Line & Page Breaking
       "enlargethispage p,"
       "hyphenation p,"
       // Page Styles
       "pagenumbering p,"
       "pagestyle p,"
       // Spaces & Boxes
       "addvspace p,"
       "framebox ooP,"
       "hspace p,"
       "vspace p,"
       "makebox ooP,"
       "parbox ooopP,"
       "raisebox pooP,"
       "rule opp,"
       "sbox pO,"
       "savebox pooP,"
       "usebox p,"
       // Splitting the Input
       "include p,"
       "includeonly p,"
       "input p,"
       // Table of Contents
       "addcontentsline ppP,"
       "addtocontents pP,"
       // Typefaces
       "fontencoding p,"
       "fontfamily p,"
       "fontseries p,"
       "fontshape p,"
       "fontsize pp,"
       "usefont pppp,"
       // Misc
       "documentstyle op,"
       "cite p,"
       "nocite p,"
       "psfig p,"
       "selectlanguage p,"
       "includegraphics op,"
       "bibitem op,"
       // Geometry Package
       "geometry p,"
       ,"TeX commands"},
      {"tex-check-comments", KeyInfoBool, "false",
	 "check TeX comments"}
  };
  const KeyInfo * tex_options_begin = tex_options;
  const KeyInfo * tex_options_end = tex_options + 2;
}
