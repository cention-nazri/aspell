// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#ifndef __aspeller_wordinfo__
#define __aspeller_wordinfo__

#include <assert.h>
#include "string.hpp"

namespace acommon {
  class OStream;
  class Convert;
}

namespace aspeller {

  using namespace acommon;

  class Language;
  struct ConvertWord;

  // WordEntry is an entry in the dictionary.  Both word and aff
  // should point to a string that will stay in memory as long as the
  // dictionary does unless under very special circumstances.  Thus
  // free_ should generally be null.
  struct WordEntry
  {
    const char * word;
    const char * aff;
    void (* adv_)(WordEntry *);
    void (* free_)(WordEntry *);
    void * intr[2];
    enum What {Other, Word, Soundslike, Stripped, Misspelled} what;
    // if type is Word than aff will be defined, otherwise it won't
    bool at_end() {return !word;}
    bool adv() {if (adv_) {adv_(this); return true;} word = 0; return false;}
    operator bool () const {return word != 0;}
    OStream & write(OStream & o, const Language & l,
		    const ConvertWord &, Convert * c = 0) const;
    WordEntry() : word(0), aff(0), adv_(0), free_(0){}
    void clear() {if (free_) free_(this); word = 0; aff = 0; adv_ = 0; free_ = 0;}
    ~WordEntry() {if (free_) free_(this);}
  };

  /*
    flags:
      1 bit:  case/accent info known
      1 bit:  all lower
      1 bit:  all upper
      1 bit:  title
      1 bit:  with accents

  */

}

#endif
