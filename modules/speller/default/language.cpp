// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#include "settings.h"

#include <vector>
#include <assert.h>

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

#ifdef ENABLE_NLS
#  include <langinfo.h>
#endif

namespace aspeller {

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
    , {"use-soundslike" ,     KeyInfoBool, "", "", 0, FOR_CONFIG}
    , {"use-jump-tables",     KeyInfoBool, "", "", 0, FOR_CONFIG}
    , {"keyboard",            KeyInfoString, "standard", "", 0, FOR_CONFIG} 
    , {"affix",               KeyInfoString, "none", ""}
    , {"affix-compress",      KeyInfoBool, "false", "", 0, FOR_CONFIG}
    , {"affix-char",          KeyInfoString, "/", "", 0, FOR_CONFIG}
    , {"flag-char",           KeyInfoString, ":", "", 0, FOR_CONFIG}
    , {"repl-table",          KeyInfoString, "none", ""}
    , {"sug-split-chars",     KeyInfoString, "- ", "", 0, FOR_CONFIG}
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

    String sbuf;
    name_          = data.retrieve("name");
    charset_       = fix_encoding_str(data.retrieve("charset"), sbuf);
    data_encoding_ = fix_encoding_str(data.retrieve("data-encoding"), sbuf);

    {
#ifdef ENABLE_NLS
      String buf;
      const char * tmp = 0;
      tmp = bind_textdomain_codeset("aspell", 0);
      if (!tmp) tmp = nl_langinfo(CODESET);
      if (is_ascii_enc(tmp)) tmp = 0;
      if (tmp)
        mesg_conv_.setup(*config, charset_, fix_encoding_str(tmp, buf));
      else 
#endif
        mesg_conv_.setup(*config, charset_, data_encoding_);
      to_utf8_.setup(*config, charset_, "utf-8");
      from_utf8_.setup(*config, "utf-8", charset_);
    }

    Conv iconv;
    iconv.setup(*config, data_encoding_, charset_);

    String buf; DataPair d;

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
    find_file(char_data_name,dir1,dir2,charset_,".dat");
    RET_ON_ERR(char_data.open(char_data_name, "r"));
    
    String temp;
    char_data.getline(temp);
    char_data.getline(temp);
    for (int i = 0; i != 256; ++i) {
      char_data >> to_uni_[i];
      char_data >> temp;
      char_type_[i] = temp == "letter" ? letter 
	: temp == "space"  ? space 
	: other;
      int num = -1;
      char_data >> num; to_lower_[i]    = static_cast<char>(num);
      char_data >> num; to_upper_[i]    = static_cast<char>(num);
      char_data >> num; to_title_[i]    = static_cast<char>(num);
      char_data >> num; to_sl_[i]       = static_cast<char>(num);
      char_data >> num; to_stripped_[i] = static_cast<char>(num);
      char_data >> num; de_accent_[i] = static_cast<char>(num);
      if (char_data.peek() != '\n') 
	return make_err(bad_file_format, char_data_name);
    }
    
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
    stripped_chars_   = get_stripped_chars(*this);

    //
    // prep affix code
    //

    affix_.reset(new_affix_mgr(data.retrieve("affix"), iconv, this));

    //
    // fill repl tables (if any)
    //

