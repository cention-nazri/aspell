/* This file is part of The New Aspell
 * Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#include "document_checker.hpp"
#include "tokenizer.hpp"
#include "speller.hpp"
#include "config.hpp"
#include "copy_ptr-t.hpp"

#include "iostream.hpp"

namespace acommon {

  DocumentChecker::DocumentChecker() {}
  DocumentChecker::~DocumentChecker() {}


  PosibErr<void> DocumentChecker
  ::setup(Tokenizer * tokenizer, Speller * speller, 
	  Config * config, Filter * filter)
  {
    tokenizer_.reset(tokenizer);
    speller_ = speller;
    return no_err;
  }

  void DocumentChecker::reset()
  {
    
  }

  void DocumentChecker::process(const char * str, int size)
  {
    proc_str_.clear();
    if (size == -1)
      proc_str_.write(str);
    else
      proc_str_.write(str, size);
    proc_str_ << '\0';
    tokenizer_->reset(proc_str_.data());
  }

  Token DocumentChecker::next_misspelling()
  {
    bool correct;
    do {
      if (!tokenizer_->advance()) break;
      //COUT << ":: \"";
      //COUT.write(tokenizer_->begin, tokenizer_->end - tokenizer_->begin);
      //COUT << ":" << tokenizer_->word.data() 
      //     << '(' << tokenizer_->word.size() << ')';
      correct = speller_->check(MutableString(tokenizer_->word.data(),
					      tokenizer_->word.size() - 1));
      //COUT << "\" is " << (correct ? "correct" : "incorrect") << "\n";
    } while (correct);
    Token tok;
    tok.len  = tokenizer_->end - tokenizer_->begin;
    tok.offset = tok.len > 0 ? tokenizer_->begin - proc_str_.data() : proc_str_.size();
    return tok;
  }

}

