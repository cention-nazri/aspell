// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "posib_err.hpp"

namespace acommon {

  String & String::operator= (const PosibErr<String> & s)
  {
    std::string::operator=(s.data);
    return *this;
  }

  struct StrSize {
    const char * str; 
    unsigned int size; 
    StrSize() : str(0), size(0) {}
    void operator= (ParmString s) {str = s; size = s.size();}
  };

  PosibErrBase & PosibErrBase::set(const ErrorInfo * inf,
				   ParmString p1, ParmString p2, 
				   ParmString p3, ParmString p4)
  {
    const char * s0 = inf->mesg ? inf->mesg : "";
    const char * s;
    ParmString p[4] = {p1,p2,p3,p4};
    StrSize m[10];
    unsigned int i = 0;
    while (i != 4 && p[i] != 0) 
      ++i;
    assert(i == inf->num_parms || i == inf->num_parms + 1);
    i = 0;
    while (true) {
      s = s0 + strcspn(s0, "%");
      m[i].str = s0;
      m[i].size = s - s0;
      if (*s == '\0') break;
      ++i;
      s = strchr(s, ':') + 1;
      unsigned int ip = *s - '0' - 1;
      assert(0 <= ip && ip < inf->num_parms);
      m[i] = p[ip];
          ++i;
      s0 = s+1;
    }
    if (!p[inf->num_parms].empty()) {
      m[++i] = " ";
      m[++i] = p[inf->num_parms];
    }
    unsigned int size = 0;
    for (i = 0; m[i].str != 0; ++i)
      size += m[i].size;
    char * str = new char[size + 1];
    s0 = str;
    for (i = 0; m[i].str != 0; str+=m[i].size, ++i)
      strncpy(str, m[i].str, m[i].size);
    *str = '\0';
    Error * e = new Error;
    e->err = inf;
    e->mesg = s0;
    err_ = new ErrPtr(e);
    return *this;
  }

  PosibErrBase & PosibErrBase::with_file(ParmString fn)
  {
    assert(err_ != 0);
    assert(err_->refcount == 1);
    char * & m = const_cast<char *>(err_->err->mesg);
    unsigned int orig_len = strlen(m);
    unsigned int new_len = fn.size() + 2 + orig_len + 1;
    char * s = new char[new_len];
    char * p = s;
    memcpy(p, fn.str(), fn.size());
    p += fn.size();
    memcpy(p, ": ", 2);
    p += 2;
    memcpy(p, m, orig_len + 1);
    delete[] m;
    m = s;
    return *this;
  }
  
  void PosibErrBase::handle_err() const {
    assert (err_);
    assert (!err_->handled);
    fputs("Unhandled Error: ", stderr);
    fputs(err_->err->mesg, stderr);
    fputs("\n", stderr);
    abort();
  }

  void PosibErrBase::handle_incompat_assign() const {
    fputs("Incompatible Assignment of PosibErr.\n", stderr);
    abort();
  }

  Error * PosibErrBase::release() {
    assert (err_);
    assert (err_->refcount <= 1);
    Error * tmp = const_cast<Error *>(err_->err);
    delete err_;
    err_ = 0;
    return tmp;
  }

  void PosibErrBase::del() {
    const Error * e = release();
    delete e;
  }

}
