
#include <stdio.h>
#include <assert.h>
#include <ctype.h>

#include "string.hpp"
#include "fstream.hpp"
#include "errors.hpp"

namespace pcommon {

  PosibErr<void> FStream::open(ParmString name, const char * mode)
  {
    assert (file_ == 0);
    file_ = fopen(name,mode);
    if (file_ == 0) {
      if (strpbrk(mode, "wa+") != 0)
	return make_err(cant_write_file, name);
      else
	return make_err(cant_read_file, name);
    } else {
      return no_err;
    }
  }

  void FStream::close()
  {
    if (file_ != 0)
      fclose(file_);
    file_ = 0;
  }

  int FStream::file_no() 
  {
    return fileno(file_);
  }

  FILE * FStream::c_stream() 
  {
    return file_;
  }

  void FStream::restart()
  {
    flush();
    fseek(file_,0,SEEK_SET);
  }

  void FStream::skipws() 
  {
    int c;
    while (c = getc(file_), c != EOF && isspace(c));
    ungetc(c, file_);
  }

  FStream & FStream::operator>> (String & str)
  {
    skipws();
    int c;
    str = "";
    while (c = getc(file_), c != EOF && !isspace(c))
      str += static_cast<char>(c);
    ungetc(c, file_);
    return *this;
  }

  FStream & FStream::operator<< (ParmString str)
  {
    fputs(str, file_);
    return *this;
  }

  bool FStream::getline(String & str, char delem)
  {
    str.clear();
    int c;
    bool prev_slash = false;
    while (true) {
      c  = fgetc(file_);
      if (c == EOF || (!prev_slash && static_cast<char>(c) == delem)) break;
      str += static_cast<char>(c);
      prev_slash = c == '\\';
    }
    if (c == EOF && str.size() == 0) return false;
    return true;
  }

  bool FStream::read(char * str, unsigned int n)
  {
    fread(str,1,n,file_);
    return operator bool();
  }

  void FStream::write(char c)
  {
    putc(c, file_);
  }

  void FStream::write(ParmString str) 
  {
    fputs(str, file_);
  }

  void FStream::write(const char * str, unsigned int n)
  {
    fwrite(str,1,n,file_);
  }

  FStream & FStream::operator>> (unsigned int & num)
  {
    int r = fscanf(file_, " %u", &num);
    if (r != 1)
      close();
    return *this;
  }


  FStream & FStream::operator<< (unsigned int num)
  {
    fprintf(file_, "%u", num);
    return *this;
  }

  FStream & FStream::operator>> (int & num)
  {
    int r = fscanf(file_, " %i", &num);
    if (r != 1)
      close();
    return *this;
  }


  FStream & FStream::operator<< (int num)
  {
    fprintf(file_, "%i", num);
    return *this;
  }

}
