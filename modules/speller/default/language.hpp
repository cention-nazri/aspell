// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#ifndef ASPELLER_LANGUAGE__HPP
#define ASPELLER_LANGUAGE__HPP

#include "affix.hpp"
#include "cache.hpp"
#include "config.hpp"
#include "convert.hpp"
#include "phonetic.hpp"
#include "posib_err.hpp"
#include "stack_ptr.hpp"
#include "string.hpp"
#include "objstack.hpp"

using namespace acommon;

namespace acommon {
  struct CheckInfo;
}

namespace aspeller {

  struct SuggestRepl {
    const char * substr;
    const char * repl;
  };
  
  class SuggestReplEnumeration
  {
    const SuggestRepl * i_;
    const SuggestRepl * end_;
  public:
    SuggestReplEnumeration(const SuggestRepl * b, const SuggestRepl * e)
      : i_(b), end_(e) {}
    bool at_end() const {return i_ == end_;}
    const SuggestRepl * next() {
      if (i_ == end_) return 0;
      return i_++;
    }
  };

  enum CasePattern {Other, FirstUpper, AllUpper};

  class Language : public Cacheable {
  public:
    typedef const Config CacheConfig;
    typedef String       CacheKey;

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
    String   data_encoding_;

    ConvObj  mesg_conv_;
    ConvObj  to_utf8_;
    ConvObj  from_utf8_;

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
    StackPtr<Config>     lang_config_;

    StringBuffer buf_;
    Vector<SuggestRepl> repls_;

    Language(const Language &);
    void operator=(const Language &);

  public:

    Language() {}
    PosibErr<void> setup(const String & lang, const Config * config);
    void set_lang_defaults(Config & config);

    const char * data_dir() const {return dir_.c_str();}
    const char * name() const {return name_.c_str();}
    const char * charset() const {return charset_.c_str();}
    const char * data_encoding() const {return data_encoding_.c_str();}

    const Convert * mesg_conv() const {return mesg_conv_.ptr;}
    const Convert * to_utf8() const {return to_utf8_.ptr;}
    const Convert * from_utf8() const {return from_utf8_.ptr;}

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
  
    const char * to_soundslike(String & res, ParmString word) const {
      res.resize(word.size());
      char * e = soundslike_->to_soundslike(res.data(), word.str(), word.size());
      res.resize(e - res.data());
      return res.str();
    }

    const char * soundslike_name() const {
      return soundslike_->name();
    }

    const char * soundslike_version() const {
      return soundslike_->version();
    }
    
    const char * soundslike_chars() const {return soundslike_chars_.c_str();}
    const char * stripped_chars() const {return stripped_chars_.c_str();}

    bool have_soundslike() const {return soundslike_;}

    const AffixMgr * affix() const {return affix_;}

    SuggestReplEnumeration * repl() const {
      return new SuggestReplEnumeration(repls_.pbegin(), repls_.pend());}

    char * to_soundslike(char * res, const char * str, int len = -1) const 
    { return soundslike_->to_soundslike(res,str,len);}
    
    char * to_lower(char * res, const char * str) const {
      while (*str) *res++ = to_lower(*str++); *res = '\0'; return res;}
    char * to_upper(char * res, const char * str) const {
      while (*str) *res++ = to_upper(*str++); *res = '\0'; return res;}
    char * to_stripped(char * res, const char * str) const {
      for (; *str; ++str) {
        if (special(*str).any()) ++str;
        *res++ = to_stripped(*str);
      }
      *res = '\0';
      return res;
    }

    const char * to_lower(String & res, const char * str) const {
      res.clear(); while (*str) res += to_lower(*str++); return res.str();}
    const char * to_upper(String & res, const char * str) const {
      res.clear(); while (*str) res += to_upper(*str++); return res.str();}
    const char * to_stripped(String & res, const char * str) const {
      res.clear();
      for (; *str; ++str) {
        if (special(*str).any()) ++str;
        res += to_stripped(*str);
      }
      return res.str();
    }

    bool is_lower(const char * str) const {
      while (*str) {if (!is_lower(*str++)) return false;} return true;}
    bool is_upper(const char * str) const {
      while (*str) {if (!is_upper(*str++)) return false;} return true;}
    bool is_stripped(const char * str) const {
      while (*str) {if (!is_stripped(*str++)) return false;} return true;}

    CasePattern case_pattern(ParmString word) const  
    {
      if (is_upper(word))
        return AllUpper;
      else if (!is_lower(word[0]))
        return FirstUpper;
      else
        return Other;
    }
    
    void fix_case(CasePattern case_pattern,
                  char * res, const char * str) const 
    {
      if (!str[0]) return;
      if (case_pattern == AllUpper) {
        to_upper(res,str);
      } if (case_pattern == FirstUpper && is_lower(str[0])) {
        *res = to_title(str[0]);
        if (res == str) return;
        res++;
        str++;
        while (*str) *res++ = *str++;
        *res = '\0';
      } else {
        if (res == str) return;
        while (*str) *res++ = *str++;
        *res = '\0';
      }
    }

    const char * fix_case(CasePattern case_pattern, const char * str,
                          String buf) const 
    {
      if (!str[0]) return str;
      if (case_pattern == AllUpper) {
        to_upper(buf,str);
        return buf.str();
      } if (case_pattern == FirstUpper && is_lower(str[0])) {
        buf.clear();
        buf += to_title(str[0]);
        str++;
        while (*str) buf += *str++;
        return buf.str();
      } else {
        return str;
      }
    }

    static inline PosibErr<Language *> get_new(const String & lang, const Config * config) {
      StackPtr<Language> l(new Language());
      RET_ON_ERR(l->setup(lang, config));
      return l.release();
    }

    bool cache_key_eq(const String & l) const  {return name_ == l;}
  };

  typedef Language LangImpl;

  struct MsgConv : public ConvP
  {
    MsgConv(const Language * l) : ConvP(l->mesg_conv()) {}
    MsgConv(const Language & l) : ConvP(l.mesg_conv()) {}
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
    void convert(ParmString in, char * out) const
    {
      if (!strip_accents) {
        memcpy(out, in, in.size() + 1);
      } else {
        unsigned int i = 0;
	for (; i != in.size(); ++i)
	  out[i] = lang->de_accent(in[i]);
        out[i] = '\0';
      }
    }
  };

  String get_stripped_chars(const Language & l);
  
  PosibErr<void> check_if_valid(const Language & l, ParmString word);

  PosibErr<Language *> new_language(const Config &, ParmString lang = 0);

  PosibErr<void> open_affix_file(const Config &, FStream & o);
  
}


#endif
