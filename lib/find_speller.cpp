// This file is part of The New Aspell
// Copyright (C) 2000-2001 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.

#include <assert.h>
#include <string.h>

// POSIX includes
#include <sys/types.h>
#include <dirent.h>

#include "asc_ctype.hpp"
#include "can_have_error.hpp"
#include "config.hpp"
#include "enumeration.hpp"
#include "errors.hpp"
#include "fstream.hpp"
#include "getdata.hpp"
#include "info.hpp"
#include "speller.hpp"
#include "string_list.hpp"
#include "string_enumeration.hpp"

#if 0
#include "preload.h"
#define LT_NON_POSIX_NAMESPACE 1
#ifdef USE_LTDL
#include <ltdl.h>
#endif
#endif

using namespace acommon;

namespace acommon {

  static void free_lt_handle(SpellerLtHandle h) 
  {
#ifdef USE_LTDL
    int s;
    s = lt_dlclose((lt_dlhandle)h);
    assert (s == 0);
    s = lt_dlexit();
    assert (s == 0);
#endif
  }

  extern "C" 
  Speller * libaspell_speller_default_LTX_new_speller_class(SpellerLtHandle);
  
  PosibErr<Speller *> get_speller_class(Config * config)
  {
    String name = config->retrieve("module");
    assert(name == "default");
    return libaspell_speller_default_LTX_new_speller_class(0);
#if 0
    unsigned int i; 
    for (i = 0; i != aspell_speller_funs_size; ++i) {
      if (strcmp(name.c_str(), aspell_speller_funs[i].name) == 0) {
	return (*aspell_speller_funs[i].fun)(config, 0);
      }
    }
  
#ifdef USE_LTDL
    int s = lt_dlinit();
    assert(s == 0);
    String libname;
    libname  = LIBDIR "/libaspell_";
    libname += name;
    libname += ".la";
    lt_dlhandle h = lt_dlopen (libname.c_str());
    if (h == 0)
      return (new CanHaveErrorImpl())
	->set_error(cant_load_module, name.c_str());
    lt_ptr_t fun = lt_dlsym (h, "new_aspell_speller_class");
    assert (fun != 0);
    CanHaveError * m = (*(NewSpellerClass)(fun))(config, h);
    assert (m != 0);
    if (m->error_number() != 0)
      free_lt_handle(h);
    return m;
#else
    return (new CanHaveErrorImpl())
      ->set_error(cant_load_module, name.c_str());
#endif
#endif
  }

  // Note this writes all over str
  static void split_string_list(StringList * list, ParmString str)
  {
    const char * s0 = str;
    const char * s1;
    while (true) {
      while (*s0 != '\0' && isspace(*s0)) ++s0;
      if (*s0 == '\0') break;
      s1 = s0;
      while (!isspace(*s1)) ++s1;
      String temp(s0,s1-s0);
      list->add(temp);
      if (*s1 != '\0')
	s0 = s1 + 1;
    }
  }

  enum IsBetter {BetterMatch, WorseMatch, SameMatch};

  struct Better
  {
    unsigned int cur_rank;
    unsigned int best_rank;
    unsigned int worst_rank;
    virtual void init() = 0;
    virtual void set_best_from_cur() = 0;
    virtual void set_cur_rank() = 0;
    IsBetter better_match(IsBetter prev);  
    virtual ~Better();
  };

  Better::~Better() {}

  IsBetter Better::better_match(IsBetter prev)
  {
    if (prev == WorseMatch)
      return prev;
    set_cur_rank();
    if (cur_rank >= worst_rank)
      return WorseMatch;
    else if (cur_rank < best_rank)
      return BetterMatch;
    else if (cur_rank == best_rank)
      return prev;
    else // cur_rank > best_rank
      if (prev == SameMatch)
	return WorseMatch;
      else
	return BetterMatch;
  }

  struct BetterList : public Better
  {
    const char *         cur;
    ClonePtr<StringList> list;
    const char *         best;
    BetterList();
    void init();
    void set_best_from_cur();
    void set_cur_rank();
  };

  BetterList::BetterList() 
    : list(new_string_list()) 
  {
  }

  void BetterList::init() {
    Enumeration<StringEnumeration> es = list->elements();
    worst_rank = 0;
    while ( (es.next()) != 0)
      ++worst_rank;
    best_rank = worst_rank;
  }

  void BetterList::set_best_from_cur() 
  {
    best_rank = cur_rank;
    best = cur;
  }

  void BetterList::set_cur_rank() 
  {
    Enumeration<StringEnumeration> es = list->elements();
    const char * m;
    cur_rank = 0;
    while ( (m = es.next()) != 0 && strcmp(m, cur) != 0)
      ++cur_rank;
  }

  struct BetterSize : public Better
  {
    unsigned int         cur;
    const char *         cur_str;
    char                 req_type;
    unsigned int         requested;
    unsigned int         size;
    unsigned int         best;
    const char *         best_str;
    void init();
    void set_best_from_cur();
    void set_cur_rank();
  };


  void BetterSize::init() {
    worst_rank = 0xFFF;
    best_rank = worst_rank;
  }

  void BetterSize::set_best_from_cur() 
  {
    best_rank = cur_rank;
    best = cur;
    best_str = cur_str;
  }

