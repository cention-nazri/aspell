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
#include "filter_char_vector.hpp"
#include "stack_ptr.hpp"
#include "filter.hpp"
#include "cache.hpp"

namespace acommon {

  class OStream;
  class Config;

  struct ConvBase : public Cacheable {
    typedef const Config CacheConfig;
    typedef const char * CacheKey;
    String key;
    bool cache_key_eq(const char * l) const  {return key == l;}
    ConvBase() {}
  private:
    ConvBase(const ConvBase &);
    void operator=(const ConvBase &);
  };

  struct Decode : public ConvBase {
    virtual PosibErr<void> init(ParmString code, const Config &) {return no_err;}
    virtual void decode(const char * in, int size,
			FilterCharVector & out) const = 0;
    virtual PosibErr<void> decode_ec(const char * in, int size,
                                     FilterCharVector & out, ParmString orig) const = 0;
    static PosibErr<Decode *> get_new(const String &, const Config *);
    virtual ~Decode() {}
  };
  struct Encode : public ConvBase {
    // null characters should be tretead like any other character
    // by the encoder.
    virtual PosibErr<void> init(ParmString, const Config &) {return no_err;}
    virtual void encode(const FilterChar * in, const FilterChar * stop, 
                        CharVector & out) const = 0;
    virtual PosibErr<void> encode_ec(const FilterChar * in, const FilterChar * stop, 
                                     CharVector & out, ParmString orig) const = 0;
    // encodes the string by modifying the input, if it can't abe done
    // return false
    virtual bool encode_direct(FilterChar * in, FilterChar * stop) const
      {return false;}
    static PosibErr<Encode *> get_new(const String &, const Config *);
    virtual ~Encode() {}
  };
  struct DirectConv  { // convert directly from in_code to out_code
    // should not take owenership of decode and encode 
    // decode and encode guaranteed to stick around for the life
    // of the object
    virtual PosibErr<void> init(const Decode *, const Encode *, 
				const Config &) {return no_err;}
    virtual void convert(const char * in, int size, 
			 CharVector & out) const = 0;
    virtual PosibErr<void> convert_ec(const char * in, int size, 
                                      CharVector & out, ParmString orig) const = 0;
    virtual ~DirectConv() {}
  };

  typedef FilterCharVector ConvertBuffer;

  class Convert {
  private:
    CachePtr<Decode> decode_;
    CachePtr<Encode> encode_;
    StackPtr<DirectConv> conv_;

    ConvertBuffer buf_;

    static const unsigned int null_len_ = 4; // POSIB FIXME: Be more precise

    Convert(const Convert &);
    void operator=(const Convert &);

  public:
    Convert() {}

    // This filter is used when the convert method is called.  It must
    // be set up by an external entity as this class does not set up
    // this class in any way.
    Filter filter;

    PosibErr<void> init(const Config &, ParmString in, ParmString out);

    const char * in_code() const   {return decode_->key.c_str();}
    const char * out_code() const  {return encode_->key.c_str();}

    void append_null(CharVector & out) const
    {
      const char nul[4] = {0,0,0,0}; // 4 should be enough
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

    // does NOT pass it through filters
    // DOES NOT use an internal state
    void convert(const char * in, int size, CharVector & out, ConvertBuffer & buf) const
    {
      if (conv_) {
	conv_->convert(in,size,out);
      } else {
        buf.clear();
        decode_->decode(in, size, buf);
        buf.append(0);
        encode_->encode(buf.pbegin(), buf.pend(), out);
      }
    }

    // does NOT pass it through filters
    // DOES NOT use an internal state
    PosibErr<void> convert_ec(const char * in, int size, CharVector & out, 
                              ConvertBuffer & buf, ParmString orig) const
    {
      if (conv_) {
	RET_ON_ERR(conv_->convert_ec(in,size,out, orig));
      } else {
        buf.clear();
        RET_ON_ERR(decode_->decode_ec(in, size, buf, orig));
        buf.append(0);
        RET_ON_ERR(encode_->encode_ec(buf.pbegin(), buf.pend(), 
                                      out, orig));
      }
      return no_err;
    }


    // convert has the potential to use internal buffers and
    // is therefore not const.  It is also not thread safe
    // and I have no intention to make it thus.

    void convert(const char * in, int size, CharVector & out) {
      if (filter.empty()) {
        convert(in,size,out,buf_);
      } else {
        generic_convert(in,size,out);
      }
    }

    void generic_convert(const char * in, int size, CharVector & out);
    
  };

  bool operator== (const Convert & rhs, const Convert & lhs);

  const char * fix_encoding_str(ParmString enc, String & buf);

  bool is_ascii_enc(ParmString enc);

  PosibErr<Convert *> internal_new_convert(const Config & c, 
                                           ParmString in, ParmString out,
                                           bool if_needed);
  
  static inline PosibErr<Convert *> new_convert(const Config & c,
                                                ParmString in, ParmString out)
  {
    return internal_new_convert(c,in,out,false);
  }
  
