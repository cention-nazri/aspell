// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#ifndef PSPELL_FSTREAM__HPP
#define PSPELL_FSTREAM__HPP

#include <stdio.h>
#include "string.hpp"
#include "istream.hpp"
#include "ostream.hpp"
#include "posib_err.hpp"

namespace pcommon {
  class String;

  class FStream : public IStream, public OStream
  {
  private:
    FILE * file_;

  public:
    FStream(char d = '\n') : IStream(d), file_(0) {}
    FStream(FILE * f) : IStream('\n'), file_(f) {}
    ~FStream() {close();}
    PosibErr<void> open(ParmString, const char *);
    void close();
 
    operator bool() {return file_ != 0 && !feof(file_) && !ferror(file_);}
    int get() {return getc(file_);}
    void ignore() {getc(file_);}
    int peek() {int c = getc(file_); ungetc(c, file_); return c;}
    void skipws();
    int file_no();
    std::FILE * c_stream();
    void flush() {fflush(file_);}
    void restart();
    bool getline(String & str) {return IStream::getline(str);}
    bool getline(String &, char d);
    bool read(char *, unsigned int i);
    void write(ParmString);
    void write(char c);
    void write(const char *, unsigned int i);

    FStream & operator>> (char & c)
    {
      skipws();
      c = getc(file_);
      return *this;
    }
    
    FStream & operator<< (char c)
    {
      putc(c, file_);
      return *this;
    }

    FStream & operator>> (String &);
    FStream & operator>> (unsigned int &);
    FStream & operator>> (int &);
    FStream & operator<< (ParmString);
    FStream & operator<< (unsigned int);
    FStream & operator<< (int);

  };
}

#endif
