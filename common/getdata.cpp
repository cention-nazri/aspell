
#include <string.h>

#include "istream.hpp"
#include "string.hpp"
#include "getdata.hpp"

namespace pcommon {

  static inline char * skip_space(char * s) {
    while (*s == ' ' || *s == '\t') ++s;
    return s;
  }

  static inline char * next_space(char * s) {
    while (*s != ' ' && *s != '\t' && *s != '\0') ++s;
    return s;
  }

  bool getdata_pair(IStream & in, 
		    String & key, 
		    String & data)
  {
    String temp;
    String::const_iterator b, m, e; // begin, middle, end
    do { 
      // get the next non blank line and remove comments and 
      // leading and trailing space
      if (!in.getline(temp)) return false;
      b = temp.begin();
      m = b;
      e = temp.end();
      while (m != e && (*m != '#' || (m != b && *(m-1) == '\\')))
	++m;
      while (b != m && (*b == ' ' || *b == '\t')) ++b;
    } while (b == m); // try again if the line is blank
    e = m;
    m = b;
    while (m != e && ((*m != ' ' && *m != '\t') 
		      || *(m-1) == '\\')) // b != m is garenteed
      ++m;
    key.assign(b, m);
    unescape(key);
    b = m;
    m = e;
    while (b != e && *b == ' ' || *b == '\t') ++b;
    while (m > b + 1 && (*(m-1) == ' ' || *(m-1) == '\t')) --m;
    if (*m == '\\') ++m;
    // (last two lines) remove space at the end.
    data.assign(b, m);
    unescape(data);
    return true;
  }

  void unescape(String & str)
  {
    String::iterator i = str.begin();
    String::iterator j = i;
    String::iterator end = str.end();
    while (j != end) {
      if (*j == '\\') ++j;
      *i = *j;
      ++i;
      ++j;
    }
    str.resize(i - str.begin());
  }

}
