// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#include "language.hpp"
#include "phonetic.hpp"
#include "phonet.hpp"

#include "file_util.hpp"
#include "file_data_util.hpp"
#include "clone_ptr-t.hpp"

#include <vector>

namespace aspeller {
  
  class GenericSoundslike : public Soundslike {
  private:
    const Language * lang;
  public:
    GenericSoundslike(const Language * l) : lang(l) {}

    PosibErr<void> setup() {return no_err;}
    
    Soundslike * clone() const {return new GenericSoundslike(*this);}
    void assign(const Soundslike * other) {
      *this = *static_cast<const GenericSoundslike *>(other);
    }
 
    String soundslike_chars() const {
      bool chars_set[256] = {0};
      String     chars_list;
      for (int i = 0; i != 256; ++i) 
	{
	  char c = lang->to_sl(static_cast<char>(i));
	  if (c) chars_set[static_cast<unsigned char>(c)] = true;
	}
      for (int i = 0; i != 256; ++i) 
	{
	  if (chars_set[i]) 
	    chars_list += static_cast<char>(i);
	}
      return chars_list;
    }

    String to_soundslike(ParmString str) const 
    {
      String new_word;
      char prev = '\0';
      char cur;
      
      for (const char * i = str; *i != '\0'; ++i) {
	cur = lang->to_sl(*i);
	if (cur != '\0' && cur != prev) new_word += lang->to_sl(*i);
	prev = cur;
      }
      
      return new_word;
    }

    const char * name () const {
      return "generic";
    }
    const char * version() const {
      return "1.0";
    }
  };

  class NoSoundslike : public Soundslike {
  private:
    const Language * lang;
  public:
    NoSoundslike(const Language * l) : lang(l) {}

    PosibErr<void> setup() {return no_err;}
    
    Soundslike * clone() const {return new NoSoundslike(*this);}
    void assign(const Soundslike * other) {
      *this = *static_cast<const NoSoundslike *>(other);
    }
 
    String soundslike_chars() const {
      bool chars_set[256] = {0};
      String     chars_list;
      for (int i = 0; i != 256; ++i) 
      {
	char c = static_cast<char>(i);
	if (lang->is_alpha(c) || lang->special(c).any())
	  chars_set[static_cast<unsigned char>(lang->to_stripped(c))] = true;
      }
      for (int i = 0; i != 256; ++i) 
      {
	if (chars_set[i]) 
	  chars_list += static_cast<char>(i);
      }
      return chars_list;
    }

    String to_soundslike(ParmString str) const 
    {
      String new_word;
      new_word.reserve(str.size());
      
      for (const char * i = str; *i != '\0'; ++i) 
	new_word += lang->to_stripped(*i);
      
      return new_word;
    }

    const char * name() const {
      return "none";
    }
    const char * version() const {
      return "1.0";
    }
  };

  class PhonetSoundslike : public Soundslike {

    const Language * lang;
    ClonePtr<PhonetParms> phonet_parms;
    
  public:

    Soundslike * clone() const {return new PhonetSoundslike(*this);}
    void assign(const Soundslike * other) {
      *this = *static_cast<const PhonetSoundslike *>(other);
    }

    PhonetSoundslike(const Language * l) : lang(l) {}

    PosibErr<void> setup() {
      String file;
      file += lang->data_dir();
      file += '/';
      file += lang->name();
      file += "_phonet.dat";
      PosibErr<PhonetParms *> pe = load_phonet_rules(file);
      if (pe.has_err()) return pe;
      phonet_parms.reset(pe);
      for (int i = 0; i != 256; ++i) {
	phonet_parms->to_upper[i] = lang->to_upper(i);
	phonet_parms->is_alpha[i] = lang->is_alpha(i);
      }
      init_phonet_hash(*phonet_parms);
      return no_err;
    }


    String soundslike_chars() const 
    {
      bool chars_set[256] = {0};
      String     chars_list;
      for (const char * * i = phonet_parms->rules + 1; 
	   *(i-1) != PhonetParms::rules_end;
	   i += 2) 
	{
	  for (const char * j = *i; *j; ++j) 
	    {
	      chars_set[static_cast<unsigned char>(*j)] = true;
	    }
	}
      for (int i = 0; i != 256; ++i) 
	{
	  if (chars_set[i]) 
	    chars_list += static_cast<char>(i);
	}
      return chars_list;
    }
    
    String to_soundslike(ParmString str) const 
    {
      std::vector<char> new_word;
      new_word.resize(str.size()+1);
      phonet(str, &new_word.front(), *phonet_parms);
      return &new_word.front();
    }
    
    const char * name() const
    {
      return "phonet";
    }
    const char * version() const 
    {
      return phonet_parms->version.c_str();
    }
  };
  
  
  PosibErr<Soundslike *> new_soundslike(ParmString name, 
                                        const Language * lang)
  {
    Soundslike * sl;
    if (name == "generic") {
      sl = new GenericSoundslike(lang);
    } else if (name == "none") {
      sl = new NoSoundslike(lang);
    } else {
      sl = new PhonetSoundslike(lang);
    }
    PosibErrBase pe = sl->setup();
    if (pe.has_err()) {
      delete sl;
      return pe;
    } else {
      return sl;
    }
  }

}

namespace acommon {
  template class ClonePtr<aspeller::Soundslike>;
}