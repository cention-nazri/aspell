// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include <stdio.h>

#include "asc_ctype.hpp"
#include "config.hpp"
#include "indiv_filter.hpp"
#include "string_map.hpp"
#include "mutable_container.hpp"
#include "copy_ptr-t.hpp"
#include "clone_ptr-t.hpp"
#include "filter_char_vector.hpp"
#include "loadable-filter-API.hpp"

//right now unused option
//  static const KeyInfo sgml_options[] = {
//    {"sgml-extension", KeyInfoList, "html,htm,php,sgml",
//     N_("sgml file extensions")}
//  };

namespace acommon {

  class ToLowerMap : public StringMap
  {
  public:
    PosibErr<bool> add(ParmString to_add) {
      String new_key;
      for (const char * i = to_add; *i; ++i) new_key += asc_tolower(*i);
      return StringMap::add(new_key);
    }

    PosibErr<bool> remove(ParmString to_rem) {
      String new_key;
      for (const char * i = to_rem; *i; ++i) new_key += asc_tolower(*i);
      return StringMap::remove(new_key);
    }
  };

  class SgmlFilter : public IndividualFilter 
  {
    bool in_markup;
    FilterChar::Chr in_quote;
    bool new_token;
    String tag_name;
    String parm_name;
    enum InWhat {InKey, InValue, InValueNoSkip, InOther};
    InWhat in_what;
    StringMap noskip_tags;

    inline bool process_char(FilterChar::Chr c);
 
    public:

    PosibErr<bool> setup(Config *);
    void reset();
    void process(FilterChar * &, FilterChar * &);
  };

  PosibErr<bool> SgmlFilter::setup(Config * opts) 
  {
    name_ = "sgml-filter";
    order_num_ = 0.35;
    noskip_tags.clear();
    opts->retrieve_list("filter-sgml-check", &noskip_tags);
    reset();
    return true;
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

      new_token = false;
      if (!in_quote)
	in_quote = c;
      else if (in_quote == c)
	in_quote = 0;
	
    } else if (!in_quote && asc_isspace(c)) {
	
      if (!new_token) {
	in_what = InKey;
	new_token = true;
      }
	
    } else if (!in_quote && c == '=') {
	
      if (noskip_tags.have(parm_name.c_str()))
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
      parm_name += asc_tolower(c);
	
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

  class SgmlDecoder : public IndividualFilter 
  {
    FilterCharVector buf;
  public:
    PosibErr<bool> setup(Config *);
    void reset() {}
    void process(FilterChar * &, FilterChar * &);
  };

  PosibErr<bool> SgmlDecoder::setup(Config *) 
  {
    name_ = "sgml-decoder";
    order_num_ = 0.65;
    return true;
  }

  void SgmlDecoder::process(FilterChar * & start, FilterChar * & stop)
  {
    buf.clear();
    FilterChar * i = start;
    while (i != stop)
    {
      if (*i == '&') {
	FilterChar * i0 = i;
	FilterChar::Chr chr;
	++i;
	if (*i == '#') {
	  chr = 0;
	  ++i;
	  while (asc_isdigit(*i)) {
	    chr *= 10;
	    chr += *i - '0';
	    ++i;
	  }
	} else {
	  while (asc_isalpha(*i) || asc_isdigit(*i)) {
	    ++i;
	  }
	  chr = '?';
	}
	if (*i == ';')
	  ++i;
	buf.append(FilterChar(chr, i0, i));
      } else {
	buf.append(*i);
	++i;
      }
    }
    buf.append('\0');
    start = buf.pbegin();
    stop  = buf.pend() - 1;
  }

  //
  //
  //

  class SgmlEncoder : public IndividualFilter 
  {
    FilterCharVector buf;
  public:
    PosibErr<bool> setup(Config *);
    void reset() {}
    void process(FilterChar * &, FilterChar * &);
  };

  PosibErr<bool> SgmlEncoder::setup(Config *) 
  {
    name_ = "sgml-encoder";
    order_num_ = 0.99;
    return true;
  }

  void SgmlEncoder::process(FilterChar * & start, FilterChar * & stop)
  {
    buf.clear();
    FilterChar * i = start;
    while (i != stop)
    {
      if (*i > 127) {
	buf.append("&#", i->width);
	char b[10];
	sprintf(b, "%d", i->chr);
	buf.append(b, 0);
	buf.append(';', 0);
      } else {
	buf.append(*i);
      }
      ++i;
    }
    buf.append('\0');
    start = buf.pbegin();
    stop  = buf.pend() - 1;
  }

  ACTIVATE_FILTER(acommon,SgmlFilter,sgml);
  ACTIVATE_DECODER(acommon,SgmlDecoder,sgml);
  //ACTIVATE_ENCODER(acommon,SgmlEncoder,sgml);
}
