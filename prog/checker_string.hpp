// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include <stdio.h>

#include "vector.hpp"
#include "char_vector.hpp"
#include "document_checker.hpp"

using namespace acommon;

class CheckerString {
private:

  typedef Vector<CharVector> Lines;

public:

  class Iterator;
  friend class Iterator;

  class Iterator {
    friend class CheckerString;
    friend int dist(Iterator, Iterator);
    
    CheckerString * cs_;
    Lines::iterator line_;
    CharVector::iterator i_;
    
    Iterator(CheckerString * cs, Lines::iterator l, CharVector::iterator i)
      : cs_(cs), line_(l), i_(i) {}
    
  public:

    Iterator() {}

    void operator-- () {
      if (i_ == line_->begin()) {cs_->dec(line_); i_ = line_->end();}
      --i_;
    }
    // NOTE: the increment operator has the potential to invalidate 
    //       another iterator if the other iterator is more than 
    //       "lines" lines apart (as given by the constructor)
    void operator++ () {
      ++i_;
      if (i_ == line_->end()) {cs_->next_line(line_); i_ = line_->begin();}
    }
    char operator* () const {return *i_;}
    bool off_end () const {return cs_->off_end(line_);}
    bool equal(Iterator o) {return line_ == o.line_ && i_ == o.i_;}
  };

  friend int dist(Iterator, Iterator);

  Iterator word_begin() 
    {return Iterator(this, cur_line_, word_begin_);}
  Iterator word_end()   
    {return Iterator(this, cur_line_, word_begin_ + word_size_);}
  int word_size() {return word_size_;}

  CheckerString(Speller * speller, FILE * in, FILE * out, int lines);
  ~CheckerString();

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

  Lines::iterator first_line() {
    Lines::iterator i = end_;
    inc(i);
    return i;
  }

  bool read_next_line();
  
  FILE * in_;
  FILE * out_;

  CopyPtr<DocumentChecker> checker_;
  Speller * speller_;
  Lines::iterator end_;
  
  Lines::iterator cur_line_;
  int diff_;
  Token tok_;
  CharVector::iterator word_begin_;
  int word_size_;
  bool has_repl_;
  Lines lines_;

};

static inline bool operator== (CheckerString::Iterator lhs, 
			       CheckerString::Iterator rhs) 
{
  return lhs.equal(rhs);
}
static inline bool operator!= (CheckerString::Iterator lhs, 
			       CheckerString::Iterator rhs) 
{
  return !lhs.equal(rhs);

}
int dist(CheckerString::Iterator smaller,
	 CheckerString::Iterator larger);

