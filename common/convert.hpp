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
    // string MUST be null terminated, even if size is given
    virtual void decode(const char * in, int size,
			FilterCharVector & out) const = 0;
    virtual PosibErr<void> decode_ec(const char * in, int size,
                                     FilterCharVector & out, ParmString orig) const = 0;
    static PosibErr<Decode *> get_new(const String &, const Config *);
    virtual ~Decode() {}
  };
  struct Encode : public ConvBase {
    virtual PosibErr<void> init(ParmString, const Config &) {return no_err;}
    virtual void encode(const FilterChar * in, const FilterChar * stop, 
                        CharVector & out) const = 0;
    virtual PosibErr<void> encode_ec(const FilterChar * in, const FilterChar * stop, 
                                     CharVector & out, ParmString orig) const = 0;
    // may convert inplace
    virtual bool encode(FilterChar * & in, FilterChar * & stop, 
                        FilterCharVector & buf) const {return false;}
    static PosibErr<Encode *> get_new(const String &, const Config *);
    virtual ~Encode() {}
  };
  struct DirectConv  { // convert directly from in_code to out_code
    // should not take owenership of decode and encode 
    // decode and encode guaranteed to stick around for the life
    // of the object
    virtual PosibErr<void> init(const Decode *, const Encode *, 
				const Config &) {return no_err;}
    // string MUST be null terminated, even if size is given
    virtual void convert(const char * in, int size,
			 CharVector & out) const = 0;
    virtual PosibErr<void> convert_ec(const char * in, int size,
                                      CharVector & out, ParmString orig) const = 0;
    virtual ~DirectConv() {}
  };
  template <class T> struct NormTable;
  struct FromUniNormEntry;
  struct ToUniNormEntry;
  struct NormTables : public Cacheable {
    typedef const Config CacheConfig;
    typedef const char * CacheKey;
    String key;
    bool cache_key_eq(const char * l) const  {return key == l;}
    static PosibErr<NormTables *> get_new(const String &, const Config *);
    NormTable<FromUniNormEntry> * internal;
    NormTable<FromUniNormEntry> * strict_d;
    NormTable<FromUniNormEntry> * strict;
    struct ToUniTable {
      String name;
      NormTable<ToUniNormEntry> * data;
      NormTable<ToUniNormEntry> * ptr;
      ToUniTable() : data(), ptr() {}
    };
    typedef Vector<ToUniTable> ToUni;
    Vector<ToUniTable> to_uni;
    ~NormTables();
  };

  typedef FilterCharVector ConvertBuffer;

  class Convert {
  private:
    CachePtr<Decode> decode_d;
    Decode * decode_;
    CachePtr<Encode> encode_d;
    Encode * encode_;
    CachePtr<NormTables> norm_tables_;
    DirectConv * conv_;

    static const size_t memory_size = 96;
    char memory[memory_size];

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
    PosibErr<void> init_norm_to(const Config &, ParmString in, ParmString out);
    PosibErr<void> init_norm_from(const Config &, ParmString in, ParmString out);
    
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

    bool encode(FilterChar * & in, FilterChar * & stop, 
                FilterCharVector & buf) const
      {return encode_->encode(in,stop,buf);}

    // does NOT pass it through filters
    // DOES NOT use an internal state
    void convert(const char * in, int size, CharVector & out, ConvertBuffer & buf) const
    {
      if (conv_) {
	conv_->convert(in,size,out);
      } else {
        buf.clear();
        decode_->decode(in, size, buf);
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

  enum Normalize {NormNone, NormFrom, NormTo};

  PosibErr<Convert *> internal_new_convert(const Config & c, 
                                           ParmString in, ParmString out,
                                           bool if_needed,
                                           Normalize n);
  
  static inline PosibErr<Convert *> new_convert(const Config & c,
                                                ParmString in, ParmString out,
                                                Normalize n)
  {
    return internal_new_convert(c,in,out,false,n);
  }
  
  static inline PosibErr<Convert *> new_convert_if_needed(const Config & c,
                                                          ParmString in, ParmString out,
                                                          Normalize n)
  {
    return internal_new_convert(c,in,out,true,n);
  }

  struct ConvObj {
    Convert * ptr;
    ConvObj(Convert * c = 0) : ptr(c) {}
    ~ConvObj() {delete ptr;}
    PosibErr<void> setup(const Config & c, ParmString from, ParmString to, Normalize norm)
    {
      delete ptr;
      ptr = 0;
      PosibErr<Convert *> pe = new_convert_if_needed(c, from, to, norm);
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
    PosibErr<void> setup(const Config & c, ParmString from, ParmString to, 
                         Normalize norm)
    {
      delete conv;
      conv = 0;
      PosibErr<Convert *> pe = new_convert_if_needed(c, from, to, norm);
      if (pe.has_err()) return pe;
      conv = pe.data;
      return no_err;
    }
    char * operator() (char * str, size_t sz)
    {
      if (conv) {
        buf.clear();
        conv->convert(str, sz, buf, buf0);
        buf.ensure_null_end();
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
        buf.ensure_null_end();
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
        buf.ensure_null_end();
        return buf.data();
      } else {
        return str;
      }
    }
    const char * operator() (char c)
    {
      buf.clear();
      if (conv) {
        char str[2] = {c, 0};
        conv->convert(str, 1, buf, buf0);
      } else {
        buf.append(c);
      }
      buf.ensure_null_end();
      return buf.data();
    }
  };

  struct Conv : public ConvP
  {
    ConvObj conv_obj;
    Conv(Convert * c = 0) : ConvP(c), conv_obj(c) {}
    PosibErr<void> setup(const Config & c, ParmString from, ParmString to, Normalize norm)
    {
      RET_ON_ERR(conv_obj.setup(c,from,to,norm));
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
    PosibErr<void> setup(const Config & c, ParmString from, ParmString to, Normalize norm)
    {
      delete conv;
      conv = 0;
      PosibErr<Convert *> pe = new_convert_if_needed(c, from, to, norm);
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
        buf.ensure_null_end();
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
        buf.ensure_null_end();
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
        buf.ensure_null_end();
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
    PosibErr<void> setup(const Config & c, ParmString from, ParmString to, Normalize norm)
    {
      RET_ON_ERR(conv_obj.setup(c,from,to,norm));
      conv = conv_obj.ptr;
      return no_err;
    }
  };

  struct MBLen 
  {
    enum Encoding {Other, UTF8, UCS2, UCS4} encoding;
    MBLen() : encoding(Other) {}
    PosibErr<void> setup(const Config &, ParmString enc);
    unsigned operator()(const char * str, const char * stop);
    unsigned operator()(const char * str, unsigned byte_size) {
      return operator()(str, str + byte_size);}
  };

}

#endif
