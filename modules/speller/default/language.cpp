// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#include "settings.h"

#include <vector>
#include <assert.h>

#include <iostream.hpp>

#include "asc_ctype.hpp"
#include "clone_ptr-t.hpp"
#include "config.hpp"
#include "enumeration.hpp"
#include "errors.hpp"
#include "file_data_util.hpp"
#include "fstream.hpp"
#include "language.hpp"
#include "string.hpp"
#include "cache-t.hpp"
#include "getdata.hpp"
#include "file_util.hpp"

#ifdef ENABLE_NLS
#  include <langinfo.h>
#endif

namespace aspeller {

  static const char TO_CHAR_TYPE[256] = {
    // 1  2  3  4  5  6  7  8  9  A  B  C  D  E  F 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 1
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 2
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 3
    0, 4, 0, 0, 3, 0, 0, 0, 2, 0, 0, 0, 6, 5, 0, 0, // 4
    0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, // 5
    0, 4, 0, 0, 3, 0, 0, 0, 2, 0, 0, 0, 6, 5, 0, 0, // 6
    0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, // 7
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 8
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 9
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // A
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // B
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // C
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // D
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // E
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  // F
  };

  static const int FOR_CONFIG = 1;

  static const KeyInfo lang_config_keys[] = {
    {"charset",             KeyInfoString, "iso-8859-1", ""}
    , {"data-encoding",       KeyInfoString, "<charset>", ""}
    , {"name",                KeyInfoString, "", ""}
    , {"run-together",        KeyInfoBool,   "", "", 0, FOR_CONFIG}
    , {"run-together-limit",  KeyInfoInt,    "", "", 0, FOR_CONFIG}
    , {"run-together-min",    KeyInfoInt,    "", "", 0, FOR_CONFIG}
    , {"soundslike",          KeyInfoString, "none", ""}
    , {"special",             KeyInfoString, "", ""}
    , {"ignore-accents" ,     KeyInfoBool, "", "", 0, FOR_CONFIG}
    , {"invisible-soundslike",KeyInfoBool, "", "", 0, FOR_CONFIG}
    , {"keyboard",            KeyInfoString, "standard", "", 0, FOR_CONFIG} 
    , {"affix",               KeyInfoString, "none", ""}
    , {"affix-compress",      KeyInfoBool, "false", "", 0, FOR_CONFIG}
    , {"partially-expand",    KeyInfoBool, "false", "", 0, FOR_CONFIG}
    , {"affix-char",          KeyInfoString, "/", "", 0, FOR_CONFIG}
    , {"flag-char",           KeyInfoString, ":", "", 0, FOR_CONFIG}
    , {"repl-table",          KeyInfoString, "none", ""}
    , {"sug-split-chars",     KeyInfoString, "- ", "", 0, FOR_CONFIG}
    , {"store-as",            KeyInfoString, "", ""}
    , {"try",                 KeyInfoString, "", ""}
    , {"normalize",           KeyInfoBool, "false", "", 0, FOR_CONFIG}
    , {"norm-required",       KeyInfoBool, "false", "", 0, FOR_CONFIG}
    , {"norm-form",           KeyInfoString, "nfc", "", 0, FOR_CONFIG}
  };

  static GlobalCache<Language> language_cache("language");