  void BetterSize::set_cur_rank() 
  {
    int diff = cur - requested;
    int sign;
    if (diff < 0) {
      cur_rank = -diff;
      sign = -1;
    } else {
      cur_rank = diff;
      sign = 1;
    }
    cur_rank <<= 1;
    if ((sign == -1 && req_type == '+') || (sign == 1 && req_type == '-'))
      cur_rank |= 0x1;
    else if ((sign == -1 && req_type == '>') || (sign == 1 && req_type == '<'))
      cur_rank |= 0x100;
  }

  PosibErr<Config *> find_word_list(Config * c) 
  {
    Config * config = c->clone();
    ////////////////////////////////////////////////////////////////////
    //
    // Give first prefrence to an exect match for the language-country
    // code, then give prefrence to those in the alternate code list
    // in the order they are presented, then if there is no match
    // look for one for just language.  If that fails give up.
    // Once the best matching code is found, try to find a matching
    // jargon if one exists, other wise look for one with no jargon.
    //

    BetterList b_code;
    BetterList b_jargon;
    BetterList b_module;
    BetterSize b_size;
    Better * better[4] = {&b_code,&b_jargon,&b_module,&b_size};
    const DictInfo * best = 0;

    //
    // retrieve and normalize code
    //
    const char * p;
    String code;
    PosibErr<String> str = config->retrieve("lang");
    p = str.data.c_str();
    code += asc_tolower(*p++); code += asc_tolower(*p++);
    String lang = code;
    bool have_country = false;
    if (*p == '-' || *p == '_') {
      ++p;
      have_country = true;
      code += '_'; code += asc_toupper(*p++); code += asc_toupper(*p++);
    }
  
    //
    // Retrive acceptable code search orders
    //
    String lang_country_list;
    if (have_country) {
      lang_country_list = code;
      lang_country_list += ' ';
    }
    String lang_only_list = lang;
    lang_only_list += ' ';

    // read retrive lang_country_list and lang_only_list from file(s)
    // FIXME: Write Me

    //
    split_string_list(b_code.list, lang_country_list);
    split_string_list(b_code.list, lang_only_list);
    b_code.init();

    //
    // Retrieve Jargon
    // 
    str = config->retrieve("jargon");
    if (str.get_err() != 0)
      b_jargon.list->add(str);
    b_jargon.list->add("");
    b_jargon.init();
    str.data.clear();

    //
    // Retrive module list
    //
    if (config->have("module"))
      b_module.list->add(config->retrieve("module"));
    else if (config->have("module-search-order"))
      config->retrieve_list("module-search-order", b_module.list);
    {
      Enumeration<ModuleInfoEnumeration> els 
	= get_module_info_list(config)->elements();
      const ModuleInfo * entry;
      while ( (entry = els.next()) != 0)
	b_module.list->add(entry->name);
    }
    b_module.init();

    //
    // Retrive size
    //
    str = config->retrieve("size");
    p = str.data.c_str();
    if (p[0] == '+' || p[0] == '-' || p[0] == '<' || p[0] == '>') {
      b_size.req_type = p[0];
      ++p;
    } else {
      b_size.req_type = '+';
    }
    if (!isdigit(p[0]) || !isdigit(p[1]) || p[2] != '\0')
      abort(); //FIXME: create an error condition here
    b_size.requested = atoi(p);
    b_size.init();

    //
    // 
    //

    DictInfoList * dlist = get_dict_info_list(config);
    DictInfoEnumeration * dels = dlist->elements();
    const DictInfo * entry;

    while ( (entry = dels->next()) != 0) {

      b_code  .cur = entry->code;
      b_module.cur = entry->module->name;

      b_jargon.cur = entry->jargon;
    
      b_size.cur_str = entry->size_str;
      b_size.cur     = entry->size;

      //
      // check to see if we got a better match than the current
      // best_match if any
      //

      IsBetter is_better = SameMatch;
      for (int i = 0; i != 4; ++i)
	is_better = better[i]->better_match(is_better);
    
      if (is_better == BetterMatch) {
	for (int i = 0; i != 4; ++i)
	  better[i]->set_best_from_cur();
	best = entry;
      }
    }

    delete dels;

    //
    // set config to best match
    //
    if (best != 0) {
      String main_wl,flags;
      PosibErrBase ret = get_dict_file_name(best, main_wl, flags);
      if (ret.has_err()) {
	delete config;
	return ret;
      }
      config->replace("lang", b_code.best);
      config->replace("language-tag", b_code.best);
      config->replace("master", main_wl.c_str());
      config->replace("master-flags", flags.c_str());
      config->replace("module", b_module.best);
      config->replace("jargon", b_jargon.best);
      config->replace("size", b_size.best_str);
    } else {
      delete config;
      return make_err(no_wordlist_for_lang, code);
    }
    return config;
  }

  PosibErr<Speller *> new_speller(Config * c0) 
  {
    RET_ON_ERR_SET(find_word_list(c0), Config *, c);
    Speller * m = get_speller_class(c);
    PosibErrBase err = m->setup(c);
    if (err.has_err()) {
      delete m;
      return err;
    } else {
      return m;
    }
  }

  void delete_speller(Speller * m) 
  {
    SpellerLtHandle h = ((Speller *)(m))->lt_handle();
    delete m;
    if (h != 0) free_lt_handle(h);
  }
}
