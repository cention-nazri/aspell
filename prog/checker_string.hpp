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

  Lines::iterator cur_line_;
  CharVector::iterator word_begin_;
  int word_size_;
  Lines lines_;

  bool next_misspelling();
  void replace(ParmString repl);

  char * get_word(CharVector & w) {
    w.resize(0);
    w.insert(w.end(), word_begin_, word_begin_ + word_size_);
    w.push_back('\0');
    return w.data();
  }

private:
  void init(int);

  void inc(Lines::iterator & i) {
    ++i;
    if (i == lines_.end())
      i = lines_.begin();
  }
  void next_line(Lines::iterator & i) {
    inc(i);
    if (i == end_)
      read_next_line();
  }
  bool off_end(Lines::iterator i) {
    return i == end_;
  }

  Lines::iterator first_line() {
    Lines::iterator i = end_;
    inc(i);
    return i;
  }

  bool read_next_line();
  
  FILE * in_;
  FILE * out_;

  CopyPtr<DocumentChecker> checker_;
  AspellSpeller * speller_;
  Lines::iterator end_;
  int diff_;
  Token tok_;
  bool has_repl_;
};