  static inline PosibErr<Convert *> new_convert_if_needed(const Config & c,
                                                          ParmString in, ParmString out)
  {
    return internal_new_convert(c,in,out,true);
  }

  struct ConvObj {
    Convert * ptr;
    ConvObj(Convert * c = 0) : ptr(c) {}
    ~ConvObj() {delete ptr;}
    PosibErr<void> setup(const Config & c, ParmString from, ParmString to)
    {
      delete ptr;
      ptr = 0;
      PosibErr<Convert *> pe = new_convert_if_needed(c, from, to);
      if (pe.has_err()) return pe;
      ptr = pe.data;
      return no_err;
    }
    operator const Convert * () const {return ptr;}
  private:
    ConvObj(const ConvObj &);
    void operator=(const ConvObj &);
  };

  struct ConvP {
    const Convert * conv;
    ConvertBuffer buf0;
    CharVector buf;
    operator bool() const {return conv;}
    ConvP(const Convert * c = 0) : conv(c) {}
    ConvP(const ConvObj & c) : conv(c.ptr) {}
    ConvP(const ConvP & c) : conv(c.conv) {}
    void operator=(const ConvP & c) { conv = c.conv; }
    PosibErr<void> setup(const Config & c, ParmString from, ParmString to)
    {
      delete conv;
      conv = 0;
      PosibErr<Convert *> pe = new_convert_if_needed(c, from, to);
      if (pe.has_err()) return pe;
      conv = pe.data;
      return no_err;
    }
    char * operator() (char * str, size_t sz)
    {
      if (conv) {
        buf.clear();
        conv->convert(str, sz, buf, buf0);
        return buf.data();
      } else {
        return str;
      }
    }
    char * operator() (MutableString str)
    {
      return operator()(str.str, str.size);
    }
    char * operator() (char * str)
    {
      if (conv) {
        buf.clear();
        conv->convert(str, strlen(str), buf, buf0);
        return buf.data();
      } else {
        return str;
      }
    }
    const char * operator() (ParmString str)
    {
      if (conv) {
        buf.clear();
        conv->convert(str, str.size(), buf, buf0);
        return buf.data();
      } else {
        return str;
      }
    }
    const char * operator() (char c)
    {
      char buf2[2] = {c, 0};
      return operator()(ParmString(buf2,1));
    }
  };

  struct Conv : public ConvP
  {
    ConvObj conv_obj;
    Conv(Convert * c = 0) : ConvP(c), conv_obj(c) {}
    PosibErr<void> setup(const Config & c, ParmString from, ParmString to)
    {
      RET_ON_ERR(conv_obj.setup(c,from,to));
      conv = conv_obj.ptr;
      return no_err;
    }
  };

  struct ConvECP {
    const Convert * conv;
    ConvertBuffer buf0;
    CharVector buf;
    operator bool() const {return conv;}
    ConvECP(const Convert * c = 0) : conv(c) {}
    ConvECP(const ConvObj & c) : conv(c.ptr) {}
    ConvECP(const ConvECP & c) : conv(c.conv) {}
    void operator=(const ConvECP & c) { conv = c.conv; }
    PosibErr<void> setup(const Config & c, ParmString from, ParmString to)
    {
      delete conv;
      conv = 0;
      PosibErr<Convert *> pe = new_convert_if_needed(c, from, to);
      if (pe.has_err()) return pe;
      conv = pe.data;
      return no_err;
    }
    // assumes str is null terminated
    PosibErr<char *> operator() (char * str, size_t sz)
    {
      if (conv) {
        buf.clear();
        RET_ON_ERR(conv->convert_ec(str, sz, buf, buf0, str));
        return buf.data();
      } else {
        return str;
      }
    }
    PosibErr<char *> operator() (MutableString str)
    {
      return operator()(str.str, str.size);
    }
    PosibErr<char *> operator() (char * str)
    {
      if (conv) {
        buf.clear();
        RET_ON_ERR(conv->convert_ec(str, strlen(str), buf, buf0, str));
        return buf.data();
      } else {
        return str;
      }
    }
    PosibErr<const char *> operator() (ParmString str)
    {
      if (conv) {
        buf.clear();
        RET_ON_ERR(conv->convert_ec(str, str.size(), buf, buf0, str));
        return buf.data();
      } else {
        return str.str();
      }
    }
    PosibErr<const char *> operator() (char c)
    {
      char buf2[2] = {c, 0};
      return operator()(ParmString(buf2,1));
    }
  };

  struct ConvEC : public ConvECP
  {
    ConvObj conv_obj;
    ConvEC(Convert * c = 0) : ConvECP(c), conv_obj(c) {}
    PosibErr<void> setup(const Config & c, ParmString from, ParmString to)
    {
      RET_ON_ERR(conv_obj.setup(c,from,to));
      conv = conv_obj.ptr;
      return no_err;
    }
  };



}

#endif
