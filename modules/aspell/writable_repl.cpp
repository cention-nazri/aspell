// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#include <vector>

#include "data_util.hpp"
#include "emulation.hpp"
#include "errors.hpp"
#include "fstream.hpp"
#include "hash-t.hpp"
#include "hash_simple_string.hpp"
#include "language.hpp"
#include "simple_string.hpp"
#include "writable_base.hpp"

using namespace aspell;

namespace aspell_default_writable_repl {

  /////////////////////////////////////////////////////////////////////
  // 
  //  WritableReplList
  //

  // LookupTable looks like this
  // HashMap:
  //    key:   soundslike (for misspelled word)
  //    value: vector: RealReplacementList:
  //                     misspelled_word()
  //                     begin() | 
  //                     end()   | Correct spelling(s) for misspelled word.
  //                     size()  |


  class WritableReplS : public WritableBase<WritableReplacementSet>
  {
  public: // but don't use
      
    class RealReplacementList {
      vector<String> info;
    public:
      typedef vector<String>::const_iterator const_iterator;
      typedef const_iterator                 iterator;
      typedef vector<String>::size_type      size_type;
      RealReplacementList() : info(1) {}
      RealReplacementList(ParmString mis, size_type num) {
	info.reserve(num+1); info.push_back(mis);
      }
	
      RealReplacementList(ParmString mis, ParmString cor) 
	: info(2) 
      {
	info[0] = mis; info[1] = cor;
      }
	
      const String & misspelled_word() const {return info[0];}
      const_iterator begin() const {return info.begin()+1;}
      const_iterator end()   const {return info.end();}
      size_type      size()  const {return info.size()-1;}
      bool add(ParmString );
      void add_nocheck(ParmString r) {info.push_back(r);}
      bool erase(ParmString);
      bool exists(ParmString);
    };
      
    class RealReplList : public vector<RealReplacementList> {};
    // ^ needed to reduce symbol length for some non-gnu assemblers
    typedef hash_map<SimpleString, RealReplList>  LookupTable;
      
  private:
    //   RealRepl.. is a custom struct where begin() and end() are
    //     iterators for the repl. list for a misspelled word
    //   The _elements_ of the hash_map are a vector of RealRepl.
    //      where the mispelled_word all have a soundslike eqilvent
    //      to _key_.
      
    LookupTable * lookup_table;
    PosibErr<void> save(FStream &, ParmString );
    PosibErr<void> merge(FStream &, ParmString , Config * config = 0);
    
  private:
    WritableReplS(const WritableReplS&);
    WritableReplS& operator=(const WritableReplS&);
  public:
    WritableReplS() 
      : WritableBase<WritableReplacementSet>(".prepl",".rpl")
    {lookup_table = new LookupTable();}
    ~WritableReplS() {delete lookup_table;}

    struct ElementsVirEmulImpl;
    VirEmul * elements() const;
    Size      size()     const;
    bool      empty()    const;
      
    PosibErr<void> clear();
      
    PosibErr<void> add(ParmString mis, ParmString cor);
    PosibErr<void> add(ParmString mis, ParmString cor, ParmString s);
      
    struct ReplsWSoundslikeParms;
    VirEmul * repls_w_soundslike(const char * soundslike) const;
    VirEmul * repls_w_soundslike(SoundslikeWord soundslike) const;
      
    struct SoundslikeElementsParms;
    VirSoundslikeEmul * soundslike_elements() const;
  };
    
  //
  // FIXME:  Fix file nameing issue
  // so that aspell create repl < tmp will WORK!!!!
  // change class file_name to cur_file_name
  // and same with cur_file_date
  //
    
  bool WritableReplS::RealReplacementList::exists(ParmString word) {
    iterator i = begin();
    iterator e = end();
    while (i != e) {
      if (*i == word) return true;
      ++i;
    }
    return false;
  }
    
  bool WritableReplS::RealReplacementList::add(ParmString word) {
    if (exists(word)) return false;
    info.push_back(word);
    return true;
  }
    
  bool WritableReplS::RealReplacementList::erase(ParmString word) {
    vector<String>::iterator i = info.begin() + 1;
    vector<String>::iterator e = info.end();
    while (i != e) {
      if (*i == word) {
	info.erase(i);
	return true;
      }
      ++i;
    }
    return false;
  }
    
  class WritableReplS::ElementsVirEmulImpl : public VirEmulation<ReplacementList> {
  private:
    typedef LookupTable::const_iterator  OuterItr;
    typedef RealReplList::const_iterator InnerItr;
    OuterItr outer_;
    OuterItr end_;
    InnerItr inner_;
  public:
    // this assums LookupTable is non empty
    ElementsVirEmulImpl (const LookupTable & c)
      : outer_(c.begin()), end_(c.end()) 
    {if (outer_ != end_) inner_ = outer_->second.begin();}
	
    ElementsVirEmulImpl * clone() const {
      return new ElementsVirEmulImpl(*this);
    }
      
    void assign(const VirEmulation<Value> * other) {
      *this = *static_cast<const ElementsVirEmulImpl *>(other);
    }
      
