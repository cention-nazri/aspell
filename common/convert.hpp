// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_CONVERT__HPP
#define ASPELL_CONVERT__HPP

#include "string.hpp"
#include "posib_err.hpp"
#include "char_vector.hpp"
#include "filter_char.hpp"
#include "stack_ptr.hpp"

namespace acommon {

  class OStream;
  class Config;

  typedef Vector<FilterChar> FilterCharVector;

  struct Decode {
    virtual PosibErr<void> init(ParmString code, Config &) {return no_err;}
    virtual void decode(const char * in, int size, 
			FilterCharVector & out) const = 0;
  };
  struct Encode {
    // null characters should be tretead like any other character
    // by the encoder.
    virtual PosibErr<void> init(ParmString, Config &) {return no_err;}
    virtual void encode(const FilterChar * in, const FilterChar * stop, 
			CharVector & out) const = 0;
    // encodes the string by modifying the input, if it can't be done
    // return false
    virtual bool encode_direct(FilterChar * in, FilterChar * stop) const
      {return false;}
  };
  struct Conv { // convert directy from in_code to out_code
    // should not take owenership of decode and encode 
    // decode and encode guaranteed to stick around for the life
    // of the object
    virtual PosibErr<void> init(const Decode *, const Encode *, 
				Config &) {return no_err;}
    virtual void convert(const char * in, int size, 
			 CharVector & out) const = 0;
  };

  class Convert {
  private:
    String in_code_;
    String out_code_;
    
    StackPtr<Decode> decode_;
    StackPtr<Encode> encode_;
    StackPtr<Conv> conv_;

    static const unsigned int null_len_ = 4; // POSIB FIXME: Be more precise

  public:

    PosibErr<void> init(Config &, ParmString in, ParmString out);

    const char * in_code() const   {return in_code_.c_str();}
    const char * out_code() const  {return out_code_.c_str();}

    void append_null(CharVector & out) const
    {
      const char nul[8] = {0}; // 8 should be more than enough
      out.write(nul, null_len_);
    }

    unsigned int null_len() const {return null_len_;}
  
    // this filters will generally not translate null characters
    // if you need a null character at the end, add it yourself
    // with append_null

    void decode(const char * in, int size, FilterCharVector & out) const 
      {decode_->decode(in,size,out);}
    
    void encode(const FilterChar * in, const FilterChar * stop, 
		CharVector & out) const
      {encode_->encode(in,stop,out);}

    bool encode_direct(FilterChar * in, FilterChar * stop) const
      {return encode_->encode_direct(in,stop);}
    void convert(const char * in, int size, CharVector & out) const
      {conv_->convert(in,size,out);}
    
  };

  bool operator== (const Convert & rhs, const Convert & lhs);

  class Config;
  
  PosibErr<Convert *> new_convert(Config &,
				  ParmString in, ParmString out);

}

#endif
