// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#ifndef __aspeller_language__
#define __aspeller_language__

#include "affix.hpp"
#include "string.hpp"
#include "posib_err.hpp"
#include "stack_ptr.hpp"
#include "cache.hpp"

#include "phonetic.hpp"

using namespace acommon;

namespace acommon {
  class Config;
  struct CheckInfo;
}

namespace aspeller {

  class Language : public Cacheable {
  public:
    typedef Config  CacheConfig;
    typedef String  CacheKey;

    enum CharType {letter, space, other};

    struct SpecialChar {
      bool begin;
      bool middle;
      bool end;
      bool any() const {return begin || middle || end;}
      SpecialChar() : begin(false), middle(false), end(false) {}
      SpecialChar(bool b, bool m, bool e) : begin(b), middle(m), end(e) {}
    };

  private:
    String   dir_;
    String   name_;
    String   charset_;
    String   mid_chars_;

    unsigned char to_uchar(char c) const {return static_cast<unsigned char>(c);}

    SpecialChar special_[256];
    char          to_lower_[256];
    char          to_upper_[256];
    char          to_title_[256];
    char          to_stripped_[256];
    unsigned char to_normalized_[256];
    char          de_accent_[256];
    char          to_sl_[256];
    int           to_uni_[256];
    CharType      char_type_[256];

    int max_normalized_;

    String      soundslike_chars_;
    String      stripped_chars_;

    StackPtr<Soundslike> soundslike_;
    StackPtr<AffixMgr>   affix_;

    bool affix_compress_;

    Language(const Language &);
    void operator=(const Language &);

  public:
    Language() {}
    PosibErr<void> setup(const String & lang, Config * config);

    const char * data_dir() const {return dir_.c_str();}
    const char * name() const {return name_.c_str();}
    const char * charset() const {return charset_.c_str();}
    const char * mid_chars() const {return mid_chars_.c_str();}

    char to_upper(char c) const {return to_upper_[to_uchar(c)];}
    bool is_upper(char c) const {return to_upper(c) == c;}

    char to_lower(char c) const {return to_lower_[to_uchar(c)];}
    bool is_lower(char c) const {return to_lower(c) == c;}

    char to_title(char c) const {return to_title_[to_uchar(c)];}
    bool is_title(char c) const {return to_title(c) == c;}

    char to_stripped(char c) const {return to_stripped_[to_uchar(c)];}
    bool is_stripped(char c) const {return to_stripped(c) == c;}

    char to_normalized(char c) const {return to_normalized_[to_uchar(c)];}
    unsigned char max_normalized() const {return max_normalized_;}
    
    char de_accent(char c) const {return de_accent_[to_uchar(c)];}

    char to_sl(char c) const {return to_sl_[to_uchar(c)];}
  
    int to_uni(char c) const {return to_uni_[to_uchar(c)];}
  
    SpecialChar special(char c) const {return special_[to_uchar(c)];}
  
    CharType char_type(char c) const {return char_type_[to_uchar(c)];}
    bool is_alpha(char c) const {return char_type(c) == letter;}
  
    String to_soundslike(ParmString word) const {
      return soundslike_->to_soundslike(word);
    }

    const char * soundslike_name() const {
      return soundslike_->name();
    }

    const char * soundslike_version() const {
      return soundslike_->version();
    }
    
    bool have_phoneme() const {return false;}
    String to_phoneme(const char * word) const 
    {
      return "";
    }
    String to_phoneme(ParmString word) const 
    {
      return "";
    }

    const char * soundslike_chars() const {return soundslike_chars_.c_str();}
    const char * stripped_chars() const {return stripped_chars_.c_str();}

    const AffixMgr * affix() const {return affix_;}

    bool affix_compress() const {return affix_compress_;}

    static inline PosibErr<Language *> get_new(const String & lang, Config * config) {
      StackPtr<Language> l(new Language());
      RET_ON_ERR(l->setup(lang, config));
      return l.release();
    }

    bool cache_key_eq(const String & l) const  {return name_ == l;}
  };

  struct InsensitiveCompare {
    // compares to strings without regards to casing or special characters
    const Language * lang;
    InsensitiveCompare(const Language * l = 0) : lang(l) {}
    operator bool () const {return lang;}
    int operator() (const char * a, const char * b) const
    { 
      if (lang->special(*a).begin) ++a;
      if (lang->special(*b).begin) ++b;
      while (*a != '\0' && *b != '\0' 
	     && lang->to_stripped(*a) == lang->to_stripped(*b)) 
      {
	++a, ++b;
	if (lang->special(*a).middle) ++a;
	if (lang->special(*b).middle) ++b;
      }
      if (lang->special(*a).end) ++a;
      if (lang->special(*b).end) ++b;
      return static_cast<unsigned char>(lang->to_stripped(*a)) 
	- static_cast<unsigned char>(lang->to_stripped(*b));
    }
  };