  PosibErr<void> Language::setup(const String & lang, const Config * config)
  {
    //
    // get_lang_info
    //

    String dir1,dir2,path;

    fill_data_dir(config, dir1, dir2);
    dir_ = find_file(path,dir1,dir2,lang,".dat");

    lang_config_ = 
      new Config("speller-lang",
                 lang_config_keys, 
                 lang_config_keys + sizeof(lang_config_keys)/sizeof(KeyInfo));
    Config & data = *lang_config_;

    {
      PosibErrBase pe = data.read_in_file(path);
      if (pe.has_err(cant_read_file)) {
	String mesg = pe.get_err()->mesg;
	mesg[0] = asc_tolower(mesg[0]);
	mesg = _("This is probably because: ") + mesg;
	return make_err(unknown_language, lang, mesg);
      } else if (pe.has_err())
	return pe;
    }

    if (!data.have("name"))
      return make_err(bad_file_format, path, _("The required field \"name\" is missing."));

    String buf;
    name_          = data.retrieve("name");
    charset_       = fix_encoding_str(data.retrieve("charset"), buf);
    data_encoding_ = fix_encoding_str(data.retrieve("data-encoding"), buf);

    {
#ifdef ENABLE_NLS
      const char * tmp = 0;
      tmp = bind_textdomain_codeset("aspell", 0);
#ifdef HAVE_LANGINFO_CODESET
      if (!tmp) tmp = nl_langinfo(CODESET);
#endif
      if (is_ascii_enc(tmp)) tmp = 0;
      if (tmp)
        RET_ON_ERR(mesg_conv_.setup(*config, charset_, fix_encoding_str(tmp, buf), NormTo));
      else 
#endif
        RET_ON_ERR(mesg_conv_.setup(*config, charset_, data_encoding_, NormTo));
      // no need to check for errors here since we know charset_ is a
      // supported encoding
      to_utf8_.setup(*config, charset_, "utf-8", NormTo);
      from_utf8_.setup(*config, "utf-8", charset_, NormFrom);
    }
    
    Conv iconv;
    RET_ON_ERR(iconv.setup(*config, data_encoding_, charset_, NormFrom));

    DataPair d;

    init(data.retrieve("special"), d, buf);
    while (split(d)) {
      char c = iconv(d.key)[0];
      split(d);
      special_[to_uchar(c)] = 
        SpecialChar (d.key[0] == '*',d.key[1] == '*', d.key[2] == '*');
    }
  
    //
    // fill_in_tables
    //
  
    FStream char_data;
    String char_data_name;
    find_file(char_data_name,dir1,dir2,charset_,".cset");
    RET_ON_ERR(char_data.open(char_data_name, "r"));
    
    String temp;
    char * p;
    do {
      p = get_nb_line(char_data, temp);
    } while (*p != '/');
    
    for (unsigned int i = 0; i != 256; ++i) {
      p = get_nb_line(char_data, temp);
      if (!p || strtoul(p, &p, 16) != i) 
        return make_err(bad_file_format, char_data_name);
      to_uni_[i] = strtol(p, &p, 16);
      while (asc_isspace(*p)) ++p;
      char_type_[i] = static_cast<CharType>(TO_CHAR_TYPE[to_uchar(*p++)]);
      while (asc_isspace(*p)) ++p;
      ++p; // display, ignored for now
      CharInfo inf = char_type_[i] >= Letter ? LETTER : 0;
      to_upper_[i] = static_cast<char>(strtol(p, &p, 16));
      inf |= to_uchar(to_upper_[i]) == i ? UPPER : 0;
      to_lower_[i] = static_cast<char>(strtol(p, &p, 16));
      inf |= to_uchar(to_lower_[i]) == i ? LOWER : 0;
      to_title_[i] = static_cast<char>(strtol(p, &p, 16));
      inf |= to_uchar(to_title_[i]) == i ? TITLE : 0;
      to_plain_[i] = static_cast<char>(strtol(p, &p, 16));
      inf |= to_uchar(to_plain_[i]) == i ? PLAIN : 0;
      inf |= to_uchar(to_plain_[i]) == 0 ? PLAIN : 0;
      sl_first_[i] = static_cast<char>(strtol(p, &p, 16));
      sl_rest_[i]  = static_cast<char>(strtol(p, &p, 16));
      char_info_[i] = inf;
    }

    for (unsigned int i = 0; i != 256; ++i) {
      de_accent_[i] = to_plain_[i] == 0 ? to_uchar(i) : to_plain_[i];
    }

    to_plain_[0] = 0x10; // to make things slightly easier
    to_plain_[1] = 0x10;

    for (unsigned int i = 0; i != 256; ++i) {
      to_stripped_[i] = to_plain_[(unsigned char)to_lower_[i]];
    }
    
    //
    //
    //

    if (data.have("store-as"))
      buf = data.retrieve("store-as");
    else if (data.retrieve_bool("affix-compress"))
      buf = "lower";
    else
      buf = "stripped";
    char * clean_is;
    if (buf == "stripped") {
      store_as_ = Stripped;
      clean_is = to_stripped_;
    } else {
      store_as_ = Lower;
      clean_is = to_lower_;
    }

    for (unsigned i = 0; i != 256; ++i) {
      to_clean_[i] = char_type_[i] > NonLetter ? clean_is[i] : 0;
      if ((unsigned char)to_clean_[i] == i) char_info_[i] |= CLEAN;
    }

    to_clean_[0x00] = 0x10; // to make things slightly easier
    to_clean_[0x10] = 0x10;

    clean_chars_   = get_clean_chars(*this);
    
    //
    //
    //
    
    for (int i = 0; i != 256; ++i) 
      to_normalized_[i] = 0;

    int c = 1;
    for (int i = 0; i != 256; ++i) {
      if (is_alpha(i)) {
	if (to_normalized_[to_uchar(to_stripped_[i])] == 0) {
	  to_normalized_[i] = c;
	  to_normalized_[to_uchar(to_stripped_[i])] = c;
	  ++c;
	} else {
	  to_normalized_[i] = to_normalized_[to_uchar(to_stripped_[i])];
	}
      }
    }
    for (int i = 0; i != 256; ++i) {
      if (to_normalized_[i]==0) to_normalized_[i] = c;
    }
    max_normalized_ = c;

    //
    // prep phonetic code
    //

    PosibErr<Soundslike *> pe = new_soundslike(data.retrieve("soundslike"),
                                               iconv,
                                               this);
    if (pe.has_err()) return pe;
    soundslike_.reset(pe);
    soundslike_chars_ = soundslike_->soundslike_chars();

    have_soundslike_ = strcmp(soundslike_->name(), "none") != 0;

    //
    // prep affix code
    //

    affix_.reset(new_affix_mgr(data.retrieve("affix"), iconv, this));

    //
    // fill repl tables (if any)
    //

    String repl = data.retrieve("repl-table");
    have_repl_ = false;
    if (repl != "none") {

      String repl_file;
      FStream REPL;
      find_file(repl_file, dir1, dir2, repl, "_repl", ".dat");
      RET_ON_ERR(REPL.open(repl_file, "r"));
      
      size_t num_repl = 0;
      while (getdata_pair(REPL, d, buf)) {
        ::to_lower(d.key);
        if (d.key == "rep") {
          num_repl = atoi(d.value); // FIXME make this more robust
          break;
        }
      }
      repls_.resize(num_repl);
      if (num_repl > 0)
        have_repl_ = true;

      for (size_t i = 0; i != num_repl; ++i) {
        bool res = getdata_pair(REPL, d, buf);
        assert(res); // FIXME
        ::to_lower(d.key);
        assert(d.key == "rep"); // FIXME
        split(d);
        repls_[i].substr = buf_.dup(iconv(d.key));
        to_clean((char *)repls_[i].substr, repls_[i].substr);
        repls_[i].repl   = buf_.dup(iconv(d.value));
        to_clean((char *)repls_[i].repl, repls_[i].repl);
      }

    }
    return no_err;
  }

