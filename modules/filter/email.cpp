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

  class EmailFilter : public IndividualFilter 
  {
    bool prev_newline;
    bool in_quote;
    int margin;
    int n;

    class QuoteChars : public MutableContainer {
      bool data[256];
    public:
      bool add(ParmString s) {
        data[static_cast<unsigned char>(s[0])] = true;
        return true;
      }
      bool remove(ParmString s) {
        data[static_cast<unsigned char>(s[0])] = false;
        return true;
      }
      void clear() {
        memset(data, 0, sizeof(bool)*256);
      }
      bool have(char c) {
        return data[static_cast<unsigned char>(c)];
      }
      QuoteChars() {clear();}
    };
    QuoteChars is_quote_char;
    
  public:
    PosibErr<void> setup(Config *);
    void reset();
    void process(char *, unsigned int size);
  };

  PosibErr<void> EmailFilter::setup(Config * opts) 
  {
    name_ = "email";
    order_num_ = 0.85;
    opts->retrieve_list("email-quote", &is_quote_char);
    margin = opts->retrieve_int("email-margin");
    reset();
    return no_err;
  }
  
  void EmailFilter::reset() 
  {
    prev_newline = true;
    in_quote = false;
    n = 0;
  }

  void EmailFilter::process(char * str, unsigned int size)
  {
    char * end = str + size;
    char * line_begin = str;
    while (str < end) {
      if (prev_newline && is_quote_char.have(*str))
	in_quote = true;
      if (*str == '\n') {
	if (in_quote)
	  memset(line_begin, ' ', str - line_begin);
	line_begin = str;
	in_quote = false;
	prev_newline = true;
	n = 0;
      } else if (n < margin) {
	++n;
      } else {
	prev_newline = false;
      }
      ++str;
    }
    if (in_quote)
      memset(line_begin, ' ', str - line_begin);
  }
  
  IndividualFilter * new_email_filter() 
  {
    return new EmailFilter();
  }

  static const KeyInfo email_options[] = {
    {"email-quote", KeyInfoList, ">,|", "email quote characters"},
    {"email-margin", KeyInfoInt, "10",  "num chars that can appear before the quote char"}
  };

  const KeyInfo * email_options_begin = email_options;
  const KeyInfo * email_options_end   = email_options + 2;


}
