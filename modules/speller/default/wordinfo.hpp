// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#ifndef __aspeller_wordinfo__
#define __aspeller_wordinfo__

#include <assert.h>
#include "string.hpp"

namespace acommon {
  class OStream;
}

namespace aspeller {

  using namespace acommon;

  class Language;
  struct ConvertWord;

  struct CompoundInfo {
    unsigned char d;
    
    CompoundInfo(unsigned char d0 = 0) : d(d0) {}
    
    unsigned int mid_char()       const {return d & (1<<0|1<<1);}
    
    void mid_char(unsigned int c) {
      assert(c < 4);
      d |= c;
    }
    
    bool mid_required ()       const {return d & 1<<2; }
    void mid_required (bool c)       { d |= c<<2;}
    
    bool beg () const {return d & 1<<3;}
    void beg (bool c) {d |= c<<3;}

    bool mid () const {return d & 1<<4;}
    void mid (bool c) {d |= c<<4;}

    bool end () const {return d & 1<<5;}
    void end (bool c) {d |= c<<5;}

    bool any() const {return d & (1<<3|1<<4|1<<5);}

    const char * read(const char * str, const Language & l);
    OStream & write(OStream &, const Language & l) const;

    enum Position {Orig, Beg, Mid, End};

    bool compatible(Position pos);
  };

  CompoundInfo::Position 
  new_position(CompoundInfo::Position unsplit_word, 
	       CompoundInfo::Position pos);
  
  struct SoundslikeWord {
    const char * soundslike;
    const void * word_list_pointer;
    
    operator bool () const {return soundslike;}
  
    SoundslikeWord() : soundslike(0) {}
    SoundslikeWord(const char * w, const void * p = 0) 
      : soundslike(w == 0 || w[0] == 0 ? 0 : w), word_list_pointer(p) {}
  };

  static const unsigned int MaxCompoundLength = 8;

  struct BasicWordInfo {
    const char * word;
    const char * affixes;
    void * buffer;
    CompoundInfo compound; 
    BasicWordInfo(const char * w = 0, CompoundInfo c = 0)
      : word(w), affixes(""), buffer(0), compound(c) {}
    BasicWordInfo(const char * w, const char * a, CompoundInfo c = 0)
      : word(w), affixes(a), buffer(0), compound(c) {}
    ~BasicWordInfo() {if (buffer) free(buffer);}
    //operator const char * () const {return word;}
    operator bool () const {return word != 0;}
    inline void get_word(String &, const ConvertWord &);
    OStream & write(OStream & o, const Language & l,
		    const ConvertWord &) const;
  };

/*
  struct SingleWordInfo {
    const char * word;
    char middle_char;
    SingleWordInfo(const char * w = 0, char mc = '\0')
      : word(w), middle_char(mc) {}
    void clear() {word = 0;}
    void set(const char * w, char mc = '\0') {word = w; middle_char = mc;}
    //void append_word(String & word, const Language & l, 
    //	     const ConvertWord &) const;
    operator bool() const {return word != 0;}
  };

  struct WordInfo {
    SingleWordInfo words[MaxCompoundLength + 1];
    void get_word(String & word, const Language & l, 
		  const ConvertWord &) const;
    operator bool () const {return words[0].word != 0;}
    OStream & write(OStream & o, const Language & l, 
		    const ConvertWord &) const;
  };
*/
  
}

#endif
