// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#include <vector>

#include "copy_ptr-t.hpp"
#include "data_util.hpp"
#include "enumeration.hpp"
#include "errors.hpp"
#include "fstream.hpp"
#include "hash-t.hpp"
#include "hash_simple_string.hpp"
#include "language.hpp"
#include "simple_string.hpp"
#include "writable_base.hpp"

using namespace aspeller;
using namespace std;

// FIXME: WritableReplS and WritableWS are very similar and this WritableBase
//   nonsense is probably overly complicated.  Combine both info a single file
//   writable_sets.cpp or something similar....

namespace aspeller_default_writable_wl {

  /////////////////////////////////////////////////////////////////////
  // 
  //  WritableWS
  //

  struct Hash {
    InsensitiveHash f;
    Hash(const Language * l) : f(l) {}
    size_t operator() (const SimpleString & s) const {
      return f(s.c_str());
    }
  };

  struct Equal {
    InsensitiveEqual f;
    Equal(const Language * l) : f(l) {}
    bool operator() (const SimpleString & a, const SimpleString & b) const
    {
      return f(a.c_str(), b.c_str());
    }
  };

  typedef hash_multiset<SimpleString,Hash,Equal>        WordLookup;
  typedef Vector<const char *>                          RealSoundslikeWordList;
  typedef hash_map<SimpleString,RealSoundslikeWordList> SoundslikeLookup;

  class WritableWS : public WritableBase<WritableWordSet>
  {
  public: //but don't use
    CopyPtr<WordLookup> word_lookup;
    SoundslikeLookup    soundslike_lookup_;

    PosibErr<void> save(FStream &, ParmString);
    PosibErr<void> merge(FStream &, ParmString, Config * config = 0);

  protected:
    void set_lang_hook(Config *) {
      word_lookup.reset(new WordLookup(10, Hash(lang()), Equal(lang())));
    }
    
  public:

    WritableWS() : WritableBase<WritableWordSet>(".pws", ".per") {
      have_soundslike = true; fast_lookup = true;
    }

    Size   size()     const;
    bool   empty()    const;
  
    PosibErr<void> add(ParmString w);
    PosibErr<void> add(ParmString w, ParmString s);
    PosibErr<void> clear();

    bool lookup (ParmString word, WordEntry &, const SensitiveCompare &) const;

    bool stripped_lookup(const char * sondslike, WordEntry &) const;

    bool soundslike_lookup(const char * soundslike, WordEntry &) const;
    bool soundslike_lookup(const WordEntry & soundslike, WordEntry &) const;

    WordEntryEnumeration * detailed_elements() const;

    SoundslikeEnumeration * soundslike_elements() const;
  };

  WritableWS::Size WritableWS::size() const 
  {
    return word_lookup->size();
  }

  bool WritableWS::empty() const 
  {
    return word_lookup->empty();
  }

  PosibErr<void> WritableWS::merge(FStream & in, 
				   ParmString file_name, 
				   Config * config)
  {
    typedef PosibErr<void> Ret;
    unsigned int c;
    unsigned int ver;
    String word, sound;

    in >> word;
    if (word == "personal_wl")
      ver = 10;
    else if (word == "personal_ws-1.1")
      ver = 11;
    else 
      return make_err(bad_file_format, file_name);

    in >> word;
    {
      Ret pe = set_check_lang(word, config);
      if (pe.has_err())
	return pe.with_file(file_name);
    }

    in >> c; // not used at the moment
    for (;;) {
      in >> word;
      if (ver == 10)
	in >> sound;
      if (!in) break;
      Ret pe = add(word);
      if (pe.has_err()) {
	clear();
	return pe.with_file(file_name);
      }
    }
    return no_err;
  }

  PosibErr<void> WritableWS::save(FStream & out, ParmString file_name) 
  {
    out << "personal_ws-1.1" << ' ' << lang_name() << ' ' 
        << word_lookup->size() << '\n';

    SoundslikeLookup::const_iterator i = soundslike_lookup_.begin();
    SoundslikeLookup::const_iterator e = soundslike_lookup_.end();
    
    RealSoundslikeWordList::const_iterator j;
  
    for (;i != e; ++i) {
      for (j = i->second.begin(); j != i->second.end(); ++j) {
	out << *j << '\n';
      }
    }
    return no_err;
  }
  
  PosibErr<void> WritableWS::add(ParmString w) {
    return WritableWS::add(w, have_soundslike ? lang()->to_soundslike(w) : "");
  }

  PosibErr<void> WritableWS::add(ParmString w, ParmString s) {
    RET_ON_ERR(check_if_valid(*lang(),w));
    SensitiveCompare c(lang());
    WordEntry we;
    if (WritableWS::lookup(w,we,c)) return no_err;
    const char * w2 = word_lookup->insert(w.str()).first->c_str();
    if (have_soundslike)
      soundslike_lookup_[s.str()].push_back(w2);
    return no_err;
  }