  void Language::set_lang_defaults(Config & config) const
  {
    config.replace_internal("actual-lang", name());
    StackPtr<KeyInfoEnumeration> els(lang_config_->possible_elements(false));
    const KeyInfo * k;
    Conv to_utf8;
    to_utf8.setup(config, data_encoding_, "utf-8", NormTo);
    while ((k = els->next()) != 0) {
      if (k->other_data == FOR_CONFIG 
	  && lang_config_->have(k->name) && !config.have(k->name))
      {
        const KeyInfo * ck = config.keyinfo(k->name);
        if (ck->flags & KEYINFO_UTF8)
          config.replace(k->name, to_utf8(lang_config_->retrieve(k->name)));
        else
          config.replace(k->name, lang_config_->retrieve(k->name));
      }
    }
  }

  WordInfo Language::get_word_info(ParmString str) const
  {
    CharInfo first = CHAR_INFO_ALL, all = CHAR_INFO_ALL;
    const char * p = str;
    while (*p && (first = char_info(*p++), all &= first, !(first & LETTER)));
    while (*p) all &= char_info(*p++);
    WordInfo res;
    if      (all & LOWER)   res = AllLower;
    else if (all & UPPER)   res = AllUpper;
    else if (first & TITLE) res = FirstUpper;
    else                    res = Other;
    if (all & PLAIN)  res |= ALL_PLAIN;
    if (all & CLEAN)  res |= ALL_CLEAN;
    return res;
  }
  
  CasePattern Language::case_pattern(ParmString str) const  
  {
    CharInfo first = CHAR_INFO_ALL, all = CHAR_INFO_ALL;
    const char * p = str;
    while (*p && (first = char_info(*p++), all &= first, !(first & LETTER)));
    while (*p) all &= char_info(*p++);
    if      (all & LOWER)   return AllLower;
    else if (all & UPPER)   return AllUpper;
    else if (first & TITLE) return FirstUpper;
    else                    return Other;
  }
  
