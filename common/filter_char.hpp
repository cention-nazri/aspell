#ifndef acommon_filter_char_hh
#define acommon_filter_char_hh

// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

namespace acommon {

  struct FilterChar {
    unsigned int chr;
    unsigned int width;
    typedef unsigned int Chr;
    typedef unsigned int Width;
    FilterChar(Chr c = 0, Width w = 1) 
      : chr(c), width(w) {}
    FilterChar(Chr c, FilterChar o)
      : chr(c), width(o.width) {}
    FilterChar(Chr c, const FilterChar * o, unsigned int size)
      : chr(c), width(sum(o,size)) {}
    static Width sum(const FilterChar * o, unsigned int size) {
      Width total = 0; 
      const FilterChar * stop = o + size;
      for (; o != stop; ++o)
	total += o->width;
      return total;
    }
    operator Chr () const {return chr;}
    FilterChar & operator= (Chr c) {chr = c; return *this;}
  };
  
  static inline bool operator==(FilterChar lhs, FilterChar::Chr rhs)
  {
    return lhs.chr == rhs;
  }

  static inline bool operator==(FilterChar lhs, FilterChar rhs)
  {
    return lhs.chr == rhs.chr;
  }
  
  static inline bool operator!=(FilterChar lhs, FilterChar::Chr rhs)
  {
    return lhs.chr != rhs;
  }

  static inline bool operator!=(FilterChar lhs, FilterChar rhs)
  {
    return lhs.chr != rhs.chr;
  }

  
}

#endif
