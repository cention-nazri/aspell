// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.

#include "speller.hpp"
#include "document_checker.hpp"
#include "stack_ptr.hpp"
#include "convert.hpp"
#include "tokenizer.hpp"

namespace acommon {

  PosibErr<DocumentChecker *> 
  new_document_checker(Speller * speller, Config * config, Filter * filter)
  {
    StackPtr<DocumentChecker> checker(new DocumentChecker());
    Tokenizer * tokenizer = new_tokenizer(speller, config);
    if (!filter) // FIXME deal with error and avoid memory leaks
      filter = new_filter(speller, config);
    RET_ON_ERR(checker->setup(tokenizer, speller, config, filter));
    return checker.release();
  }

}