    String repl = data.retrieve("repl-table");
    if (repl != "none") {

      String repl_file;
      FStream REPL;
      find_file(repl_file, dir1, dir2, repl, "_repl", ".dat");
      RET_ON_ERR(REPL.open(repl_file, "r"));
      
      while (getdata_pair(REPL, d, buf), ::to_lower(d.key), d.key != "rep");
      size_t num_repl = atoi(d.value); // FIXME make this more robust
      repls_.resize(num_repl);

      for (size_t i = 0; i != num_repl; ++i) {
        bool res = getdata_pair(REPL, d, buf);
        assert(res); // FIXME
        ::to_lower(d.key);
        assert(d.key == "rep"); // FIXME
        split(d);
        repls_[i].substr = buf_.dup(iconv(d.key));
        repls_[i].repl   = buf_.dup(iconv(d.value));
      }

    }
    return no_err;
  }

  void Language::set_lang_defaults(Config & config)
  {
    StackPtr<KeyInfoEnumeration> els(lang_config_->possible_elements(false));
    const KeyInfo * k;
    Conv to_utf8;
    to_utf8.setup(config, data_encoding_, "utf-8");
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
    
  // FIXME: Bug, returns true when inword is a prefix of word
  //        ie (going, go)
  bool SensitiveCompare::operator() (const char * word, 
				     const char * inlist) const
  {
    // this will fail if word or inlist is empty
    assert (*word != '\0' && *inlist != '\0');
    
    // if begin inlist is a begin char then it must match begin word
    // chop all begin chars from the begin of word and inlist  
    if (lang->special(*inlist).begin) {
      if (*word != *inlist)
	return false;
      ++word, ++inlist;
    } else if (lang->special(*word).begin) {
      ++word;
    }
    
    // this will fail if word or inlist only contain a begin char
    assert (*word != '\0' && *inlist != '\0');
    
    if (case_insensitive) {
      if (ignore_accents) {

	while (*word != '\0' && *inlist != '\0') 
	  ++word, ++inlist;

      } else if (strip_accents) {

	while (*word != '\0' && *inlist != '\0') {
	  if (lang->to_lower(*word) != lang->de_accent(lang->to_lower(*inlist)))
	    return false;
	  ++word, ++inlist;
	}

      } else {

	while (*word != '\0' && *inlist != '\0') {
	  if (lang->to_lower(*word) != lang->to_lower(*inlist))
	    return false;
	  ++word, ++inlist;
	}

      }
    } else {
      //   (note: there are 3 possible casing lower, upper and title)
      //   if is lower begin inlist then begin word can be any casing
      //   if not                   then begin word must be the same case
      bool case_compatible = true;
      if (!ignore_accents) {
	if (strip_accents) {
	  if (lang->to_lower(*word) != lang->de_accent(lang->to_lower(*inlist)))
	    return false;
	} else {
	  if (lang->to_lower(*word) != lang->to_lower(*inlist))
	    return false;
	}
      }
      if (!lang->is_lower(*inlist) && lang->de_accent(*word) != lang->de_accent(*inlist))
	case_compatible = false;
      bool all_upper = lang->is_upper(*word);
      ++word, ++inlist;
      while (*word != '\0' && *inlist != '\0') {
	if (lang->char_type(*word) == Language::letter) {
	  if (!lang->is_upper(*word))
	    all_upper = false;
	  if (ignore_accents) {
	    if (lang->de_accent(*word) != lang->de_accent(*inlist))
	      case_compatible = false;
	  } else if (strip_accents) {
	    if (*word != lang->de_accent(*inlist))
	      if (lang->to_lower(*word) != lang->de_accent(lang->to_lower(*inlist)))
		return false;
	      else // accents match case does not
		case_compatible = false;
	  } else {
	    if (*word != *inlist)
	      if (lang->to_lower(*word) != lang->to_lower(*inlist))
		return false;
	      else // accents match case does not
		case_compatible = false;
	  }
	}
	++word, ++inlist;
      }
      //   if word is all upper than casing of inlist can be anything
      //   otherwise the casing of tail begin and tail inlist must match
      if (all_upper) 
	case_compatible = true;
      if (!case_compatible) 
	return false;
    }
    if (*inlist != '\0') ++inlist;
    assert(*inlist == '\0');
  
    //   if end   inlist is a end   char then it must match end word
    if (lang->special(*(inlist-1)).end) {
      if (*(inlist-1) != *(word-1))
	return false;
    }
    return true;
  }

  static inline PosibErr<void> invalid_char(ParmString word, ParmString letter, 
                                            const char * where)
  {
    char m[70];
    switch (where[0]) {
    case 'b':
      snprintf(m, 70, 
               _("The character '%s' may not appear at the beginning of a word."),
               letter.str());
      break;
    case 'm':
      snprintf(m, 70, 
               _("The character '%s' may not appear at the middle of a word."),
               letter.str());
      break;
    case 'e':
      snprintf(m, 70, 
               _("The character '%s' may not appear at the end of a word."),
               letter.str());
      break;
    default:
      abort();
    }
    return make_err(invalid_word, word, m);
  }

  PosibErr<void> check_if_valid(const Language & l, ParmString word) {
    if (*word == '\0') 
      return make_err(invalid_word, MsgConv(l)(word), _("Empty string."));
    const char * i = word;
    if (l.char_type(*i) != Language::letter) {
      if (!l.special(*i).begin)
	return invalid_char(MsgConv(l)(word), MsgConv(l)(*i), "beg");
      else if (l.char_type(*(i+1)) != Language::letter)
	return make_err(invalid_word, MsgConv(l)(word), _("Does not contain any letters."));
    }
    for (;*(i+1) != '\0'; ++i) { 
      if (l.char_type(*i) != Language::letter) {
	if (!l.special(*i).middle)
	  return invalid_char(MsgConv(l)(word), MsgConv(l)(*i), "middle");
      }
    }
    if (l.char_type(*i) != Language::letter) {
      if (!l.special(*i).end)
	return invalid_char(MsgConv(l)(word), MsgConv(l)(*i), "end");
    }
    return no_err;
  }

  String get_stripped_chars(const Language & lang) {
    bool chars_set[256] = {0};
    String     chars_list;
    for (int i = 0; i != 256; ++i) 
    {
      char c = static_cast<char>(i);
	if (lang.is_alpha(c) || lang.special(c).any())
	  chars_set[static_cast<unsigned char>(lang.to_stripped(c))] = true;
    }
    for (int i = 0; i != 256; ++i) 
    {
      if (chars_set[i]) 
	chars_list += static_cast<char>(i);
    }
    return chars_list;
  }


  PosibErr<Language *> new_language(const Config & config, ParmString lang)
  {
    if (!lang)
      return get_cache_data(&language_cache, &config, config.retrieve("actual-lang"));
    else
      return get_cache_data(&language_cache, &config, lang);
  }

  PosibErr<void> open_affix_file(const Config & c, FStream & f)
  {

    String lang = c.retrieve("actual-lang");

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

}
