// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include <stdio.h>

#include "aspell.h"

#include "vector.hpp"
#include "char_vector.hpp"
#include "document_checker.hpp"

using namespace acommon;

class CheckerString {
public:

  typedef Vector<CharVector> Lines;
  CheckerString(AspellSpeller * speller, FILE * in, FILE * out, int lines);
  ~CheckerString();

private:
  void inc(Lines::iterator & i) {
    ++i;
    if (i == lines_.end())
      i = lines_.begin();
  }
  void dec(Lines::iterator & i) {
    if (i == lines_.begin())
      i = lines_.end();
    --i;
  }
  void next_line(Lines::iterator & i) {
    inc(i);
    if (i == end_)
      read_next_line();
  }
  bool off_end(Lines::iterator i) {
    return i == end_;
  }
public:

  class LineIterator {
  public:
    CheckerString * cs_;
    Lines::iterator line_;

    CharVector * operator-> () {return &*line_;}

    void operator-- () {cs_->dec(line_);}
    void operator++ () {cs_->next_line(line_);}
    bool off_end () const {return cs_->off_end(line_);}
    
    LineIterator() {}

    LineIterator(CheckerString * cs, Lines::iterator l) : cs_(cs), line_(l) {}
  };

  LineIterator cur_line() {return LineIterator(this, cur_line_);}

  const char * word_begin() {return &*word_begin_;}
  const char * word_end()   {return &*word_begin_ + word_size_;}
  size_t word_size()        {return word_size_;}

  bool next_misspelling();
  void replace(ParmString repl);

  char * get_word(String & w) {
    w.clear();
    w.insert(w.end(), word_begin_, word_begin_ + word_size_);
    return w.mstr();
  }

private:
  Lines::iterator first_line() {
    Lines::iterator i = end_;
    inc(i);
    return i;
  }

  bool read_next_line();

  Lines::iterator cur_line_;
  Lines lines_;

  CharVector::iterator word_begin_;
  int word_size_;
  
  FILE * in_;
  FILE * out_;

  CopyPtr<DocumentChecker> checker_;
  AspellSpeller * speller_;
  Lines::iterator end_;
  int diff_;
  Token tok_;
  bool has_repl_;
};


typedef CheckerString::LineIterator LineIterator;