    Value next() {
      if (outer_ == end_) return ReplacementList();
      if (inner_ == outer_->second.end()) {
	++outer_;
	if (outer_ == end_) return ReplacementList();
	inner_ = outer_->second.begin();
      }
      ReplacementList temp
	(inner_->misspelled_word().c_str(), 
	 new MakeVirEmulation<StrParms<RealReplacementList::const_iterator> >
	 (inner_->begin(), inner_->end()));
      ++inner_;
      return temp;
    }
    
    bool at_end() const {return outer_ == end_;}
      
  };
    
  WritableReplS::VirEmul * WritableReplS::elements() const {
    return new ElementsVirEmulImpl(*lookup_table);
  }
    
  //FIXME: Don't always return a size of 0!!!!
  WritableReplS::Size WritableReplS::size() const {return 0;}
      
  bool WritableReplS::empty() const {
    return lookup_table->empty();
  }

  PosibErr<void> WritableReplS::add(ParmString mis, ParmString cor) {
    return add(mis, cor, lang()->to_soundslike(mis));
  }

  PosibErr<void> WritableReplS::add(ParmString mis, ParmString cor, ParmString s) {

    LookupTable::iterator i = lookup_table->find(SimpleString(s.str(),1));
    if (i == lookup_table->end())
      i = lookup_table->insert
	(LookupTable::value_type(s.str(), RealReplList())).first;
  
    RealReplList::iterator    j = i->second.begin();
    RealReplList::iterator    e = i->second.end();
    for (; j != e; ++j) {
      if ((*j).misspelled_word() == mis) {
	(*j).add(cor);
	return no_err;
      }
    }
    i->second.push_back(RealReplacementList(mis,cor));
    return no_err;
  }

  struct WritableReplS::ReplsWSoundslikeParms {
    typedef ReplacementList                             Value;
    typedef vector<RealReplacementList>::const_iterator Iterator;
    Iterator end_;
    ReplsWSoundslikeParms(Iterator e) : end_(e) {}
    bool endf(Iterator i) const {return i == end_;}
    Value end_state() const {return ReplacementList();}
    Value deref(Iterator i) const {
      return ReplacementList
	(i->misspelled_word().c_str(), 
	 new MakeVirEmulation<StrParms<RealReplacementList::const_iterator> >
	 (i->begin(), i->end()));
    }
  };
    
  WritableReplS::VirEmul *
  WritableReplS::repls_w_soundslike(const char * soundslike) const {
      
    LookupTable::const_iterator i = 
      lookup_table->find(SimpleString(soundslike,1));
      
    if (i == lookup_table->end()) {
      return new MakeAlwaysEndEmulation<ReplacementList>();
    } else {
      return new MakeVirEmulation<ReplsWSoundslikeParms>
	(i->second.begin(), ReplsWSoundslikeParms(i->second.end()));
    }
  }
  

  WritableReplS::VirEmul *
  WritableReplS::repls_w_soundslike(SoundslikeWord soundslike) const {
    
    const RealReplList * p = 
      reinterpret_cast<const RealReplList *>(soundslike.word_list_pointer);

    return new MakeVirEmulation<ReplsWSoundslikeParms>(p->begin(), p->end());

  }

  struct WritableReplS::SoundslikeElementsParms {
    typedef SoundslikeWord                   Value;
    typedef LookupTable::const_iterator      Iterator;
    Iterator end_;
    SoundslikeElementsParms(Iterator e) : end_(e) {}
    bool endf(Iterator i) const {return i==end_;}
    static Value deref(Iterator i) {
      return Value(i->first.c_str(),
		   reinterpret_cast<const void *>(&i->second));
    }
    static Value end_state() {return Value(0,0);}
  };

  WritableReplS::VirSoundslikeEmul * 
  WritableReplS::soundslike_elements() const {
    return new MakeVirEmulation<SoundslikeElementsParms>
      (lookup_table->begin(),SoundslikeElementsParms(lookup_table->end()));
  }

  PosibErr<void> WritableReplS::save (FStream & out, ParmString file_name) 
  {
    out << "personal_repl-1.1" << ' ' << lang_name() <<  " 0 \n";
  
    LookupTable::iterator i = lookup_table->begin();
    LookupTable::iterator e = lookup_table->end();
  
    for (;i != e; ++i) {
      for (RealReplList::iterator j = i->second.begin(); 
	   j != i->second.end(); 
	   ++j) 
	{
	  for (RealReplacementList::iterator k = j->begin(); 
	       k != j->end(); 
	       ++k) 
	    {
	      out << (*j).misspelled_word() << ' ' << *k << '\n';
	    }
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
	add(mis, repl);
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
	    add(mis, repl);
	  }
	}
      }

    }
    return no_err;
  }

  PosibErr<void> WritableReplS::clear() {
    delete lookup_table;
    lookup_table = new LookupTable();
    return no_err;
  }
}

namespace aspell {
  WritableReplacementSet * new_default_writable_replacement_set() {
    return new aspell_default_writable_repl::WritableReplS();
  }
}

