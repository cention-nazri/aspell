// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_CONVERT__HPP
#define ASPELL_CONVERT__HPP

#include "string.hpp"

namespace acommon {

  class OStream;

  class Convert {
  private:
    String in_code_;
    String out_code_;

  protected:
    Convert(const char * incode, const char * outcode);

  public:

    const char * in_code() const   {return in_code_.c_str();}
    const char * out_code() const  {return out_code_.c_str();}
  
    virtual void convert           (const char * in, 
				    OStream & out) const;

    virtual const char * convert_until (const char * in, const char * stop,
					OStream & out) const;

    void convert(const char * in, int size, OStream & out) const
    {
      if (size == -1)
	convert(in,out);
      else
	convert_until(in, in + size, out);
    }
  
    virtual bool convert_next_char (const char * & in, 
				    OStream & out) const = 0;
    // converts the next char. Advances "in" to the location of the next
    // string
  };

  bool operator== (const Convert & rhs, const Convert & lhs);

  class Config;

  CanHaveError * new_aspell_convert(Config &,
				    const char * in, const char * out);

}

#endif
