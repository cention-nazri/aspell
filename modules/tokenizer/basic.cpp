
// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "tokenizer.hpp"
#include "convert.hpp"
#include "speller.hpp"

namespace acommon {

  class TokenizerBasic : public Tokenizer
  {
  public:
    bool advance();
  };

#define increment__ \
  do { \
    prev = cur; \
    cur = next; \
    res = to_encoded_->convert_next_char(next,word); /* advances next */\
    if (!res) word.append('\0'); /* to avoid special cases at the end */\
    ++p; \
  } while (false)

  bool TokenizerBasic::advance() {
    begin = end;
    bool res;
    const char * prev = begin;
    const char * cur;
    const char * next;
    int p = 0;
    word.clear();

    // get the first 2 characters 
    cur = prev;
    res = to_encoded_->convert_next_char(cur,word);
    if (!res) return false;
    next = cur;
    res = to_encoded_->convert_next_char(next,word);
    if (!res) word.append('\0');

    // skip spaces (non-word characters)
    while (word[p] != '\0' &&
	   !(is_word(word[p])
	     || (is_begin(word[p]) && is_word(word[p+1]))))
      increment__;

    // remove the trailing space from the word
    p = 0;
    begin = end = prev;
    word.erase(word.begin(), word.end() - 2);

    if (word[0] == '\0') return false;

    if (is_begin(word[p]) && is_word(word[p+1]))
      increment__;

    while (is_word(word[p]) || 
	   (is_middle(word[p]) && 
	    p > 0 && is_word(word[p-1]) &&
	    is_word(word[p+1]) )) 
      increment__;

    if (is_end(word[p]))
      increment__;

    word.resize(word.size() - 2); 
    word.append('\0');
    end = prev;

    return true;
  }
#undef increment__

  PosibErr<Tokenizer *> new_tokenizer(Speller * speller, Config *)
  {
    Tokenizer * tok = new TokenizerBasic();
    speller->setup_tokenizer(tok);
    return tok;
  }

}