  PosibErr<void> WritableWS::clear() {
    word_lookup->clear(); 
    soundslike_lookup_.clear();
    return no_err;
  }

  bool WritableWS::lookup(ParmString word, WordEntry & o,
			  const SensitiveCompare & c) const
  {
    o.clear();
    pair<WordLookup::iterator, WordLookup::iterator> 
      p(word_lookup->equal_range(SimpleString(word,1)));
    while (p.first != p.second) {
      if (c(word,p.first->c_str())) {
	o.what = WordEntry::Word;
	o.word = p.first->c_str();
	o.aff  = "";
        return true;
      }
      ++p.first;
    }
    return false;
  }

  bool WritableWS::stripped_lookup(const char * sl, WordEntry & o) const
  {
    o.clear();
    pair<WordLookup::iterator, WordLookup::iterator> 
      p(word_lookup->equal_range(SimpleString(sl,1)));
    if (p.first == p.second) return false;
    o.what = WordEntry::Word;
    o.word = p.first->c_str();
    o.aff  = "";
    return true;
    // FIXME: Deal with multiple entries
  }  

  static void soundslike_next(WordEntry * w)
  {
    const char * const * i   = (const char * const *)(w->intr[0]);
    const char * const * end = (const char * const *)(w->intr[1]);
    w->word = *i;
    ++i;
    if (i == end) w->adv_ = 0;
  }

  static void sl_init(const RealSoundslikeWordList * tmp, WordEntry & o)
  {
    o.what = WordEntry::Word;
    const char * const * i   = tmp->pbegin();
    const char * const * end = tmp->pend();
    o.word = *i;
    o.aff  = "";
    ++i;
    if (i != end) {
      o.intr[0] = (void *)i;
      o.intr[1] = (void *)end;
      o.adv_ = soundslike_next;
    }
  }

  bool WritableWS::soundslike_lookup(const WordEntry & word, WordEntry & o) const 
  {
    if (have_soundslike) {
      const RealSoundslikeWordList * tmp 
	= (const RealSoundslikeWordList *)(word.intr[0]);
      o.clear();
      sl_init(tmp, o);
    } else {
      o.what = WordEntry::Word;
      o.word = word.word;
      o.aff  = "";
    }
    return true;
  }

  bool WritableWS::soundslike_lookup(const char * soundslike, WordEntry & o) const {
    if (have_soundslike) {
      o.clear();
      SoundslikeLookup::const_iterator i = 
	soundslike_lookup_.find(SimpleString(soundslike,1));
      if (i == soundslike_lookup_.end()) {
	return false;
      } else {
	sl_init(&(i->second), o);
	return true;
      }
    } else {
      return WritableWS::stripped_lookup(soundslike, o);
    }
  }

  struct SoundslikeElements : public SoundslikeEnumeration {

    typedef SoundslikeLookup::const_iterator Itr;

    Itr i;
    Itr end;

    WordEntry d;

    SoundslikeElements(Itr i0, Itr end0) : i(i0), end(end0) {
      d.what = WordEntry::Soundslike;
    }

    WordEntry * next(int) {
      if (i == end) return 0;
      d.word = i->first.c_str();
      d.intr[0] = (void *)(&i->second);
      ++i;
      return &d;
    }
  };
    
  struct StrippedElements : public SoundslikeEnumeration {

    typedef WordLookup::const_iterator Itr;

    Itr i;
    Itr end;

    WordEntry d;

    StrippedElements(Itr i0, Itr end0) : i(i0), end(end0) {
      d.what = WordEntry::Word;
      d.aff  = "";
    }

    WordEntry * next(int) {
      if (i == end) return 0;
      d.word = i->c_str();
      ++i;
      return &d;
    }
  };
    
  SoundslikeEnumeration * WritableWS::soundslike_elements() const {
    if (have_soundslike)
      return new SoundslikeElements(soundslike_lookup_.begin(), 
				    soundslike_lookup_.end());
    else
      return new StrippedElements(word_lookup->begin(),
				  word_lookup->end());
  }

  struct ElementsParms {
    typedef WordEntry *                Value;
    typedef WordLookup::const_iterator Iterator;
    Iterator end_;
    WordEntry data;
    ElementsParms(Iterator e) : end_(e) {}
    bool endf(Iterator i) const {return i==end_;}
    Value deref(Iterator i) {data.word = i->c_str(); return &data;}
    static Value end_state() {return 0;}
  };

  WritableWS::Enum * WritableWS::detailed_elements() const {
    return new MakeEnumeration<ElementsParms>
      (word_lookup->begin(),ElementsParms(word_lookup->end()));
  }

}

namespace aspeller {
  WritableWordSet * new_default_writable_word_set() {
    return new aspeller_default_writable_wl::WritableWS();
  }
}
