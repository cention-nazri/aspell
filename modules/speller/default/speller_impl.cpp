// This file is part of The New Aspell
// Copyright (C) 2000-2001 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.

#include <stdlib.h>

#include "aspeller.hpp"
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
    DataSetCollection::Iterator i = wls_->locate(personal_id);
    if (i == wls_->end()) return no_err;
    return static_cast<WritableWordSet *>(i->data_set)->add(word.str());
  }
  
  PosibErr<void> SpellerImpl::add_to_session(MutableString word) {
    DataSetCollection::Iterator i = wls_->locate(session_id);
    if (i == wls_->end()) return no_err;
    return static_cast<WritableWordSet *>(i->data_set)->add(word.str());
  }

  PosibErr<void> SpellerImpl::clear_session() {
    DataSetCollection::Iterator i = wls_->locate(session_id);
    if (i == wls_->end()) return no_err;
    return static_cast<WritableWordSet *>(i->data_set)->clear();
  }

  PosibErr<void> SpellerImpl::store_replacement(MutableString mis, 
						MutableString cor)
  {
    return store_replacement(mis.str(),cor.str(), true);
  }


  PosibErr<void> SpellerImpl::store_replacement(const String & mis, 
						const String & cor, 
						bool memory) 
  {
    if (ignore_repl) return no_err;
    DataSetCollection::Iterator i = wls_->locate(personal_repl_id);
    if (i == wls_->end()) return no_err;
    String::size_type pos;
    Enumeration<StringEnumeration> sugels 
      = intr_suggest_->suggest(mis.c_str()).elements();
    const char * first_word = sugels.next();
    const char * w1;
    const char * w2 = 0;
    if (pos = cor.find(' '), pos == String::npos 
	? (w1 =check_simple(cor).word) != 0
	: ((w1 = check_simple((String)cor.substr(0,pos)).word) != 0
	   && (w2 = check_simple((String)cor.substr(pos+1)).word) != 0) ) { 
      // cor is a correct spelling
      String cor_orignal_casing(w1);
      if (w2 != 0) {
	cor_orignal_casing += cor[pos];
	cor_orignal_casing += w2;
      }
      if (first_word == 0 || cor != first_word) {
	static_cast<WritableReplacementSet *>(i->data_set)
	  ->add(aspeller::to_lower(lang(), mis), 
		cor_orignal_casing);
      }
      
      if (memory && prev_cor_repl_ == mis) 
	store_replacement(prev_mis_repl_, cor, false);
      
    } else { // cor is not a correct spelling
      
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
    return &suggest_->suggest(word.str());
  }
  
  SpellerImpl::SpecialId SpellerImpl::check_id(const DataSet::Id & wl) const {
    return wls_->locate(wl)->special_id;
  }

  bool SpellerImpl::use_to_check(const DataSet::Id & wl) const 
  {
    return wls_->locate(wl)->use_to_check;
  }

  void SpellerImpl::use_to_check(const DataSet::Id & wl, bool v) {
    wls_->locate(wl)->use_to_check = v;
  }

  bool SpellerImpl::use_to_suggest(const DataSet::Id & wl) const {
    return wls_->locate(wl)->use_to_suggest;
  }

  void SpellerImpl::use_to_suggest(const DataSet::Id & wl, bool v) {
    wls_->locate(wl)->use_to_suggest = v;
  }

  bool SpellerImpl::save_on_saveall(const DataSet::Id & wl) const {
    return wls_->locate(wl)->save_on_saveall;
  }

  void SpellerImpl::save_on_saveall(const DataSet::Id & wl, bool v) {
    wls_->locate(wl)->save_on_saveall = v;
  }

  bool SpellerImpl::own(const DataSet::Id & wl) const {
    return wls_->locate(wl)->own;
  }

  void SpellerImpl::own(const DataSet::Id & wl, bool v) {
    wls_->locate(wl)->own = v;
  }

  BasicWordInfo SpellerImpl::check_simple (ParmString w) {
    const char * x = w;
    BasicWordInfo w0;
    while (*x != '\0' && (x-w) < static_cast<int>(ignore_count)) ++x;
    if (*x == '\0') return w.str();
    DataSetCollection::ConstIterator i   = wls_->begin();
    DataSetCollection::ConstIterator end = wls_->end();
    for (; i != end; ++i) {
      if  (i->use_to_check && 
	   i->data_set->basic_type == DataSet::basic_word_set &&
	   (w0 = static_cast<const BasicWordSet *>(i->data_set)
	    ->lookup(w,i->local_info.compare))
	   )
	return w0;
    }
    return 0;
  };

  PosibErr<bool> SpellerImpl::check(char * word, char * word_end, 
                                    /* it WILL modify word */
				    unsigned int run_together_limit,
				    CompoundInfo::Position pos,
				    SingleWordInfo * words)
  {
    assert(run_together_limit <= 8); // otherwise it will go above the 
                                     // bounds of the word array
    words[0].clear();
    BasicWordInfo w = check_simple(word);
    if (w) {
      if (pos == CompoundInfo::Orig) {
	words[0] = w.word;
	words[1].clear();
	return true;
      }
      bool check_if_valid = !(unconditional_run_together_ 
			      && strlen(word) >= run_together_min_);
      if (!check_if_valid || w.compound.compatible(pos)) { 
	words[0] = w.word;
	words[1].clear();
	return true;
      } else {
	return false;
      }
    }
    
    if (run_together_limit <= 1 
	|| (!unconditional_run_together_ && !run_together_specified_))
      return false;
    for (char * i = word + run_together_start_len_; 
	 i <= word_end - run_together_start_len_;
	 ++i) 
      {
	char t = *i;
	*i = '\0';
	BasicWordInfo s = check_simple(word);
	*i = t;
	if (!s) continue;
	CompoundInfo c = s.compound;
	CompoundInfo::Position end_pos = new_position(pos, CompoundInfo::End);
	char m = run_together_middle_[c.mid_char()];
	//
	// FIXME: Deal with casing of the middle character properly
	//        if case insentate than it can be anything
	//        otherwise it should match the case of previous
	//        letter
	//
	bool check_if_valid = !(unconditional_run_together_ 
				&& i - word >= static_cast<int>(run_together_min_));
	if (check_if_valid) {
	  CompoundInfo::Position beg_pos = new_position(pos, CompoundInfo::Beg);
	  if (!c.compatible(beg_pos)) 
	    continue;
	  if (c.mid_required() && *i != m)
	    continue;
	}
	words[0].set(s.word, *i == m ? m : '\0');
	words[1].clear();
	if ((!check_if_valid || !c.mid_required()) // if check then !s.mid_required() 
	    && check(i, word_end, run_together_limit - 1, end_pos, words + 1))
	  return true;
	if ((check_if_valid ? *i == m : strchr(run_together_middle_, *i) != 0) 
	    && word_end - (i + 1) >= static_cast<int>(run_together_min_)) {
	  if (check(i+1, word_end, run_together_limit - 1, end_pos, words + 1))
	    return true;
	  else // already checked word (i+1) so no need to check it again
	    ++i;
	}
      }
    words[0].clear();
    return false;
  }
  

  //////////////////////////////////////////////////////////////////////
  //
  // Word list managment methods
  //
  
  PosibErr<void> SpellerImpl::save_all_word_lists() {
    DataSetCollection::Iterator i   = wls_->begin();
    DataSetCollection::Iterator end = wls_->end();
    WritableDataSet * wl;
    for (; i != end; ++i) {
      if  (i->save_on_saveall && 
	   (wl = dynamic_cast<WritableDataSet *>(i->data_set)))
	RET_ON_ERR(wl->synchronize());
    }
    return no_err;
  }

  int SpellerImpl::num_wordlists() const {
    return wls_->wordlists_.size();
  }

  SpellerImpl::WordLists SpellerImpl::wordlists() const {
    return WordLists(MakeVirEnumeration<DataSetCollection::Parms>
		     (wls_->begin(), DataSetCollection::Parms(wls_->end())));
  }

  bool SpellerImpl::have(const DataSet::Id &to_find) const {
    return wls_->locate(to_find) != wls_->end();
  }

  LocalWordSet SpellerImpl::locate(const DataSet::Id &to_find) {
    DataSetCollection::Iterator i = wls_->locate(to_find);
    LocalWordSet ws;
    if (i == wls_->end()) {
      return LocalWordSet();
    } else {
      return LocalWordSet(static_cast<LoadableDataSet *>(i->data_set), 
			  i->local_info);
    }
    return ws;
  }

  bool SpellerImpl::have(SpellerImpl::SpecialId to_find) const {
    return wls_->locate(to_find) != wls_->end();
  }

  PosibErr<const WordList *> SpellerImpl::personal_word_list() const {
    return 
      static_cast<const WordList *>
      (static_cast<const BasicWordSet *>
       (wls_->locate(personal_id)->data_set));
  }

  PosibErr<const WordList *> SpellerImpl::session_word_list() const {
    return 
      static_cast<const WordList *>
      (static_cast<const BasicWordSet *>
       (wls_->locate(session_id)->data_set));
  }

  PosibErr<const WordList *> SpellerImpl::main_word_list() const {
    return 
      static_cast<const WordList *>
      (static_cast<const BasicWordSet *>
       (wls_->locate(main_id)->data_set));
  }

  bool SpellerImpl::attach(DataSet * w, const LocalWordSetInfo * li) {
    DataSetCollection::Iterator i = wls_->locate(w);
    if (i != wls_->end()) {
      return false;
    } else {
      if (!lang_) 
	{
	  lang_.reset(new Language(*w->lang()));
	}
      w->attach(*lang_);
      DataSetCollection::Item wc(w);
      wc.set_sensible_defaults();
      if (li == 0) {
	wc.local_info.set(lang_, config_);
      } else {
	wc.local_info = *li;
	wc.local_info.set_language(lang_);
      }
      wls_->wordlists_.push_back(wc);
      return true;
    }
  }

  bool SpellerImpl::steal(DataSet * w, const LocalWordSetInfo * li) {
    bool ret = attach(w,li);
    own(w, true);
    return ret;
  }

  bool SpellerImpl::detach(const DataSet::Id &w) {
    DataSetCollection::Iterator to_del = wls_->locate(w);
    if (to_del == wls_->wordlists_.end()) return false;
    to_del->data_set->detach();
    wls_->wordlists_.erase(to_del);
    return true;
  }  

  bool SpellerImpl::destroy(const DataSet::Id & w) {
    DataSetCollection::Iterator to_del = wls_->locate(w);
    if (to_del == wls_->wordlists_.end()) return false;
    assert(to_del->own);
    delete to_del->data_set;
    wls_->wordlists_.erase(to_del);
    return true;
  }

  void SpellerImpl::change_id(const DataSet::Id & w , SpecialId id) {
    DataSetCollection::Iterator to_change = wls_->locate(w);

    assert(to_change != wls_->end());

    assert (id == none_id || !have(id));
    
    switch (id) {
    case main_id:
      if (dynamic_cast<BasicWordSet *>(to_change->data_set)) {

	to_change->use_to_check    = true;
	to_change->use_to_suggest  = true;
	to_change->save_on_saveall = false;

      } else if (dynamic_cast<BasicMultiSet *>(to_change->data_set)) {
	
	to_change->use_to_check    = false;
	to_change->use_to_suggest  = false;
	to_change->save_on_saveall = false;
	
      } else {
	
	abort();
	
      }
      break;
    case personal_id:
      assert(dynamic_cast<WritableWordSet *>(to_change->data_set));
      to_change->use_to_check = true;
      to_change->use_to_suggest = true;
      to_change->save_on_saveall = true;
      break;
    case session_id:
      assert(dynamic_cast<WritableWordSet *>(to_change->data_set));
      to_change->use_to_check = true;
      to_change->use_to_suggest = true;
      to_change->save_on_saveall = false;
      break;
    case personal_repl_id:
      assert (dynamic_cast<BasicReplacementSet *>(to_change->data_set));
      to_change->use_to_check = false;
      to_change->use_to_suggest = true;
      to_change->save_on_saveall = config_->retrieve_bool("save-repl");
      break;
    case none_id:
      break;
    }
    to_change->special_id = id;
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
      abort();
    }
    static PosibErr<void> ignore_case(SpellerImpl * m, bool value) {
      abort();
    }
    static PosibErr<void> ignore_repl(SpellerImpl * m, bool value) {
      m->ignore_repl = value;
      return no_err;
    }
    static PosibErr<void> save_repl(SpellerImpl * m, bool value) {
      // FIXME
      // m->save_on_saveall(DataSet::Id(&m->personal_repl()), value);
      abort();
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
    : Speller(0) /* FIXME */, ignore_repl(true)
  {}

  PosibErr<void> SpellerImpl::setup(Config * c) {
    assert (config_ == 0);
    config_.reset(c);
    //config_->read_in(&override); // FIXME

    ignore_repl = config_->retrieve_bool("ignore-repl");
    ignore_count = config_->retrieve_int("ignore");

    wls_.reset(new DataSetCollection());

    RET_ON_ERR_SET(add_data_set(config_->retrieve("master-path"), *config_, this),
		   LoadableDataSet *, ltemp);
    
    change_id(ltemp, main_id);

    StackPtr<StringList> extra_dicts(new_string_list());
    config_->retrieve_list("extra-dicts", extra_dicts);
    Enumeration<StringEnumeration> els = extra_dicts->elements();
    const char * dict_name;
    while ( (dict_name = els.next()) != 0)
      RET_ON_ERR(add_data_set(dict_name,*config_, this));
    
    {
      BasicWordSet * temp;
      temp = new_default_writable_word_set();
      PosibErrBase pe = temp->load(config_->retrieve("personal-path"),config_);
      if (pe.has_err(cant_read_file))
	temp->set_check_lang(lang_name(), config_);
      else if (pe.has_err())
	return pe;
      steal(temp);
      change_id(temp, personal_id);
    }
    
    {
      BasicWordSet * temp;
      temp = new_default_writable_word_set();
      temp->set_check_lang(lang_name(), config_);
      steal(temp);
      change_id(temp, session_id);
    }
     
    {
      BasicReplacementSet * temp = new_default_writable_replacement_set();
      PosibErrBase pe = temp->load(config_->retrieve("repl-path"),config_);
      if (pe.has_err(cant_read_file))
	temp->set_check_lang(lang_name(), config_);
      else if (pe.has_err())
	return pe;
      steal(temp);
      change_id(temp, personal_repl_id);
    }


    const char * sys_enc = lang_->charset();
    if (!config_->have("encoding"))
      config_->replace("encoding", sys_enc);
    String user_enc = config_->retrieve("encoding");

    PosibErr<Convert *> conv;
    conv = new_convert(*c, user_enc, sys_enc);
    if (conv.has_err()) return conv;
    to_encoded_.reset(conv);
    conv = new_convert(*c, sys_enc, user_enc);
    if (conv.has_err()) return conv;
    from_encoded_.reset(conv);

    unconditional_run_together_ = config_->retrieve_bool("run-together");
    run_together_specified_     = config_->retrieve_bool("run-together-specified");
    run_together_middle_        = lang().mid_chars();

    run_together_limit_  = config_->retrieve_int("run-together-limit");
    if (run_together_limit_ > 8) {
      config_->replace("run-together-limit", "8");
      run_together_limit_ = 8;
    }
    run_together_min_    = config_->retrieve_int("run-together-min");

    run_together_start_len_ = config_->retrieve_int("run-together-specified");
    if (unconditional_run_together_ 
	&& run_together_min_ < run_together_start_len_)
      run_together_start_len_ = run_together_min_;
      
    suggest_.reset(new_default_suggest(this));
    intr_suggest_.reset(new_default_suggest(this));

    config_->add_notifier(new ConfigNotifier(this));

    config_->set_attached(true);
    return no_err;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // SpellerImpl destrution members
  //

  SpellerImpl::~SpellerImpl() {
    DataSetCollection::Iterator i   = wls_->begin();
    DataSetCollection::Iterator end = wls_->end();
    for (; i != end; ++i) {
      if (i->own && i->data_set)
	delete i->data_set;
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
    tok->to_encoded_ = to_encoded_;
  }


  //////////////////////////////////////////////////////////////////////
  //
  //
  //

  void SpellerImpl::DataSetCollection::Item::set_sensible_defaults()
  {
    switch (data_set->basic_type) {
    case DataSet::basic_word_set:
      use_to_check = true;
      use_to_suggest = true;
      break;
    case DataSet::basic_replacement_set:
      use_to_check = false;
      use_to_suggest = true;
    case DataSet::basic_multi_set:
      break;
    default:
      abort();
    }
  }

  extern "C"
  Speller * libaspell_speller_default_LTX_new_speller_class(SpellerLtHandle)
  {
    return new SpellerImpl();
  }
}

namespace acommon {
  template class CopyPtr<aspeller::Language>;
}

