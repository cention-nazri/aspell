// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#include "language.hpp"
#include "phonetic.hpp"
#include "phonet.hpp"

#include "file_util.hpp"
#include "file_data_util.hpp"
#include "clone_ptr-t.hpp"

namespace aspeller {
  
  class GenericSoundslike : public Soundslike {
  private:
    const Language * lang;
  public:
    GenericSoundslike(const Language * l) : lang(l) {}

    PosibErr<void> setup(Conv &) {return no_err;}
    
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

    char * to_soundslike(char * res, const char * str, int size) const 
    {
      char prev = '\0';
      char cur;
      
      for (const char * i = str; *i; ++i) {
	cur = lang->to_sl(*i);
	if (cur != '\0' && cur != prev) *res++ = cur;
	prev = cur;
      }
      *res = '\0';
      return res;
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

    PosibErr<void> setup(Conv &) {return no_err;}
    
    String soundslike_chars() const {
      return get_stripped_chars(*lang);
    }

    char * to_soundslike(char * res, const char * str, int size) const 
    {
      return lang->LangImpl::to_stripped(res, str);
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
    StackPtr<PhonetParms> phonet_parms;
    
  public:

    PhonetSoundslike(const Language * l) : lang(l) {}

    PosibErr<void> setup(Conv & iconv) {
      String file;
      file += lang->data_dir();
      file += '/';
      file += lang->name();
      file += "_phonet.dat";
      PosibErr<PhonetParms *> pe = new_phonet(file, iconv, lang);
      if (pe.has_err()) return pe;
      phonet_parms.reset(pe);
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
    
    char * to_soundslike(char * res, const char * str, int size) const 
    {
      int new_size = phonet(str, res, size, *phonet_parms);
      return res + new_size;
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
                                        Conv & iconv,
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
    PosibErrBase pe = sl->setup(iconv);
    if (pe.has_err()) {
      delete sl;
      return pe;
    } else {
      return sl;
    }
  }

}