  struct InsensitiveEqual {
    InsensitiveCompare cmp;
    InsensitiveEqual(const Language * l = 0) : cmp(l) {}
    bool operator() (const char * a, const char * b) const
    {
      return cmp(a,b) == 0;
    }
  };
  
  struct InsensitiveHash {
    // hashes a string without regards to casing or special begin
    // or end characters
    const Language * lang;
    InsensitiveHash() {}
    InsensitiveHash(const Language * l)
	: lang(l) {}
    size_t operator() (const char * s) const
    {
      size_t h = 0;
      for (;;) {
	if (lang->special(*s).any()) ++s;
	if (*s == 0) break;
	if (lang->char_type(*s) == Language::letter)
	  h=5*h + lang->to_stripped(*s);
	++s;
      }
      return h;
    }
  };

  struct SensitiveCompare {
    const Language * lang;
    bool case_insensitive;
    bool ignore_accents;
    bool strip_accents;
    SensitiveCompare(const Language * l = 0) 
      : lang(l), case_insensitive(false)
      , ignore_accents(false), strip_accents(false) {}
    bool operator() (const char * word, const char * inlist) const;
    // Pre:
    //   word == to_find as given by Language::InsensitiveEqual 
    //   both word and inlist contain at least one letter as given by
    //     lang->char_type
    // Rules:
    //   if begin inlist is a begin char then it must match begin word
    //   if end   inlist is a end   char then it must match end word
    //   chop all begin/end chars from the begin/end of word and inlist
    //  unless ignore_accents
    //   accents must match
    //  unless case_insensitive
    //   (note: there are 3 posssible casings lower, upper and title)
    //   if is lower begin inlist then begin word can be any casing
    //   if not                   then begin word must be the same case
    //   if word is all upper than casing of inlist can be anything
    //   otherwise the casing of tail begin and tail inlist must match
  };

  struct ConvertWord {
    const Language * lang;
    bool strip_accents;
    ConvertWord(const Language * l = 0)
      : lang(l), strip_accents(false) {}
    void convert(ParmString in, String & out) const
    {
      if (!strip_accents) {
	out += in;
      } else {
	for (unsigned int i = 0; i != in.size(); ++i)
	  out += lang->de_accent(in[i]);
      }
    }
  };

  inline String to_lower(const Language & l, ParmString word) 
  {
    String new_word; 
    for (const char * i = word; *i; ++i) 
      new_word += l.to_lower(*i); 
    return new_word;
  }
  
  inline bool is_lower(const Language & l, ParmString word)
  {
    for (const char * i = word; *i; ++i) 
      if (!l.is_lower(*i))
	return false;
    return true;
  }

  template <class Str>
  inline void to_stripped(const Language & l, ParmString word, Str & new_word)
  {
    for (const char * i = word; *i; ++i) {
      if (l.special(*i).any()) ++i;
      new_word.push_back(l.to_stripped(*i));
    }
  }

  inline String to_stripped(const Language & l, ParmString word) 
  {
    String new_word;
    to_stripped(l, word, new_word);
    return new_word;
  }

  inline bool is_stripped(const Language & l, ParmString word)
  {
    for (const char * i = word; *i; ++i) 
      if (!l.is_stripped(*i))
	return false;
    return true;
  }
  
  inline String to_upper(const Language & l, ParmString word) 
  {
    String new_word; 
    for (const char * i = word; *i; ++i) 
      new_word += l.to_upper(*i); 
    return new_word;
  }
  
  inline bool is_upper(const Language & l, ParmString word)
  {
    for (const char * i = word; *i; ++i) 
      if (!l.is_upper(*i))
	return false;
    return true;
  }
  
  enum CasePattern {Other, FirstUpper, AllUpper};

  inline CasePattern case_pattern(const Language & l, ParmString word) 
  {
    if (is_upper(l,word))
      return AllUpper;
    else if (!l.is_lower(word[0]))
      return FirstUpper;
    else
      return Other;
  }

  inline String fix_case(const Language & l, 
			 CasePattern case_pattern,
			 ParmString word)
  {
    if (word.empty()) return word;
    if (case_pattern == AllUpper) {
      return to_upper(l,word);
    } else if (case_pattern == FirstUpper) {
      String new_word;
      if (l.is_lower(word[0]))
	new_word += l.to_title(word[0]);
      else
	new_word += word[0];
      new_word.append(word + 1);
      return new_word;
    } else {
      return word;
    }
  }
  
  PosibErr<void> check_if_valid(const Language & l, ParmString word);

  void normalize_mid_characters(const Language & l, String & s);

  PosibErr<Language *> new_language(Config &, ParmString lang = 0);
  
}


#endif
