// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#include <vector>

#include "copy_ptr-t.hpp"
#include "data_util.hpp"
#include "emulation.hpp"
#include "errors.hpp"
#include "fstream.hpp"
#include "hash-t.hpp"
#include "hash_simple_string.hpp"
#include "language.hpp"
#include "simple_string.hpp"
#include "writable_base.hpp"

using namespace std;
using namespace aspell;

namespace aspell_default_writable_wl {

  /////////////////////////////////////////////////////////////////////
  // 
  //  WritableWS
  //

  class WritableWS : public WritableBase<WritableWordSet>
  {
  public: //but don't use
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
    typedef hash_multiset<SimpleString,Hash,Equal> WordLookup;
    CopyPtr<WordLookup>                            word_lookup;
    
    typedef vector<const char *>                       RealSoundslikeWordList;
    typedef hash_map<SimpleString,RealSoundslikeWordList> SoundslikeLookup;
    SoundslikeLookup                                      soundslike_lookup;
    
    PosibErr<void> save(FStream &, ParmString);
    PosibErr<void> merge(FStream &, ParmString, Config * config = 0);

  protected:
    void set_lang_hook(Config *) {
      word_lookup.reset(new WordLookup(10, Hash(lang()), Equal(lang())));
    }
    
  public:

    struct ElementsParms;
    struct SoundslikeWordsParms;

    VirEmul * detailed_elements() const;

    Size   size()     const;
    bool   empty()    const;
  
    WritableWS() : WritableBase<WritableWordSet>(".pws", ".per") {}

    PosibErr<void> add(ParmString w);
    PosibErr<void> add(ParmString w, ParmString s);
    PosibErr<void> clear();
    BasicWordInfo lookup (ParmString word, const SensitiveCompare &) const;

    VirEmul * words_w_soundslike(const char * soundslike) const;
    VirEmul * words_w_soundslike(SoundslikeWord soundslike) const;
  
    struct SoundslikeElementsParms;
    VirSoundslikeEmul * soundslike_elements() const;
  };

  struct WritableWS::ElementsParms {
    typedef BasicWordInfo                   Value;
    typedef WordLookup::const_iterator Iterator;
    Iterator end_;
    ElementsParms(Iterator e) : end_(e) {}
    bool endf(Iterator i) const {return i==end_;}
    static Value deref(Iterator i) {return i->c_str();}
    static Value end_state() {return 0;}
  };

  WritableWS::VirEmul * WritableWS::detailed_elements() const {

    return new MakeVirEmulation<ElementsParms>
      (word_lookup->begin(),ElementsParms(word_lookup->end()));
  }

  WritableWS::Size WritableWS::size() const {
    return word_lookup->size();
  }

  bool WritableWS::empty() const {
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

    SoundslikeLookup::const_iterator i = soundslike_lookup.begin();
    SoundslikeLookup::const_iterator e = soundslike_lookup.end();
    
    RealSoundslikeWordList::const_iterator j;
  
    for (;i != e; ++i) {
      for (j = i->second.begin(); j != i->second.end(); ++j) {
	out << *j << '\n';
      }
    }
    return no_err;
  }
  
  PosibErr<void> WritableWS::add(ParmString w) {
    return add(w, lang()->to_soundslike(w));
  }

  PosibErr<void> WritableWS::add(ParmString w, ParmString s) {
    RET_ON_ERR(check_if_valid(*lang(),w));
    SensitiveCompare c(lang());
    if (lookup(w,c)) return no_err;
    const char * w2 = word_lookup->insert(w.str()).first->c_str();
    soundslike_lookup[s.str()].push_back(w2);
    return no_err;
  }
    
  PosibErr<void> WritableWS::clear() {
    word_lookup->clear(); 
    soundslike_lookup.clear();
    return no_err;
  }

  BasicWordInfo WritableWS::lookup (ParmString word,
				    const SensitiveCompare & c) const
  {
    pair<WordLookup::iterator, WordLookup::iterator> 
      p(word_lookup->equal_range(SimpleString(word,1)));
    while (p.first != p.second) {
      if (c(word,p.first->c_str()))
	return p.first->c_str();
      ++p.first;
    }
    return 0;
  }

  struct WritableWS::SoundslikeWordsParms
  {
    typedef BasicWordInfo                               Value;
    typedef RealSoundslikeWordList::const_iterator Iterator;
    Iterator   end_;
    SoundslikeWordsParms(Iterator e) : end_(e) {}
    bool endf(Iterator i) const {return i == end_;}
    Value deref(Iterator i) const {return *i;}
    Value end_state() const {return 0;}
  };

  WritableWS::VirEmul *
  WritableWS::words_w_soundslike(const char * soundslike) const {
    SoundslikeLookup::const_iterator i = 
      soundslike_lookup.find(SimpleString(soundslike,1));
    
    if (i == soundslike_lookup.end()) {
      return new MakeAlwaysEndEmulation<BasicWordInfo>();
    } else {
      return new MakeVirEmulation<SoundslikeWordsParms>
	(i->second.begin(), SoundslikeWordsParms(i->second.end()));
    }
  }

  struct WritableWS::SoundslikeElementsParms {
    typedef SoundslikeWord                   Value;
    typedef SoundslikeLookup::const_iterator Iterator;
    Iterator end_;
    SoundslikeElementsParms(Iterator e) : end_(e) {}
    bool endf(Iterator i) const {return i==end_;}
    static Value deref(Iterator i) {
      return Value(i->first.c_str(),
		   reinterpret_cast<const void *>(&i->second));
    }
    static Value end_state() {return Value(0,0);}
  };
    
    
  WritableWS::VirSoundslikeEmul * WritableWS::soundslike_elements() const {
    return new MakeVirEmulation<SoundslikeElementsParms>
      (soundslike_lookup.begin(), soundslike_lookup.end());
  }

  WritableWS::VirEmul *
  WritableWS::words_w_soundslike(SoundslikeWord word) const {

    const RealSoundslikeWordList * temp 
      = reinterpret_cast<const RealSoundslikeWordList *>
      (word.word_list_pointer);
      
    return new MakeVirEmulation<SoundslikeWordsParms>
      (temp->begin(), SoundslikeWordsParms(temp->end()));
  }

}


#if 0 //don't remove
    void Writable_SoundslikeList::erase(const char* w, ParmStrings) {
      WordList& item = lookup_table.find(s.c_str())->second;
      WordList::iterator i = item.begin();
      WordList::iterator end = item.end();
      while(i != end && *i != w) ++i;
      if (i == end) return;
      item.erase(i);
      if (!item.size()) {
	lookup_table.erase(s.c_str());
      }
    }
#endif

namespace aspell {
  WritableWordSet * new_default_writable_word_set() {
    return new aspell_default_writable_wl::WritableWS();
  }
}
