// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_CONVERT__HPP
#define ASPELL_CONVERT__HPP

#include "string.hpp"
#include "posib_err.hpp"

namespace acommon {

  class OStream;
  class Config;

  class Convert {
  private:
    String in_code_;
    String out_code_;
    
    static const unsigned int null_len_ = 4; // POSIB FIXME: Be more precise

  protected:
    Convert(ParmString incode, ParmString outcode);

  public:

    virtual PosibErr<void> init(Config &) {return no_err;}

    virtual ~Convert() {}
    
    const char * in_code() const   {return in_code_.c_str();}
    const char * out_code() const  {return out_code_.c_str();}

    void append_null(OStream & out) const
    {
      const char nul[8] = {0}; // 8 should be more than enough
      out.write(nul, null_len_);
    }

    unsigned int null_len() const {return null_len_;}
  
    // this filters will generally not translate null characters
    // if you need a null character at the end, add it yourself
    // with append_null

    void convert(ParmString in, int size, OStream & out) const
    {
      if (size == -1)
	convert(in,out);
      else
	convert_until(in, in + size, out);
      const char buf[4] = {0};
      out.write(buf, 4);
    }

    virtual void convert(const char * in, OStream & out) const;

    virtual const char * convert_until (const char * in, const char * stop,
					OStream & out) const;
  
    virtual bool convert_next_char (const char * & in, 
				    OStream & out) const = 0;
    // converts the next char. Advances "in" to the location of the next
    // string returns false if there is nothing more to convert
    // (ie a null character).  Will not advance in on a null character.

    
  };

  bool operator== (const Convert & rhs, const Convert & lhs);

  class Config;
  
  PosibErr<Convert *> new_convert(Config &,
				  ParmString in, ParmString out);

}

#endif
