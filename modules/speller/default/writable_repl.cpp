// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#include <vector>

#include "data_util.hpp"
#include "objstack.hpp"
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

namespace aspeller_default_writable_repl {

  /////////////////////////////////////////////////////////////////////
  // 
  //  WritableReplList
  //

  typedef const char * Str;

  struct Hash {
    InsensitiveHash f;
    Hash(const Language * l) : f(l) {}
    size_t operator() (Str s) const {return f(s);}
  };

  struct Equal {
    InsensitiveEqual f;
    Equal(const Language * l) : f(l) {}
    bool operator() (Str a, Str b) const {return f(a, b);}
  };

  typedef hash_multimap<Str,Vector<Str>,Hash,Equal> WordLookup;
  typedef Vector<Str>                               RealSoundslikeWordList;
  typedef hash_map<Str, RealSoundslikeWordList>     SoundslikeLookup;
      
  class WritableReplS : public WritableBase<WritableReplacementSet>
  {
  private:
    StringBuffer         buffer;
    StackPtr<WordLookup> word_lookup;
    SoundslikeLookup     soundslike_lookup_;

    WritableReplS(const WritableReplS&);
    WritableReplS& operator=(const WritableReplS&);

  protected:
    void set_lang_hook(Config *) {
      word_lookup.reset(new WordLookup(10, Hash(lang()), Equal(lang())));
    }

  public:
    WritableReplS() 
      : WritableBase<WritableReplacementSet>(".prepl",".rpl") {
      have_soundslike = true;
      fast_lookup = true;
    }

    Size   size()     const;
    bool   empty()    const;

    bool lookup(ParmString, WordEntry &, const SensitiveCompare &) const;

    bool stripped_lookup(const char * sondslike, WordEntry &) const;

    bool soundslike_lookup(const WordEntry &, WordEntry &) const;
    bool soundslike_lookup(const char * soundslike, WordEntry &) const;

    bool repl_lookup(const WordEntry &, WordEntry &) const;
    bool repl_lookup(const char * word, WordEntry &) const;
      
    WordEntryEnumeration * detailed_elements() const;
    SoundslikeEnumeration * soundslike_elements() const;
      
    PosibErr<void> add(ParmString mis, ParmString cor);
    PosibErr<void> add(ParmString mis, ParmString cor, ParmString s);
    PosibErr<void> clear();

  private:
    PosibErr<void> save(FStream &, ParmString );
    PosibErr<void> merge(FStream &, ParmString , Config * config = 0);
  };

  WritableReplS::Size WritableReplS::size() const 
  {
    return word_lookup->size();
  }

  bool WritableReplS::empty() const 
  {
    return word_lookup->empty();
  }
    
  bool WritableReplS::lookup(ParmString word, WordEntry & o,
			     const SensitiveCompare & c) const
  {
    o.clear();
    pair<WordLookup::iterator, WordLookup::iterator> p(word_lookup->equal_range(word));
    while (p.first != p.second) {
      if (c(word,p.first->first)) {
	o.what = WordEntry::Misspelled;
	o.word = p.first->first;
 	o.intr[0] = (void *)&p.first->second;
        return true;
      }
      ++p.first;
    }
    return false;
  }

  bool WritableReplS::stripped_lookup(const char * sl, WordEntry & o) const
  {
    o.clear();
    pair<WordLookup::iterator, WordLookup::iterator> p(word_lookup->equal_range(sl));
    if (p.first == p.second) return false;
    o.what = WordEntry::Misspelled;
    o.word = p.first->first;
    o.intr[0] = (void *)&p.first->second;
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
    o.what = WordEntry::Misspelled;
    const char * const * i   = tmp->pbegin();
    const char * const * end = tmp->pend();
    o.word = *i;
    ++i;
    if (i != end) {
      o.intr[0] = (void *)i;
      o.intr[1] = (void *)end;
      o.adv_ = soundslike_next;
    } else {
      o.intr[0] = 0;
    }
  }

  bool WritableReplS::soundslike_lookup(const WordEntry & word, WordEntry & o) const 
  {
    if (have_soundslike) {
      const RealSoundslikeWordList * tmp 
	= (const RealSoundslikeWordList *)(word.intr[0]);
      o.clear();
      sl_init(tmp, o);
    } else {
      o.what = WordEntry::Misspelled;
      o.word = word.word;
    }
    return true;
  }

  bool WritableReplS::soundslike_lookup(const char * soundslike, WordEntry & o) const {
    if (have_soundslike) {
      o.clear();
      SoundslikeLookup::const_iterator i = soundslike_lookup_.find(soundslike);
      if (i == soundslike_lookup_.end()) {
	return false;
      } else {
	sl_init(&(i->second), o);
	return true;
      }
    } else {
      return WritableReplS::stripped_lookup(soundslike, o);
    }
  }

  static void repl_next(WordEntry * w)
  {
    const Str * i   = (const Str *)(w->intr[0]);
    const Str * end = (const Str *)(w->intr[1]);
    w->word = *i;
    ++i;
    if (i == end) w->adv_ = 0;
  }

