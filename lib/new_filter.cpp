// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.

#include "asc_ctype.hpp"
#include "config.hpp"
#include "enumeration.hpp"
#include "filter.hpp"
#include "indiv_filter.hpp"
#include "itemize.hpp"
#include "parm_string.hpp"
#include "stack_ptr.hpp"
#include "string_enumeration.hpp"
#include "string_list.hpp"
#include "string_map.hpp"

#include "iostream.hpp"

namespace acommon {

  //
  // filter modes
  // 

  const char * filter_modes = "none,url,email,sgml,tex";
  
  static const KeyInfo modes_module[] = {
    {"fm-email", KeyInfoList  , "url,email",0}
    , {"fm-none",  KeyInfoList  , "",         0}
    , {"fm-sgml",  KeyInfoList  , "url,sgml", 0}
    , {"fm-tex",   KeyInfoList  , "url,tex",  0}
    , {"fm-url",   KeyInfoList  , "url",      0}
  };

  class IndividualFilter;

  //
  // filter constructors
  //

  struct FilterEntry {
    const char * name;
    IndividualFilter * (* decoder) ();
    IndividualFilter * (* filter) ();
    IndividualFilter * (* encoder) ();
  };
  
  IndividualFilter * new_url_filter ();
  IndividualFilter * new_email_filter ();
  IndividualFilter * new_tex_filter ();
  IndividualFilter * new_sgml_decoder ();
  IndividualFilter * new_sgml_filter ();
  IndividualFilter * new_sgml_encoder ();

  static FilterEntry standard_filters[] = {
    {"url",   0, new_url_filter, 0},
    {"email", 0, new_email_filter, 0},
    {"tex", 0, new_tex_filter, 0},
    {"sgml", new_sgml_decoder, new_sgml_filter, new_sgml_encoder}
  };
  static unsigned int standard_filters_size 
  = sizeof(standard_filters)/sizeof(FilterEntry);

  //
  // config options for the filters
  //
  
  extern const KeyInfo * email_options_begin;
  extern const KeyInfo * email_options_end;

  extern const KeyInfo * tex_options_begin;
  extern const KeyInfo * tex_options_end;

  extern const KeyInfo * sgml_options_begin;
  extern const KeyInfo * sgml_options_end;

  static ConfigModule filter_modules[] =  {
    {"fm", modes_module, modes_module + sizeof(modes_module)/sizeof(KeyInfo)},
    {"email", email_options_begin, email_options_end},
    {"tex",   tex_options_begin,   tex_options_end},
    {"sgml",  sgml_options_begin, sgml_options_end}
  };

  // these variables are used in the new_config function and
  // thus need external linkage
  const ConfigModule * filter_modules_begin = filter_modules;
  const ConfigModule * filter_modules_end   
  = filter_modules + sizeof(filter_modules)/sizeof(ConfigModule);

  //
  // actual code
  //

  FilterEntry * find_individual_filter(ParmString);

  class ExtsMap : public StringMap 
  {
    const char * cur_mode;
  public:
    void set_mode(ParmString mode) {cur_mode = mode;}
    PosibErr<bool> add(ParmString key) {insert(key, cur_mode); return true;}
  };

  void set_mode_from_extension(Config * config,
			       ParmString filename)
  {

    // Initialize exts mapping
    StringList modes;
    itemize(filter_modes, modes);
    StringListEnumeration els = modes.elements_obj();
    const char * mode;
    ExtsMap exts;
    while ( (mode = els.next()) != 0) {
      exts.set_mode(mode);
      String to_find = mode;
      to_find += "-extension";
      PosibErr<void> err = config->retrieve_list(to_find, &exts);
      err.ignore_err();
    }
    const char * ext0 = strrchr(filename, '.');
    if (ext0 == 0) ext0 = filename;
    else ++ext0;
    String ext = ext0;
    for (unsigned int i = 0; i != ext.size(); ++i)
      ext[i] = asc_tolower(ext[i]);
    mode = exts.lookup(ext);
    if (mode != 0)
      config->replace("mode", mode);
  }

  PosibErr<void> setup_filter(Filter & filter, 
			      Config * config, 
			      bool use_decoder, 
			      bool use_filter, 
			      bool use_encoder)
  {
    StringList sl;
    config->retrieve_list("filter", &sl);
    StringListEnumeration els = sl.elements_obj();
    StackPtr<IndividualFilter> ifilter;
    const char * filter_name;
    while ( (filter_name = els.next()) != 0) 
    {
      FilterEntry * f = find_individual_filter(filter_name);
      assert(f); //FIXME: Return Error Condition

      if (use_decoder && f->decoder && (ifilter = f->decoder())) {
	RET_ON_ERR_SET(ifilter->setup(config), bool, keep);
	if (!keep)
	  ifilter.release();
	else
	  filter.add_filter(ifilter.release());
      } 
      if (use_filter && f->filter && (ifilter = f->filter())) {
	RET_ON_ERR_SET(ifilter->setup(config), bool, keep);
	if (!keep)
	  ifilter.release();
	else
	  filter.add_filter(ifilter.release());
      }
      if (use_encoder && f->encoder && (ifilter = f->encoder())) {
	RET_ON_ERR_SET(ifilter->setup(config), bool, keep);
	if (!keep)
	  ifilter.release();
	else
	  filter.add_filter(ifilter.release());
      }
    }
    return no_err;
  }

  FilterEntry * find_individual_filter(ParmString filter_name)
  {
    unsigned int i = 0;
    while (i != standard_filters_size) {
      if (standard_filters[i].name == filter_name)
	return standard_filters + i;
      ++i;
    }
    return 0;
  }

}
