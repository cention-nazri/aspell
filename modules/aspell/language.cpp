// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#include "settings.h"

#include <vector>
#include <assert.h>

#include "clone_ptr-t.hpp"
#include "config.hpp"
#include "emulation.hpp"
#include "errors.hpp"
#include "file_data_util.hpp"
#include "fstream.hpp"
#include "language.hpp"
#include "split.hpp"
#include "string.hpp"

namespace aspell {

  // FIXME: The "c" might conflict with ConfigData Use of taht slot
  //   work on a policy to avoid that such resering the first half
  //   for ConfigData's use and the otehr for users.
  static const KeyInfo lang_config_keys[] = {
    {"charset",             KeyInfoString, "iso8859-1", ""}
    , {"name",                KeyInfoString, "", ""}
    , {"run-together",        KeyInfoBool,   "", "", "c"}
    , {"run-together-limit",  KeyInfoInt,    "", "", "c"}
    , {"run-together-middle", KeyInfoString, "", ""}
    , {"run-together-min",    KeyInfoInt,    "", "", "c"}
    , {"soundslike",          KeyInfoString, "generic", ""}
    , {"special",             KeyInfoString, "", ""}
    , {"ignore-accents" ,     KeyInfoBool, "", "", "c"}
    , {"use-soundslike" ,     KeyInfoBool, "",  ""}
    , {"keyboard",            KeyInfoString, "standard", "", "c"} 
  };
  
  PosibErr<void> Language::setup(String lang, Config * config) 
  {
    //if (!config)      config = new Config(); FIXME
    assert(config != 0);
    if (lang.empty()) lang   = config->retrieve("actual-lang");

    String dir1, dir2;
    fill_data_dir(config, dir1, dir2);

    //
    // get_lang_info
    //
    
    Config data("aspell-lang",
		lang_config_keys, 
		lang_config_keys + sizeof(lang_config_keys)/sizeof(KeyInfo));
    String path;
    find_file(path,dir1,dir2,lang,".dat");
    data.read_in_file(path);

    if (!data.have("name"))
      return make_err(bad_file_format, path, "The required field \"name\" is missing.");

    name_         = data.retrieve("name");
    charset_      = data.retrieve("charset");
    mid_chars_    = data.retrieve("run-together-middle");

    std::vector<String> special_data = split(data.retrieve("special"));
    for (std::vector<String>::iterator i = special_data.begin();
	 i != special_data.end();
	 ++i) 
      {
	char c = (*i)[0];
	++i;
	special_[to_uchar(c)] = 
	  SpecialChar ((*i)[0] == '*',(*i)[1] == '*',(*i)[2] == '*');
      }

    //
    //
    //

    Emulation<KeyInfoEmulation> els = data.possible_elements(false);
    const KeyInfo * k;
    while ((k = els.next()) != 0) {
      if (k->otherdata[0] == 'c' 
	  && data.have(k->name) && !config->have(k->name))
	config->replace(k->name, data.retrieve(k->name));
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
    // 
    // 

    normalize_mid_characters(*this,mid_chars_);

    //
    // prep phonetic code
    //
    
    soundslike_.reset(new_soundslike(data.retrieve("soundslike"), 
				     dir1, dir2,
				     this));
    soundslike_chars_ = soundslike_->soundslike_chars();

    return no_err;
  }

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

  static PosibErr<void> invalid_char(ParmString word, char letter, ParmString where)
  {
    String m;
    m += "The character '";
    m += letter;
    m += "' may not appear at the ";
    m += where;
    m += " of a word.";
    return make_err(invalid_word, word, m);
  }

  PosibErr<void> check_if_valid(const Language & l, ParmString word) {
    if (*word == '\0') 
      return make_err(invalid_word, word, "Empty string.");
    const char * i = word;
    if (l.char_type(*i) != Language::letter) {
      if (!l.special(*i).begin)
	return invalid_char(word, *i, "beginning");
      else if (l.char_type(*(i+1)) != Language::letter)
	return make_err(invalid_word, word, "Does not contain any letters.");
    }
    for (;*(i+1) != '\0'; ++i) { 
      if (l.char_type(*i) != Language::letter) {
	if (!l.special(*i).middle)
	  return invalid_char(word, *i, "middle");
      }
    }
    if (l.char_type(*i) != Language::letter) {
      if (!l.special(*i).end)
	return invalid_char(word, *i, "end");
    }
    return no_err;
  }

  void normalize_mid_characters(const Language & l, String & s) 
  {
    assert (s.size() < 4);
    for (unsigned int i = 0; i != s.size(); ++i) 
    {
      s[i] = l.to_lower(s[i]);
    }
    // now sort it
    if (s.size() == 3) 
    {
      if (s[0] < s[1])
	std::swap(s[0], s[1]);
      if (s[1] < s[2])
	std::swap(s[1], s[2]);
    } 
    if (s.size() >= 2) 
    {
      if (s[0] < s[1])
	std::swap(s[0], s[1]);
    }
    
  }

}