  static void repl_init(const Vector<Str> * tmp, WordEntry & o)
  {
    o.what = WordEntry::Word;
    const Str * i   = tmp->pbegin();
    const Str * end = tmp->pend();
    o.word = *i;
    o.aff  = "";
    ++i;
    if (i != end) {
      o.intr[0] = (void *)i;
      o.intr[1] = (void *)end;
      o.adv_ = repl_next;
    } else {
      o.intr[0] = 0;
    }
  }
  
  bool WritableReplS::repl_lookup(const WordEntry & w, WordEntry & o) const 
  {
    const Vector<Str> * repls;
    if (w.intr[0] && !w.intr[1]) { // the intr are not for the sl iter
      repls = (const Vector<Str> *)w.intr[0];
    } else {
      SensitiveCompare c(lang()); // FIXME: This is not exactly right
      WordEntry tmp;
      WritableReplS::lookup(w.word, tmp, c);
      repls = (const Vector<Str> *)tmp.intr[0];
      if (!repls) return false;
    }
    o.clear();
    repl_init(repls, o);
    return true;
  }

  bool WritableReplS::repl_lookup(const char * word, WordEntry & o) const
  {
    WordEntry w;
    w.word = word;
    return WritableReplS::repl_lookup(w, o);
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
      d.word = i->first;
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
      d.word = i->first;
      ++i;
      return &d;
    }
  };
    
  SoundslikeEnumeration * WritableReplS::soundslike_elements() const {
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
    Value deref(Iterator i) {data.word = i->first; return &data;}
    static Value end_state() {return 0;}
  };

  WritableReplS::Enum * WritableReplS::detailed_elements() const {
    return new MakeEnumeration<ElementsParms>
      (word_lookup->begin(),ElementsParms(word_lookup->end()));
  }

  PosibErr<void> WritableReplS::add(ParmString mis, ParmString cor) 
  {
    return WritableReplS::add(mis, cor, have_soundslike ? lang()->to_soundslike(mis) : "");
  }

  PosibErr<void> WritableReplS::add(ParmString mis, ParmString cor, ParmString sl) 
  {
    Str m, c, s;
    SensitiveCompare cmp(lang()); // FIXME: I don't think this is completely correct
    WordEntry we;

    pair<WordLookup::iterator, WordLookup::iterator> p0(word_lookup->equal_range(mis));
    WordLookup::iterator p = p0.first;

    for (; p != p0.second && !cmp(mis,p->first); ++p);

    if (p == p0.second) {
      m = buffer.dup(mis);
      p = word_lookup->insert(WordLookup::value_type(m,Vector<Str>())).first;
    } else {
      m = p->first;
    }

    for (Vector<Str>::iterator i = p->second.begin(); i != p->second.end(); ++i)
      if (cmp(cor, *i)) return no_err;
    
    c = buffer.dup(cor);
    p->second.push_back(c);

    if (have_soundslike) {
      s = buffer.dup(sl);
      soundslike_lookup_[s].push_back(m);
    }

    return no_err;
  }

  PosibErr<void> WritableReplS::clear() {
    // FIXME: clear buffer
    word_lookup->clear(); 
    soundslike_lookup_.clear();
    return no_err;
  }

  PosibErr<void> WritableReplS::save (FStream & out, ParmString file_name) 
  {
    out << "personal_repl-1.1" << ' ' << lang_name() <<  " 0 \n";
  
    WordLookup::iterator i = word_lookup->begin();
    WordLookup::iterator e = word_lookup->end();
  
    for (;i != e; ++i) 
    {
      for (Vector<Str>::iterator j = i->second.begin(); j != i->second.end(); ++j)
      {
	out << i->first << ' ' << *j << '\n';
      }
    }
    return no_err;
  }

  PosibErr<void> WritableReplS::merge(FStream & in,
				      ParmString file_name, 
				      Config * config)
  {
    typedef PosibErr<void> Ret;
    unsigned int c;
    unsigned int version;
    String word, mis, sound, repl;
    unsigned int num_words, num_repls;

    in >> word;
    if (word == "personal_repl")
      version = 10;
    else if (word == "personal_repl-1.1") 
      version = 11;
    else
      return make_err(bad_file_format, file_name);

    in >> word;

    {
      Ret pe = set_check_lang(word, config);
      if (pe.has_err())
	return pe.with_file(file_name);
    }

    unsigned int num_soundslikes;
    if (version == 10) {
      in >> num_soundslikes;
    }
    in >> c;  // not used at the moment
    in.skipws();

    if (version == 11) {

      do {
	in.getline(mis, ' ');
	if (!in) break;
	in.getline(repl, '\n');
	if (!in) make_err(bad_file_format, file_name);
	WritableReplS::add(mis, repl);
      } while (true);

    } else {

      unsigned int h,i,j;
      for (h=0; h != num_soundslikes; ++h) {
	in >> sound >> num_words;
	for (i = 0; i != num_words; ++i) {
	  in >> mis >> num_repls;
	  in.ignore(); // ignore space
	  for (j = 0; j != num_repls; ++j) {
	    in.getline(repl, ',');
	    WritableReplS::add(mis, repl);
	  }
	}
      }

    }
    return no_err;
  }
}

namespace aspeller {
  WritableReplacementSet * new_default_writable_replacement_set() {
    return new aspeller_default_writable_repl::WritableReplS();
  }
}

