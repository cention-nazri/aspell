// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include <stdio.h>

#include "aspell.h"

#include "vector.hpp"
#include "string.hpp"
#include "simple_string.hpp"
#include "checker.hpp"

using namespace acommon;

class CheckerString {
public:

  struct Line {
    String       real;
    SimpleString disp;
    String       buf;
    void clear() {real.clear(); disp.str = ""; disp.size = 0; buf.clear();}
  };

  typedef Vector<Line> Lines;
  CheckerString(AspellSpeller * speller, FILE * in, FILE * out, int lines);
  ~CheckerString();

public: // but don't use
  void inc(Line * & i) {
    ++i;
    if (i == lines_.pend())
      i = lines_.pbegin();
  }
  void dec(Line * & i) {
    if (i == lines_.pbegin())
      i = lines_.pend();
    --i;
  }
  void next_line(Line * & i) {
    inc(i);
    if (i == end_)
      read_next_line();
  }
  bool off_end(Line * i) {
    return i == end_;
  }
public:

  class LineIterator {
  public:
    CheckerString * cs_;
    Line * line_;

    SimpleString * operator-> () {return &line_->disp;}

    Line & get() {return *line_;}

    void operator-- () {cs_->dec(line_);}
    void operator++ () {cs_->next_line(line_);}
    bool off_end () const {return cs_->off_end(line_);}

    LineIterator() {}

    LineIterator(CheckerString * cs, Line * l) : cs_(cs), line_(l) {}
  };

  LineIterator cur_line() {return LineIterator(this, cur_line_);}

  const char * word_begin() {return disp_word_begin_;}
  const char * word_end()   {return disp_word_begin_ + disp_word_size_;}
  size_t word_size()        {return disp_word_size_;}

  bool next_misspelling();
  void replace(ParmString repl); // encoded in "real" encoding

  char * get_real_word(String & w) {
    w.clear();
    w.insert(w.end(), real_word_begin_, real_word_begin_ + real_word_size_);
    return w.mstr();
  }

private:
  Line * first_line() {
    Line * i = end_;
    inc(i);
    return i;
  }

  bool read_next_line();

  Line * cur_line_;
  Lines lines_;

  String::iterator real_word_begin_;
  int real_word_size_;
  const char * disp_word_begin_;
  int disp_word_size_;
  
  FILE * in_;
  FILE * out_;

  CopyPtr<Checker> checker_;
  AspellSpeller * speller_;
  Line * end_;

  void fix_display_str();

  static void checker_callback(void *, void *);
};


typedef CheckerString::LineIterator LineIterator;
