// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include <string.h>

#include "istream.hpp"
#include "getdata.hpp"
#include "asc_ctype.hpp"

namespace acommon {

  bool getdata_pair(IStream & in, DataPair & d,
                    char * buf, size_t len)
  {
    buf[0] = '\0'; // to avoid some special cases
    char * p;
    char * end;

    // get first non blank line
    do {
        p = buf + 1;
        end = in.getline(p, len-1);
        if (end == 0) return false;
        while (*p == ' ' || *p == '\t') ++p;
    } while (*p == '#' || *p == '\0');

    // get key
    d.key.str_ = p;
    while (*p != '\0' &&
           ((*p != ' ' && *p != '\t' && *p != '#') || *(p-1) == '\\')) ++p;
    d.key.size_ = p - d.key.str_;

    // figure out if there is a value and add terminate key
    d.value.str_ = p; // in case there is no value
    d.value.size_ = 0;
    if (*p == '#' || *p == '\0') {*p = '\0'; return true;}
    *p = '\0';

    // skip any whitspace
    ++p;
    while (*p == ' ' || *p == '\t') ++p;
    if (*p == '\0' || *p == '#') {return true;}

    // get value
    d.value.str_ = p;
    while (*p != '\0' && (*p != '#' || *(p-1) == '\\')) ++p;
    
    // remove trailing white space and terminate value
    --p;
    while (*p == ' ' || *p == '\t') --p;
    if (*p == '\\' && *(p + 1) != '\0') ++p;
    ++p;
    d.value.size_ = p - d.value.str_;
    *p = '\0';

    return true;
  }

  void unescape(char * str)
  {
    char * i = str;
    char * j = i;
    while (*j) {
      if (*j == '\\') ++j;
      *i = *j;
      ++i;
      ++j;
    }
    *i = '\0';
  }

  void to_lower(char * str)
  {
    for (; *str; str++) *str = asc_tolower(*str);
  }


}
