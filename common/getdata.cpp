// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include <string.h>

#include "getdata.hpp"
#include "string.hpp"
#include "asc_ctype.hpp"

#include "iostream.hpp"

namespace acommon {

  bool getdata_pair(IStream & in, DataPair & d, String & buf)
  {
    char * p;

    // get first non blank line
    do {
      buf.clear();
      buf.append('\0'); // to avoid some special cases
      if (!in.append_line(buf)) return false;
      d.line_num++;
      p = buf.mstr() + 1;
      while (*p == ' ' || *p == '\t') ++p;
    } while (*p == '#' || *p == '\0');

    // get key
    d.key.str = p;
    while (*p != '\0' &&
           ((*p != ' ' && *p != '\t' && *p != '#') || *(p-1) == '\\')) ++p;
    d.key.size = p - d.key.str;

    // figure out if there is a value and add terminate key
    d.value.str = p; // in case there is no value
    d.value.size = 0;
    if (*p == '#' || *p == '\0') {*p = '\0'; return true;}
    *p = '\0';

    // skip any whitspace
    ++p;
    while (*p == ' ' || *p == '\t') ++p;
    if (*p == '\0' || *p == '#') {return true;}

    // get value
    d.value.str = p;
    while (*p != '\0' && (*p != '#' || *(p-1) == '\\')) ++p;
    
    // remove trailing white space and terminate value
    --p;
    while (*p == ' ' || *p == '\t') --p;
    if (*p == '\\' && *(p + 1) != '\0') ++p;
    ++p;
    d.value.size = p - d.value.str;
    *p = '\0';

    return true;
  }

  char * unescape(char * dest, const char * src)
  {
    while (*src) {
      if (*src == '\\') {
	++src;
	switch (*src) {
	case 'n': *dest = '\n'; break;
	case 'r': *dest = '\r'; break;
	case 't': *dest = '\t'; break;
	case 'f': *dest = '\f'; break;
	case 'v': *dest = '\v'; break;
	default: *dest = *src;
	}
      } else {
	*dest = *src;
      }
      ++src;
      ++dest;
    }
    *dest = '\0';
    return dest;
  }

  bool escape(char * dest, const char * src, size_t limit, const char * others)
  {
    const char * end = dest + limit;
    while (*src) {
      if (dest == end) return false;
      switch (*src) {
      case '\n': *dest++ = '\\'; *dest = 'n'; break;
      case '\r': *dest++ = '\\'; *dest = 'r'; break;
      case '\t': *dest++ = '\\'; *dest = 't'; break;
      case '\f': *dest++ = '\\'; *dest = 'f'; break;
      case '\v': *dest++ = '\\'; *dest = 'v'; break;
      case '\\': *dest++ = '\\'; *dest = '\\'; break;
      case '#' : *dest++ = '\\'; *dest = '#'; break;
      default:
	if (others && strchr(others, *src)) *dest++ = '\\';
	*dest = *src;
      }
      ++src;
      ++dest;
    }
    *dest = '\0';
    return true;
  }

  void to_lower(char * str)
  {
    for (; *str; str++) *str = asc_tolower(*str);
  }

  bool split(DataPair & d)
  {
    char * p   = d.value;
    char * end = p + d.value.size;
    d.key.str = p;
    while (p != end) {
      ++p;
      if ((*p == ' ' || *p == '\t') && *(p-1) != '\\') break;
    }
    d.key.size = p - d.key.str;
    *p = 0;
    if (p != end) {
      ++p;
      while (p != end && (*p == ' ' || *p == '\t')) ++p;
    }
    d.value.str = p;
    d.value.size = end - p;
    return d.key.size != 0;
  }

  void init(ParmString str, DataPair & d, String & buf)
  {
    const char * s = str;
    while (*s == ' ' || *s == '\t') ++s;
    size_t l = str.size() - (s - str);
    buf.assign(s, l);
    d.value.str  = buf.mstr();
    d.value.size = l;
  }

  bool getline(IStream & in, DataPair & d, String & buf)
  {
    if (!in.getline(buf)) return false;
    d.value.str  = buf.mstr();
    d.value.size = buf.size();
    return true;
  }

}
