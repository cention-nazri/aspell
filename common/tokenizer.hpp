// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ACOMMON_TOKENIZER__HPP
#define ACOMMON_TOKENIZER__HPP

#include "char_vector.hpp"

namespace acommon {

  class Convert;
  class Speller;
  class Config;

  class Tokenizer {

  public:
    Tokenizer() : begin(0), end(0), to_encoded_(0) {}
    virtual ~Tokenizer() {}

    CharVector word; // this word is in the final encoded form
    const char * begin; // pointers back to the orignal word
    const char * end;
    
    // the string is expected to be null terminated
    void reset (const char * i) {end = i;}
    bool at_end() const {return begin == end; /* == 0 */}
    
    virtual bool advance() = 0; // returns false if there is nothing left

    bool is_begin(char c) const
      {return char_type_[static_cast<unsigned char>(c)].begin;}
    bool is_middle(char c) const
      {return char_type_[static_cast<unsigned char>(c)].middle;}
    bool is_end(char c) const
      {return char_type_[static_cast<unsigned char>(c)].end;}
    bool is_word(char c) const
      {return char_type_[static_cast<unsigned char>(c)].word;}


  public: // but don't use
    // The speller class is expected to fill these members in
    struct CharType {
      bool begin;
      bool middle;
      bool end;
      bool word;
      CharType() : begin(false), middle(false), end(false), word(false) {}
    };
    
    CharType char_type_[256];
    Convert * to_encoded_;
  };

  // returns a new tokenizer and sets it up with the given speller
  // class

  PosibErr<Tokenizer *> new_tokenizer(Speller *, Config *);

}

#endif
