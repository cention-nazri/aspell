#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

#include "convert.hpp"
#include "fstream.hpp"
#include "getdata.hpp"
#include "config.hpp"

namespace pcommon {

  typedef unsigned int   Uni32;
  typedef unsigned short Uni16;


  //////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////
  //
  // Lookups
  //
  //////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////

  //////////////////////////////////////////////////////////////////////
  //
  // ToUniLookup
  //

  class ToUniLookup 
  {
    Uni32 data[256];
    static const Uni32 npos = (Uni32)(-1);
  public:
    void reset();
    Uni32 operator[] (char key) const {return data[(unsigned char)key];}
    bool have(char key) const {return data[(unsigned char)key] != npos;}
    bool insert(char key, Uni32 value);
  };

  void ToUniLookup::reset() 
  {
    for (int i = 0; i != 256; ++i)
      data[i] = npos;
  }

  bool ToUniLookup::insert(char key, Uni32 value)
  {
    if (data[(unsigned char)key] != npos) 
      return false;
    data[(unsigned char)key] = value;
    return true;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // FromUniLookup
  //

  // Assumes that the maxium number of items in the table is 256
  // Also assumes (unsigned char)i == i % 256

  // Based on the iso8859-* character sets it is very fast, almsot all
  // lookups involving no more than 2 comparisons.
  // NO looks ups involded more than 3 comparssions.
  // Also, no division (or modules) is done whatsoever.


  struct UniItem {
    Uni32 key;
    char  value;
  };

  class FromUniLookup 
  {
  private:
    char unknown;
    static const Uni32 npos = (Uni32)(-1);
    UniItem * overflow_end;
  
    UniItem data[256*4];

    UniItem overflow[256]; // you can never be too careful;
  
  public:
    FromUniLookup(char u = '?') : unknown(u) {}
    void reset();
    char operator[] (Uni32 key) const;
    bool insert(Uni32 key, char value);
  };

  void FromUniLookup::reset()
  {
    for (unsigned int i = 0; i != 256*4; ++i)
      data[i].key = npos;
    overflow_end = overflow;
  }

  char FromUniLookup::operator[] (Uni32 k) const
  {
    const UniItem * i = data + (unsigned char)k * 4;

    if (i->key == k) return i->value;
    ++i;
    if (i->key == k) return i->value;
    ++i;
    if (i->key == k) return i->value;
    ++i;
    if (i->key == k) return i->value;
  
    if (i->key == npos) return unknown;
  
    for(i = overflow; i != overflow_end; ++i)
      if (i->key == k) return i->value;

    return unknown;
  }

  bool FromUniLookup::insert(Uni32 k, char v) 
  {
    UniItem * i = data + (unsigned char)k * 4;
    UniItem * e = i + 4;
    while (i != e && i->key != npos) {
      if (i->key == k)
	return false;
      ++i;
    }
    if (i == e) {
      for(i = overflow; i != overflow_end; ++i)
	if (i->key == k) return false;
    }
    i->key = k;
    i->value = v;
    return true;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // CharLookup
  //

  class CharLookup 
  {
  private:
    int data[256];
  public:
    void reset();
    char operator[] (char key) const {return data[(unsigned char)key];}
    bool insert(char key, char value);
  };

  void CharLookup::reset() {
    for (int i = 0; i != 256; ++i) 
      data[i] = -1;
  }

  bool CharLookup::insert(char key, char value) 
  {
    if (data[(unsigned char)key] != -1)
      return false;
    data[(unsigned char)key] = value;
    return true;
  }


  //////////////////////////////////////////////////////////////////////
  //
  //  StriaghtThrough
  //

  class StraightThrough : public Convert
  {
  public:
    StraightThrough(const char * e)
      : Convert(e,e) {}
    void convert           (const char   * in, 
			    OStream & out) const;
    const char * convert_until (const char   * in, const char * stop, 
				OStream & out) const;
    bool convert_next_char (const char * & in, 
			    OStream & out) const;
  };

  void StraightThrough::convert(const char * in, 
				OStream & out) const
  {
    out.write(in);
  }

  const char * StraightThrough::convert_until(const char * in, 
					      const char * stop,
					      OStream & out) const
  {
    out.write(in, stop-in);
    return stop;
  }

  bool StraightThrough::convert_next_char (const char * & in, 
						  OStream & out) const
  {
    if (*in != '\0') {
      out.write(in,1);
      ++in;
      return true;
    } else {
      return false;
    }
  }

  //////////////////////////////////////////////////////////////////////
  //
  // read in char data
  //

  void read_in_char_data (CanHaveError & error,
			  Config & config,
			  const char * encoding,
			  ToUniLookup & to,
			  FromUniLookup & from)
  {
    error.reset_error();
    to.reset();
    from.reset();
    const char * temp_str = config.retrieve("pspell-data-dir");
    if (temp_str == 0)
      temp_str = config.retrieve("data-dir");
    assert(temp_str != 0);
    String file_name = temp_str;
    file_name += '/';
    file_name += encoding;
    file_name += ".map";
    FStream data(file_name.c_str(), "r");
    if (!data) {
      error.set_error(unknown_encoding, encoding);
      // FIXME: add appending for appending messages to error class
      //error.error_mesg_ += " This could also mean that the file \"";
      //error.error_mesg_ += file_name;
      //error.error_mesg_ += "\" could not be opened for reading "
      //  "or does not exist.";
      return;
    }
    String chr_hex,uni_hex;
    char  chr;
    Uni32 uni;
    char * p;
    unsigned long t;
    while (getdata_pair(data, chr_hex, uni_hex)) {
      p = (char *)chr_hex.c_str();
      t = strtoul(p, &p, 16);
      if (p != chr_hex.c_str() + chr_hex.size() 
	  || t != (unsigned char)t /* check for overflow */) 
	{
	  error.error_with_file(encoding, ".dat");
	  error.set_error(bad_key, 
			  chr_hex.c_str(),
			  "two digit hex string");
	  return;
	}
      chr = (char)t;
     
      p = (char *)uni_hex.c_str();
      t = strtoul(p, &p, 16);
      if (p != uni_hex.c_str() + uni_hex.size() 
	  || t != (Uni32)t /* check for overflow */) 
	{
	  error.error_with_file(encoding, ".dat");
	  error.set_error(bad_value,
			  chr_hex.c_str(), uni_hex.c_str(),
			  "four digit hex string");
	  return;
	}
      uni = (Uni32)t;

      if (to.have(chr)) {
	error.error_with_file(encoding, ".dat");
	error.set_error(duplicate,
			"Character",
			chr_hex.c_str());
	return;
      }
      to.insert(chr, uni);
      if (!from.insert(uni, chr)) {
	error.error_with_file(encoding, ".dat");
	error.set_error(duplicate,
			"Uni Character",
			uni_hex.c_str());
	return;
      }
    }
  
    // insert the ascii characters if they are not already there
    unsigned int i; 
    for (i = 0; i != 128; ++i) {
      if (to.insert(i, i))
	from.insert(i,i);
    }
    for (; i != 255; ++i) {
      to.insert(i, '?');
    }
  
  }


  //////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////
  //
  //  Convert
  //
  //////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////


  Convert::Convert(const char * incode, const char * outcode)
    : in_code_(incode), out_code_(outcode) 
  {}

  bool operator== (const Convert & rhs, const Convert & lhs)
  {
    return strcmp(rhs.in_code(), lhs.in_code()) == 0
      && strcmp(rhs.out_code(), lhs.out_code()) == 0;
  }

  void Convert::convert (const char *  in, 
			       OStream & out) const
  {
    while (convert_next_char(in, out));
  }

  const char * Convert::convert_until (const char *  in, 
					     const char * stop, 
					     OStream & out) const
  {
    while (in < stop && convert_next_char(in, out));
    return in;
  }

  //////////////////////////////////////////////////////////////////////
  //
  //  Char to Uni16
  //

  class Char_Uni16 : public Convert
  {
  public:
    ToUniLookup lookup;
    Char_Uni16(Config & c, const char * e);
    bool convert_next_char (const char * & in, 
			    OStream & out) const;
  };

  Char_Uni16::Char_Uni16(Config & c, const char * e) 
    : Convert(e, "machine unsigned 16")
  {
    FromUniLookup unused;
    read_in_char_data(*this, c, e, lookup, unused);
  }


  bool Char_Uni16
  ::convert_next_char (const char * & in, 
		       OStream & out) const
  {
    if (*in != '\0') {
      Uni16 d = lookup[*in];
      out.write((char *)&d, 2);
      ++in;
      return true;
    } else {
      return false;
    }
  }

  //////////////////////////////////////////////////////////////////////
  //
  //  Char to Uni32
  //

  class Char_Uni32 : public Convert
  {
  public:
    ToUniLookup lookup;
    Char_Uni32(Config & c, const char * e);
    bool convert_next_char (const char * & in, 
			    OStream & out) const;
  };

  Char_Uni32::Char_Uni32(Config & c, const char * e) 
    : Convert(e, "machine unsigned 32")
  {
    FromUniLookup unused;
    read_in_char_data(*this, c, e, lookup, unused);
  }


  bool Char_Uni32
  ::convert_next_char (const char * & in, 
		       OStream & out) const
  {
    if (*in != '\0') {
      Uni32 d = lookup[*in];
      out.write((char *)&d, 4);
      ++in;
      return true;
    } else {
      return false;
    }
  }

  //////////////////////////////////////////////////////////////////////
  //
  //  Char to UTF8
  //

  class Char_UTF8 : public Convert
  {
  public:
    ToUniLookup lookup;
    Char_UTF8(Config & c, const char * e);
    bool convert_next_char (const char * & in, 
			    OStream & out) const;
  };

  Char_UTF8::Char_UTF8(Config & c, const char * e) 
    : Convert(e, "UTF-8")
  {
    FromUniLookup unused;
    read_in_char_data(*this, c, e, lookup, unused);
  }


  bool Char_UTF8
  ::convert_next_char (const char * & in, 
		       OStream & out) const
  {
    if (*in != '\0') {
      Uni32 c = lookup[*in];
      char str[4];

      if (c < 0x80) {
	str[0] = (char)c;
	out.write(str, 1);
      }
      else if (c < 0x800) {
	str[0] = (char)(0xC0 | c>>6);
	str[1] = (char)(0x80 | c & 0x3F);
	out.write(str, 2);
      }
      else if (c < 0x10000) {
	str[0] = (0xE0 | c>>12);
	str[1] = (0x80 | c>>6 & 0x3F);
	str[2] = (0x80 | c & 0x3F);
	out.write(str, 3);
      }
      else if (c < 0x200000) {
	str[0] = (0xF0 | c>>18);
	str[1] = (0x80 | c>>12 & 0x3F);
	str[2] = (0x80 | c>>6 & 0x3F);
	str[3] = (0x80 | c & 0x3F);
	out.write(str, 4);
      }

      ++in;
      return true;

    } else {

      return false;

    }
  }

  //////////////////////////////////////////////////////////////////////
  //
  //  Uni16 to Char
  //

  class Uni16_Char : public Convert
  {
  public:
    FromUniLookup lookup;
    Uni16_Char(Config & c, const char * e);
    bool convert_next_char (const char * & in, 
			    OStream & out) const;
  };

  Uni16_Char::Uni16_Char(Config & c, const char * e)
    : Convert("machine unsigned 16", e) 
  {
    ToUniLookup unused;
    read_in_char_data(*this, c, e, unused, lookup);
  }


  bool Uni16_Char
  ::convert_next_char (const char * & in, 
		       OStream & out) const
  {
    Uni16 c = *(const Uni16 *)(in);
    if (c != 0) {
      char d = lookup[c];
      out.write(&d, 1);
      in += 2;
      return true;
    } else {
      return false;
    }
  }

  //////////////////////////////////////////////////////////////////////
  //
  //  Uni32 to Char
  //

  class Uni32_Char : public Convert
  {
  public:
    FromUniLookup lookup;
    Uni32_Char(Config & c, const char * e);
    bool convert_next_char (const char * & in, 
			    OStream & out) const;
  };

  Uni32_Char::Uni32_Char(Config & c, const char * e)
    : Convert("machine unsigned 32", e) 
  {
    ToUniLookup unused;
    read_in_char_data(*this, c, e, unused, lookup);
  }


  bool Uni32_Char
  ::convert_next_char (const char * & in, 
		       OStream & out) const
  {
    Uni32 c = *(const Uni32 *)(in);
    if (c != 0) {
      char d = lookup[c];
      out.write(&d, 1);
      in += 4;
      return true;
    } else {
      return false;
    }
  }

  //////////////////////////////////////////////////////////////////////
  //
  //  UTF8 to Char
  //

  class UTF8_Char : public Convert
  {
  public:
    FromUniLookup lookup;
    UTF8_Char(Config & c, const char * e);
    bool convert_next_char (const char * & in, 
			    OStream & out) const;
  };

  UTF8_Char::UTF8_Char(Config & c, const char * e)
    : Convert("UTF-8", e) 
  {
    ToUniLookup unused;
    read_in_char_data(*this, c, e, unused, lookup);
  }

#define get_check_next \
  c = *in;                                        \
  if ((c & 0xC0) != 0x80) {u = '?'; goto FINISH;} \
  ++in;                                           \
  u <<= 6;                                        \
  u |= c & 0x3F


  bool UTF8_Char
  ::convert_next_char (const char * & in, 
		       OStream & out) const
  {
    if (*in == '\0') {
      return false;
    }

    Uni32 u = (Uni32)(-1);

    char c = *in++;
    while ((c & 0xC0) == 0x80) c = *in++;
    if ((c & 0x80) == 0x00) { // 1-byte wide
      u = c;
    } else if ((c & 0xE0) == 0xC0) { // 2-byte wide
      u  = c & 0x1F; 
      get_check_next;
    } else if ((c & 0xF0) == 0xE0) { // 3-byte wide
      u  = c & 0x0F; 
      get_check_next;
      get_check_next;
    } else if ((c & 0xF8) == 0xF0) { // 4-byte wide
      u  = c & 0x0E; 
      get_check_next;
      get_check_next;
      get_check_next;
    }

    FINISH:

    assert (u != (Uni32)(-1));

    char d = lookup[u];
    out.write(&d, 1);
    return true;

  }

  //////////////////////////////////////////////////////////////////////
  //
  //  Char to Char
  //

  class Char_Char : public Convert
  {
  public:
    CharLookup lookup;
  
    Char_Char(Config & c, const char * in, const char * out);
    bool convert_next_char (const char * & in, 
			    OStream & out) const;
  };

  Char_Char::Char_Char(Config & c, 
				     const char * in_code, 
				     const char * out_code)
    : Convert(in_code, out_code) 
  {
    ToUniLookup   to;
    FromUniLookup from;
    {
      FromUniLookup unused;
      read_in_char_data(*this, c, in_code, to, unused);
      if (error_info().primary != 0) return;
    } {
      ToUniLookup unused;
      read_in_char_data(*this, c, out_code, unused, from);
      if (error_info().primary != 0) return;
    }
    lookup.reset();
    for (unsigned int i = 0; i != 256; ++i) {
      lookup.insert(i, from[to[i]]);
    }
  }


  bool Char_Char
  ::convert_next_char (const char * & in, 
		       OStream & out) const
  {
    if (*in != '\0') {
      char d = lookup[*in];
      out.write(&d, 1);
      in += 1;
      return true;
    } else {
      return false;
    }
  }


  //////////////////////////////////////////////////////////////////////
  //
  // new_pspell_convert
  //

  CanHaveError * new_pspell_convert(Config & c,
					  const char * in, 
					  const char * out) 
  {
    assert(sizeof(Uni16) == 2);
    assert(sizeof(Uni32) == 4);

    String in_s  = in;
    String out_s = out;

    unsigned int i;
    for (i = 0; i != in_s.size(); ++i)
      in_s[i] = tolower(in_s[i]);
    for (i = 0; i != out_s.size(); ++i)
      out_s[i] = tolower(out_s[i]);
    in  = in_s .c_str();
    out = out_s.c_str();

    if (strcmp(in ,"ascii") == 0) 
      in = "iso8859-1";
    if (strcmp(out,"ascii") == 0) 
      out = "iso8859-1";

    if (strcmp(in, out) == 0)
      return new StraightThrough(in);

    else if (strcmp(in, "machine unsigned 16") == 0)
      return new Uni16_Char(c,out);

    else if (strcmp(in, "machine unsigned 32") == 0)
      return new Uni32_Char(c,out);

    else if (strcmp(in, "utf-8") == 0)
      return new UTF8_Char(c,out);

    else if (strcmp(out, "machine unsigned 16") == 0)
      return new Char_Uni16(c,in);

    else if (strcmp(out, "machine unsigned 32") == 0)
      return new Char_Uni32(c,in);

    else if (strcmp(out, "utf-8") == 0)
      return new Char_UTF8(c,in);

    else
      return new Char_Char(c, in, out);
  }

}
