/* This file is part of The New Aspell
 * Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#ifndef ASPELL_DOCUMENT_CHECKER__HPP
#define ASPELL_DOCUMENT_CHECKER__HPP

#include "filter.hpp"
#include "char_vector.hpp"
#include "copy_ptr.hpp"
#include "can_have_error.hpp"

namespace acommon {

  class Config;
  class Speller;
  class Tokenizer;

  struct Token {
    unsigned int offset;
    unsigned int len;
  };
  
  
  class DocumentChecker : public CanHaveError {
  public:
    // will take ownership of tokenizer and filter.
    // config only used for this method.
    // speller expected to stick around.
    PosibErr<void> setup(Tokenizer *, Speller *, Config *, Filter *);
    void reset();
    void process(const char * str, int size);
    Token next_misspelling();
    
    Config * config() {return filter_->config();}
    Filter * filter() {return filter_;}
    
    DocumentChecker();
    ~DocumentChecker();
    
  private:
    CopyPtr<Filter> filter_;
    CopyPtr<Tokenizer> tokenizer_;
    Speller * speller_;
    CharVector proc_str_;
  };

  PosibErr<DocumentChecker *> new_document_checker(Speller *, Config *, Filter *);

}

#endif /* ASPELL_DOCUMENT_CHECKER__HPP */
