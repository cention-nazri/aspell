// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#ifndef PSPELL_STRING__HPP
#define PSPELL_STRING__HPP

#include <string.h>
#include <string>

#include "hash_fun.hpp"
#include "parm_string.hpp"
#include "istream.hpp"
#include "ostream.hpp"

namespace pcommon {

  template <typename Ret> class PosibErr;
  
  class String : public std::string, public OStream
  {
  public:
    String() : std::string() {}
    String(const char * s) : std::string(s) {}
    String(const char * s, unsigned int size) : std::string(s, size) {}
    String(ParmString s) : std::string(s) {}
    String(const std::string & s) : std::string(s) {}
    String(const String & other) : std::string(other) {}
    String & operator= (const char * s) {
      std::string::operator= (s);
      return *this;
    }
    inline String & operator= (const PosibErr<const char *> & s);
    String & operator= (ParmString s) {
      std::string::operator= (s);
      return *this;
    }
    String & operator= (const std::string & s) {
      std::string::operator= (s);
      return *this;
    }
    String & operator= (const String & other) {
      std::string::operator= (other);
      return *this;
    }
    /*inline*/ String & operator= (const PosibErr<String> & s);

    ~String() {}

    void clear() {*this = "";}

    void write (char c);
    void write (ParmString);
    void write (const char *, unsigned int);
  };

  inline String operator+ (ParmString rhs, ParmString lhs)
  {
    String tmp;
    tmp.reserve(rhs.size() + lhs.size());
    tmp += rhs;
    tmp += lhs;
    return tmp;
  }

  inline ParmString::ParmString(const String & s) : str_(s.c_str()), size_(s.size()) {}


  class StringIStream : public IStream {
    const char * in_str;
    char         delem;
  public:
    StringIStream(ParmString s, char d = ';')
      : IStream(d), in_str(s) {}
    bool getline(String & str, char c);
    bool read(void * data, unsigned int size);
  };

  template <> struct hash<String> : public HashString<String> {};


}
#endif
