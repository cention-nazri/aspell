// This file is part of The New Aspell
// Copyright (C) 2000-2001 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.

#include <stdlib.h>
#include <typeinfo>

#include "clone_ptr-t.hpp"
#include "config.hpp"
#include "copy_ptr-t.hpp"
#include "data.hpp"
#include "data_id.hpp"
#include "errors.hpp"
#include "language.hpp"
#include "speller_impl.hpp"
#include "string_list.hpp"
#include "suggest.hpp"
#include "tokenizer.hpp"
#include "convert.hpp"
#include "stack_ptr.hpp"

#include "iostream.hpp"

namespace aspeller {
  //
  // data_access functions
  //

  const char * SpellerImpl::lang_name() const {
    return lang_->name();
  }

  //
  // to lower
  //

  char * SpellerImpl::to_lower(char * str) 
  {
    for (char * i = str; *i; ++i)
      *i = lang_->to_lower(*i);
    return str;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // Spell check methods
  //

  PosibErr<void> SpellerImpl::add_to_personal(MutableString word) {
    if (!personal_) return no_err;
    return personal_->add(word);
  }
  
  PosibErr<void> SpellerImpl::add_to_session(MutableString word) {
    if (!session_) return no_err;
    return session_->add(word);
  }

  PosibErr<void> SpellerImpl::clear_session() {
    if (!session_) return no_err;
    return session_->clear();
  }

  PosibErr<void> SpellerImpl::store_replacement(MutableString mis, 
						MutableString cor)
  {
    return SpellerImpl::store_replacement(mis,cor,true);
  }

  PosibErr<void> SpellerImpl::store_replacement(const String & mis, 
						const String & cor, 
						bool memory) 
  {
    if (ignore_repl) return no_err;
    if (!repl_) return no_err;
    String::size_type pos;
    StackPtr<StringEnumeration> sugels(intr_suggest_->suggest(mis.c_str()).elements());
    const char * first_word = sugels->next();
    CheckInfo w1, w2;
    String cor1, cor2;
    String buf;
    bool correct = false;
    if (pos = cor.find(' '), pos == String::npos) {
      cor1 = cor;
      correct = check_affix(cor, w1, 0);
    } else {
      cor1 = (String)cor.substr(0,pos);
      cor2 = (String)cor.substr(pos+1);
      correct = check_affix(cor1, w1, 0) && check_affix(cor2, w2, 0);
    }
    if (correct) {
      String cor_orignal_casing(cor1);
      if (!cor2.empty()) {
 	cor_orignal_casing += cor[pos];
 	cor_orignal_casing += cor2;
      }
      if (first_word == 0 || cor != first_word) {
 	repl_->add_repl(lang().to_lower(buf, mis.str()), cor_orignal_casing);
      }
      
      if (memory && prev_cor_repl_ == mis) 
 	store_replacement(prev_mis_repl_, cor, false);
      
    } else { //!correct
      
      if (memory) {
	 if (prev_cor_repl_ != mis)
 	  prev_mis_repl_ = mis;
 	prev_cor_repl_ = cor;
       }
    }
    return no_err;
  }

  //
  // simple functions
  //

  PosibErr<const WordList *> SpellerImpl::suggest(MutableString word) 
  {
    return &suggest_->suggest(word);
  }

  bool SpellerImpl::check_simple (ParmString w, WordEntry & w0) 
  {
    w0.clear(); // FIXME: is this necessary?
    const char * x = w;
    while (*x != '\0' && (x-w) < static_cast<int>(ignore_count)) ++x;
    if (*x == '\0') {w0.word = w; return true;}
    WS::const_iterator i   = check_ws.begin();
    WS::const_iterator end = check_ws.end();
    do {
      if (i->dict->lookup(w, w0, i->compare)) return true;
      ++i;
    } while (i != end);
    return false;
  };

  bool SpellerImpl::check_affix(ParmString word, CheckInfo & ci, GuessInfo * gi)
  {
    WordEntry w;
    bool res = check_simple(word, w);
    if (res) {ci.word = w.word; return true;}
    if (affix_compress) {
      res = lang_->affix()->affix_check(LookupInfo(this, LookupInfo::Word), word, ci, 0);
      if (res) return true;
    }
    if (affix_info && gi) {
      lang_->affix()->affix_check(LookupInfo(this, LookupInfo::Guess), word, ci, gi);
    }
    return false;
  }

  PosibErr<bool> SpellerImpl::check(char * word, char * word_end, 
                                    /* it WILL modify word */
				    unsigned int run_together_limit,
				    CheckInfo * ci, GuessInfo * gi)
  {
    assert(run_together_limit <= 8); // otherwise it will go above the 
                                     // bounds of the word array
    clear_check_info(*ci);
    bool res = check_affix(word, *ci, gi);
    if (res) return true;
    if (run_together_limit <= 1) return false;
    for (char * i = word + run_together_start_len_; 
	 i <= word_end - run_together_start_len_;
	 ++i) 
      {
	char t = *i;
	*i = '\0';
        //FIXME: clear ci, gi?
	res = check_affix(word, *ci, gi);
	*i = t;
	if (!res) continue;
	if (check(i, word_end, run_together_limit - 1, ci + 1, 0)) {
          ci->next = ci + 1;
	  return true;
        }
      }
    return false;
  }
  

  //////////////////////////////////////////////////////////////////////
  //
  // Word list managment methods
  //
  
  PosibErr<void> SpellerImpl::save_all_word_lists() {
    SpellerDict * i = dicts_;
    for (; i; i = i->next) {
      if  (i->save_on_saveall)
	RET_ON_ERR(i->dict->synchronize());
    }
    return no_err;
  }
  
  int SpellerImpl::num_wordlists() const {
    return 0; //FIXME
  }

  SpellerImpl::WordLists SpellerImpl::wordlists() const {
    return 0; //FIXME
    //return MakeEnumeration<DataSetCollection::Parms>(wls_->begin(), DataSetCollection::Parms(wls_->end()));
  }

  PosibErr<const WordList *> SpellerImpl::personal_word_list() const {
    return static_cast<const WordList *>(personal_);
  }

  PosibErr<const WordList *> SpellerImpl::session_word_list() const {
    return static_cast<const WordList *>(session_);
  }

  PosibErr<const WordList *> SpellerImpl::main_word_list() const {
    return dynamic_cast<const WordList *>(main_);
  }

  const SpellerDict * SpellerImpl::locate (const Dict::Id & id) const
  {
    for (const SpellerDict * i = dicts_; i; i = i->next)
      if (i->dict->id() == id) return i;
    return 0;
  }

  void SpellerImpl::add_dict(SpellerDict * wc)
  {
    Dict * w = wc->dict;
    assert(locate(w->id()) == 0);

    if (!lang_) 
    {
      lang_.copy(w->lang());
      config_->replace("lang", lang_name());
      config_->replace("language-tag", lang_name());
    }

    // add to master list
    wc->next = dicts_;
    dicts_ = wc;

    // check if it has a special_id and act accordingly
    switch (wc->special_id) {
    case main_id:
      assert(main_ == 0);
      main_ = w;
      break;
    case personal_id:
      assert(personal_ == 0);
      personal_ = w;
      break;
    case session_id:
      assert(session_ == 0);
      session_ = w;
      break;
    case personal_repl_id:
      assert(repl_ == 0);
      repl_ = w;
      break;
    case none_id:
      break;
    }
  }

  //////////////////////////////////////////////////////////////////////
  //
  // Config Notifier
  //

  struct UpdateMember {
    const char * name;
    enum Type {String, Int, Bool, Add, Rem, RemAll};
    Type type;
    union Fun {
      typedef PosibErr<void> (*WithStr )(SpellerImpl *, const char *);
      typedef PosibErr<void> (*WithInt )(SpellerImpl *, int);
      typedef PosibErr<void> (*WithBool)(SpellerImpl *, bool);
      WithStr  with_str;
      WithInt  with_int;
      WithBool with_bool;
      Fun() {}
      Fun(WithStr  m) : with_str (m) {}
      Fun(WithInt  m) : with_int (m) {}
      Fun(WithBool m) : with_bool(m) {}
      PosibErr<void> call(SpellerImpl * m, const char * val) const 
	{return (*with_str) (m,val);}
      PosibErr<void> call(SpellerImpl * m, int val)          const 
	{return (*with_int) (m,val);}
      PosibErr<void> call(SpellerImpl * m, bool val)         const 
	{return (*with_bool)(m,val);}
    } fun;
    typedef SpellerImpl::ConfigNotifier CN;
  };

  template <typename T>
  PosibErr<void> callback(SpellerImpl * m, const KeyInfo * ki, T value, 
			  UpdateMember::Type t);
  
  class SpellerImpl::ConfigNotifier : public Notifier {
  private:
    SpellerImpl * speller_;
  public:
    ConfigNotifier(SpellerImpl * m) 
      : speller_(m) 
    {}

    PosibErr<void> item_updated(const KeyInfo * ki, int value) {
      return callback(speller_, ki, value, UpdateMember::Int);
    }
    PosibErr<void> item_updated(const KeyInfo * ki, bool value) {
      return callback(speller_, ki, value, UpdateMember::Bool);
    }
    PosibErr<void> item_updated(const KeyInfo * ki, ParmString value) {
      return callback(speller_, ki, value, UpdateMember::String);
    }

    static PosibErr<void> ignore(SpellerImpl * m, int value) {
      m->ignore_count = value;
      return no_err;
    }
    static PosibErr<void> ignore_accents(SpellerImpl * m, bool value) {
      abort(); return no_err;
    }
    static PosibErr<void> ignore_case(SpellerImpl * m, bool value) {
      abort(); return no_err;
    }
    static PosibErr<void> ignore_repl(SpellerImpl * m, bool value) {
      m->ignore_repl = value;
      return no_err;
    }
    static PosibErr<void> save_repl(SpellerImpl * m, bool value) {
      // FIXME
      // m->save_on_saveall(DataSet::Id(&m->personal_repl()), value);
      abort(); return no_err;
    }
    static PosibErr<void> sug_mode(SpellerImpl * m, const char * mode) {
      RET_ON_ERR(m->suggest_->set_mode(mode));
      RET_ON_ERR(m->intr_suggest_->set_mode(mode));
      return no_err;
    }
    static PosibErr<void> run_together(SpellerImpl * m, bool value) {
      m->unconditional_run_together_ = value;
      return no_err;
    }
    static PosibErr<void> run_together_limit(SpellerImpl * m, int value) {
      if (value > 8) {
	m->config()->replace("run-together-limit", "8");
	// will loop back
      } else {
	m->run_together_limit_ = value;
      }
      return no_err;
    }
    static PosibErr<void> run_together_min(SpellerImpl * m, int value) {
      m->run_together_min_ = value;
      if (m->unconditional_run_together_ 
	  && m->run_together_min_ < m->run_together_start_len_)
	m->run_together_start_len_ = m->run_together_min_;
      return no_err;
    }
    
  };

  static UpdateMember update_members[] = 
  {
    {"ignore",         UpdateMember::Int,     UpdateMember::CN::ignore}
    ,{"ignore-accents",UpdateMember::Bool,    UpdateMember::CN::ignore_accents}
    ,{"ignore-case",   UpdateMember::Bool,    UpdateMember::CN::ignore_case}
    ,{"ignore-repl",   UpdateMember::Bool,    UpdateMember::CN::ignore_repl}
    ,{"save-repl",     UpdateMember::Bool,    UpdateMember::CN::save_repl}
    ,{"sug-mode",      UpdateMember::String,  UpdateMember::CN::sug_mode}
    ,{"run-together",  
	UpdateMember::Bool,    
	UpdateMember::CN::run_together}
    ,{"run-together-limit",  
	UpdateMember::Int,    
	UpdateMember::CN::run_together_limit}
    ,{"run-together-min",  
	UpdateMember::Int,    
	UpdateMember::CN::run_together_min}
  };

  template <typename T>
  PosibErr<void> callback(SpellerImpl * m, const KeyInfo * ki, T value, 
			  UpdateMember::Type t) 
  {
    const UpdateMember * i
      = update_members;
    const UpdateMember * end   
      = i + sizeof(update_members)/sizeof(UpdateMember);
    while (i != end) {
      if (strcmp(ki->name, i->name) == 0) {
	if (i->type == t) {
	  RET_ON_ERR(i->fun.call(m, value));
	  break;
	}
      }
      ++i;
    }
    return no_err;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // SpellerImpl inititization members
  //

  SpellerImpl::SpellerImpl() 
    : Speller(0) /* FIXME */, ignore_repl(true), 
      dicts_(0), personal_(0), session_(0), repl_(0), main_(0),
      guess_info(7)
  {}

  inline void add_dicts(SpellerImpl * sp, LocalDictList & d)
  {
    for (;!d.empty(); d.pop())
    {
      if (!sp->locate(d.last().dict->id())) {
        sp->add_dict(new SpellerDict(d.last()));
      }
    }
  }

  PosibErr<void> SpellerImpl::setup(Config * c) {
    assert (config_ == 0);
    config_.reset(c);

    ignore_repl = config_->retrieve_bool("ignore-repl");
    ignore_count = config_->retrieve_int("ignore");

    LocalDict res;
    LocalDictList to_add;
    RET_ON_ERR(add_data_set(config_->retrieve("master-path"), *config_, res, &to_add, this));
    add_dicts(this, to_add);

    use_soundslike = true;

    for (SpellerDict * i = dicts_; i; i = i->next) {
      
      if (i->dict->basic_type == Dict::basic_dict)
        use_soundslike = use_soundslike && i->dict->have_soundslike;
    }

    StringList extra_dicts;
    config_->retrieve_list("extra-dicts", &extra_dicts);
    StringListEnumeration els = extra_dicts.elements_obj();
    const char * dict_name;
    while ( (dict_name = els.next()) != 0) {
      RET_ON_ERR(add_data_set(dict_name,*config_, res, &to_add, this));
      add_dicts(this, to_add);
    }

    bool use_other_dicts = config_->retrieve_bool("use-other-dicts");

    if (use_other_dicts && !personal_)
    {
      Dictionary * temp;
      temp = new_default_writable_dict();
      temp->have_soundslike = use_soundslike;
      PosibErrBase pe = temp->load(config_->retrieve("personal-path"),*config_);
      if (pe.has_err(cant_read_file))
	temp->set_check_lang(lang_name(), config_);
      else if (pe.has_err())
	return pe;
      add_dict(new SpellerDict(temp, lang_, *config_, personal_id));
    }
    
    if (use_other_dicts && !session_)
    {
      Dictionary * temp;
      temp = new_default_writable_dict();
      temp->have_soundslike = use_soundslike;
      temp->set_check_lang(lang_name(), config_);
      add_dict(new SpellerDict(temp, lang_, *config_, session_id));
    }
     
    if (use_other_dicts && !repl_)
    {
      ReplacementDict * temp = new_default_replacement_dict();
      temp->have_soundslike = use_soundslike;
      PosibErrBase pe = temp->load(config_->retrieve("repl-path"),*config_);
      if (pe.has_err(cant_read_file))
	temp->set_check_lang(lang_name(), config_);
      else if (pe.has_err())
	return pe;
      add_dict(new SpellerDict(temp, lang_, *config_, personal_repl_id));
    }

    const char * sys_enc = lang_->charset();
    if (!config_->have("encoding"))
      config_->replace("encoding", sys_enc);
    String user_enc = config_->retrieve("encoding");

    PosibErr<Convert *> conv;
    conv = new_convert(*c, user_enc, sys_enc);
    if (conv.has_err()) return conv;
    to_internal_.reset(conv);
    conv = new_convert(*c, sys_enc, user_enc);
    if (conv.has_err()) return conv;
    from_internal_.reset(conv);

    unconditional_run_together_ = config_->retrieve_bool("run-together");

    run_together_limit_  = config_->retrieve_int("run-together-limit");
    if (run_together_limit_ > 8) {
      config_->replace("run-together-limit", "8");
      run_together_limit_ = 8;
    }
    run_together_min_    = config_->retrieve_int("run-together-min");

    if (unconditional_run_together_ 
	&& run_together_min_ < run_together_start_len_)
      run_together_start_len_ = run_together_min_;
      
    config_->add_notifier(new ConfigNotifier(this));

    config_->set_attached(true);

    affix_info = lang_->affix();

    //
    // setup word set lists
    //

    typedef Vector<SpellerDict *> AllWS; AllWS all_ws;
    for (SpellerDict * i = dicts_; i; i = i->next) {
      if (i->dict->basic_type == Dict::basic_dict) {
        all_ws.push_back(i);
      }
    }
    
    const std::type_info * ti = 0;
    while (!all_ws.empty())
    {
      AllWS::iterator i0 = all_ws.end();
      int max = -2;
      AllWS::iterator i = all_ws.begin();
      for (; i != all_ws.end(); ++i)
      {
        const Dictionary * ws = (*i)->dict;
        if (ti && *ti != typeid(*ws)) continue;
        if ((int)ws->size() > max) {max = ws->size(); i0 = i;}
      }

      if (i0 == all_ws.end()) {ti = 0; continue;}

      SpellerDict * cur = *i0;

      all_ws.erase(i0);

      ti = &typeid(*cur->dict);

      WSInfo inf;
      inf.dict = cur->dict;
      inf.set(*cur);

      if (cur->use_to_check) {
        check_ws.push_back(inf);
        if (inf.dict->affix_compressed) affix_ws.push_back(inf);
      }
      if (cur->use_to_suggest) {
        suggest_ws.push_back(inf);
        if (inf.dict->affix_compressed) suggest_affix_ws.push_back(inf);
      }
    }
    fast_scan   = suggest_ws.front().dict->fast_scan;
    fast_lookup = suggest_ws.front().dict->fast_lookup;
    affix_compress = !affix_ws.empty();

    //
    // Setup suggest
    //

    suggest_.reset(new_default_suggest(this));
    intr_suggest_.reset(new_default_suggest(this));

    return no_err;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // SpellerImpl destrution members
  //

  SpellerImpl::~SpellerImpl() {
    while (dicts_) {
      SpellerDict * next = dicts_->next;
      delete dicts_;
      dicts_ = next;
    }
  }

  //////////////////////////////////////////////////////////////////////
  //
  // SpellerImple setup tokenizer method
  //

  void SpellerImpl::setup_tokenizer(Tokenizer * tok)
  {
    for (int i = 0; i != 256; ++i) 
    {
      tok->char_type_[i].word   = lang_->is_alpha(i);
      tok->char_type_[i].begin  = lang_->special(i).begin;
      tok->char_type_[i].middle = lang_->special(i).middle;
      tok->char_type_[i].end    = lang_->special(i).end;
    }
    tok->conv_ = to_internal_;
  }


  //////////////////////////////////////////////////////////////////////
  //
  //
  //

  SpellerDict::SpellerDict(LocalDict & d) 
    : LocalDict(d), special_id(none_id), next(0) 
  {
    switch (dict->basic_type) {
    case Dict::basic_dict:
      use_to_check = true;
      use_to_suggest = true;
      break;
    case Dict::replacement_dict:
      use_to_check = false;
      use_to_suggest = true;
    case Dict::multi_dict:
      break;
    default:
      abort();
    }
    save_on_saveall = false;
  }

  SpellerDict::SpellerDict(Dict * w, const Language * l, const Config & c, SpecialId id)
    : next(0) 
  {
    set(l, c);
    dict = w;
    special_id = id;
    switch (id) {
    case main_id:
      if (dict->basic_type == Dict::basic_dict) {

	use_to_check    = true;
	use_to_suggest  = true;
	save_on_saveall = false;

      } else if (dict->basic_type == Dict::replacement_dict) {
	
	use_to_check    = false;
	use_to_suggest  = false;
	save_on_saveall = false;
	
      } else {
	
	abort();
	
      }
      break;
    case personal_id:
      use_to_check = true;
      use_to_suggest = true;
      save_on_saveall = true;
      break;
    case session_id:
      use_to_check = true;
      use_to_suggest = true;
      save_on_saveall = false;
      break;
    case personal_repl_id:
      use_to_check = false;
      use_to_suggest = true;
      save_on_saveall = c.retrieve_bool("save-repl");
      break;
    case none_id:
      break;
    }
  }

  extern "C"
  Speller * libaspell_speller_default_LTX_new_speller_class(SpellerLtHandle)
  {
    return new SpellerImpl();
  }
}

