// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "asc_ctype.hpp"
#include "config.hpp"
#include "indiv_filter.hpp"
#include "string_map.hpp"
#include "mutable_container.hpp"
#include "copy_ptr-t.hpp"
#include "clone_ptr-t.hpp"

namespace acommon {

  class ToLowerMap : public MutableContainer
  {
    StringMap * real_;

  public:
    ToLowerMap(StringMap * r) : real_(r) {}

    PosibErr<bool> add(ParmString to_add) {
      String new_key;
      for (const char * i = to_add; *i; ++i) new_key += asc_tolower(*i);
      return real_->add(new_key);
    }

    PosibErr<bool> remove(ParmString to_rem) {
      String new_key;
      for (const char * i = to_rem; *i; ++i) new_key += asc_tolower(*i);
      return real_->remove(new_key);
    }

    PosibErr<void> clear() {
      return real_->clear();
    }
  };

  class SgmlFilter : public IndividualFilter 
  {
    bool in_markup;
    char in_quote;
    bool new_token;
    String tag_name;
    String parm_name;
    enum InWhat {InKey, InValue, InValueNoSkip, InOther};
    InWhat in_what;
    ClonePtr<StringMap> noskip_tags;

    inline bool process_char(FilterChar::Chr c);

  public:

    PosibErr<void> setup(Config *);
    void reset();
    void process(FilterChar * &, FilterChar * &);
  };

  PosibErr<void> SgmlFilter::setup(Config * opts) 
  {
    name_ = "sgml";
    order_num_ = 0.35;
    noskip_tags.reset(new_string_map());
    ToLowerMap tl_noskip_tags(noskip_tags);
    RET_ON_ERR(opts->retrieve_list("sgml-check", &tl_noskip_tags));
    reset();
    return no_err;
  }
  
  void SgmlFilter::reset() 
  {
    in_markup = false;
    in_quote = false;
    new_token = false;
    in_what = InOther;
  }

  // yes this should be inlines, it is only called once
  inline bool SgmlFilter::process_char(FilterChar::Chr c) {
    if (!in_quote)
      if (c == '<') {
	in_markup = true;
	in_what = InKey;
	new_token = true;
	tag_name = "";
	return true;
      } else if (c == '>') {
	in_markup = false;
	return true;
      }

    if (!in_markup)
      return false;
  
    if (c == '"' || c == '\'') {
	
      if (!in_quote)
	in_quote = c;
      else if (in_quote == c)
	in_quote = 0;
	
    } else if (!in_quote && isspace(c)) {
	
      if (!new_token) {
	in_what = InKey;
	new_token = true;
      }
	
    } else if (!in_quote && c == '=') {
	
      if (noskip_tags->have(parm_name.c_str()))
	in_what = InValueNoSkip;
      else
	in_what = InValue;
      new_token = true;
      return true;
	
    } else if (!in_quote && c == '/') {
	
      in_what = InOther;
	
    } else if (in_what == InKey) {
	
      if (new_token) {
	if (tag_name.empty()) tag_name = parm_name;
	parm_name = "";
	new_token = false;
      }
      parm_name += tolower(c);
	
    } else if (in_what == InValue || in_what == InValueNoSkip) {
	
      new_token = false;
	
    }
      
    return in_what != InValueNoSkip;
  }

  void SgmlFilter::process(FilterChar * & str, FilterChar * & stop)
  {
    FilterChar * cur = str;
    while (cur != stop) {
      if (process_char(*cur))
	*cur = ' ';
      ++cur;
    }
  }

  //
  //
  //
  
  IndividualFilter * new_sgml_filter() 
  {
    return new SgmlFilter();
  }
  
  static const KeyInfo sgml_options[] = {
    {"sgml-check", KeyInfoList, "alt", "sgml attributes to always check."},
    {"sgml-extension", KeyInfoList, "html,htm,php,sgml", "sgml file extensions"}
  };
  const KeyInfo * sgml_options_begin = sgml_options;
  const KeyInfo * sgml_options_end = sgml_options + 2;

}
