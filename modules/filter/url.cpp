// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "indiv_filter.hpp"

namespace acommon {

  class UrlFilter : public IndividualFilter 
  {
  public:
    PosibErr<void> setup(Config *);
    void reset() {}
    void process(char *, unsigned int size);
  };

  PosibErr<void> UrlFilter::setup(Config *) 
  {
    name_ = "url";
    order_num_ = 0.95;
    return no_err;
  }

  void UrlFilter::process(char * str, unsigned int size)
  {
    enum {slash, who_cares} prev_char;
    enum {wbegin, wmiddle, wend} word_pos;
    bool blank_out;
    int point_chars;
    char * cur = str;
    char * end = str + size;
    while (cur < end) {
      prev_char = who_cares;
      word_pos = wbegin;
      blank_out = false;
      point_chars = 0;
      do {
	if (cur + 1 == end ||
	    *(cur + 1) == ' ' || *(cur + 1) == '\n' || *(cur + 1) == '\t')
	  word_pos = wend;
	if (word_pos == wmiddle) {
	  switch (*cur) {
	  case '@':
	    blank_out = true;
	    prev_char = who_cares;
	    break;
	  case '.':
	    ++point_chars;
	    prev_char = who_cares;
	    break;
	  case '/':
	    if (prev_char == slash) blank_out = true;
	    prev_char = slash;
	    break;
	  default:
	    prev_char = who_cares;
	  }
	}
	if (word_pos == wbegin) word_pos = wmiddle;
	++cur;
      } while (word_pos != wend);
      if (point_chars > 1) blank_out = true;
      if (blank_out)
	memset(str, ' ', cur - str);
      str = cur;
    }
  }

  IndividualFilter * new_url_filter() 
  {
    return new UrlFilter();
  }


}