  void Language::fix_case(CasePattern case_pattern,
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

  const char * Language::fix_case(CasePattern case_pattern, const char * str,
                                  String & buf) const 
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
  
  bool SensitiveCompare::operator() (const char * word0, 
				     const char * inlist0) const
  {
    assert(*word0 && *inlist0);
  try_again:
    const char * word = word0;
    const char * inlist = inlist0;
    if (*word == *inlist || *word == lang->to_title(*inlist)) ++word, ++inlist;
    else                                                      goto try_upper;
    while (*word && *inlist && *word == *inlist) ++word, ++inlist;
    if (*inlist) goto try_upper;
    if (lang->special(*word).end) ++word;
    if (*word) goto try_upper;
    return true;
  try_upper:
    word = word0;
    inlist = inlist0;
    while (*word && *inlist && *word == lang->to_upper(*inlist)) ++word, ++inlist;
    if (*inlist) goto fail;
    if (lang->special(*word).end) ++word;
    if (*word) goto fail;
    return true;
  fail:
    if (lang->special(*word0).begin) {++word0; goto try_again;}
    return false;
  }

  PosibErr<void> check_if_valid(const Language & l, ParmString word) {
    if (*word == '\0') 
      return make_err(invalid_word, MsgConv(l)(word), _("Empty string."));
    const char * i = word;
    if (!l.is_alpha(*i)) {
      if (!l.special(*i).begin) {
        char m[70];
        snprintf(m, 70, 
                 _("The character '%s' may not appear at the beginning of a word."),
                 MsgConv(l)(*i));
        return make_err(invalid_word, MsgConv(l)(word), m);
      } else if (!l.is_alpha(*(i+1))) {
	return make_err(invalid_word, MsgConv(l)(word), _("Does not contain any letters."));
      }
    }
    for (;*(i+1) != '\0'; ++i) { 
      if (!l.is_alpha(*i)) {
	if (!l.special(*i).middle) {
          char m[70];
          snprintf(m, 70, 
                   _("The character '%s' may not appear in the middle of a word."),
                   MsgConv(l)(*i));
          return make_err(invalid_word, MsgConv(l)(word), m);
        }
      }
    }
    if (!l.is_alpha(*i)) {
      if (!l.special(*i).end) {
        char m[70];
        snprintf(m, 70, 
                 _("The character '%s' may not appear at the end of a word."),
                 MsgConv(l)(*i));
        return make_err(invalid_word, MsgConv(l)(word), m);
      }
    }
    return no_err;
  }

  String get_stripped_chars(const Language & lang) {
    bool chars_set[256] = {0};
    String     chars_list;
    for (int i = 0; i != 256; ++i) 
    {
      char c = static_cast<char>(i);
	if (lang.is_alpha(c) || lang.special(c).any)
	  chars_set[static_cast<unsigned char>(lang.to_stripped(c))] = true;
    }
    for (int i = 1; i != 256; ++i) 
    {
      if (chars_set[i]) 
	chars_list += static_cast<char>(i);
    }
    return chars_list;
  }

  String get_clean_chars(const Language & lang) {
    bool chars_set[256] = {0};
    String     chars_list;
    for (int i = 0; i != 256; ++i) 
    {
      char c = static_cast<char>(i);
      if (lang.is_alpha(c) || lang.special(c).any) 
        chars_set[static_cast<unsigned char>(lang.to_clean(c))] = true;
    }
    for (int i = 1; i != 256; ++i) 
    {
      if (chars_set[i]) {
	chars_list += static_cast<char>(i);
      }
    }
    return chars_list;
  }

  PosibErr<Language *> new_language(const Config & config, ParmString lang)
  {
    if (!lang)
      return get_cache_data(&language_cache, &config, config.retrieve("lang"));
    else
      return get_cache_data(&language_cache, &config, lang);
  }

  PosibErr<void> open_affix_file(const Config & c, FStream & f)
  {
    String lang = c.retrieve("lang");

    String dir1,dir2,path;
    fill_data_dir(&c, dir1, dir2);
    String dir = find_file(path,dir1,dir2,lang,".dat");

    String file;
    file += dir;
    file += '/';
    file += lang;
    file += "_affix.dat";
    
    RET_ON_ERR(f.open(file,"r"));

    return no_err;
  }

  bool find_language(Config & c)
  {
    String lang = c.retrieve("lang");
    lang.ensure_null_end();
    char * l = lang.data();

    String dir1,dir2,path;
    fill_data_dir(&c, dir1, dir2);

    char * s = l + strlen(l);

    while (s > l) {
      find_file(path,dir1,dir2,l,".dat");
      if (file_exists(path)) {
        c.replace_internal("actual-lang", lang);
        return true;
      }
      while (s > l && !(*s == '-' || *s == '_')) --s;
      *s = '\0';
    }
    return false;
  }

}
