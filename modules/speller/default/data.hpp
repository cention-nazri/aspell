// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#ifndef ASPELLER_DATA__HPP
#define ASPELLER_DATA__HPP

#include <assert.h>

#include "copy_ptr.hpp"
#include "enumeration.hpp"
#include "language.hpp"
#include "posib_err.hpp"
#include "string.hpp"
#include "string_enumeration.hpp"
#include "word_list.hpp"

using namespace acommon;

namespace acommon {
  class Config;
  class FStream;
  class OStream;
}

namespace aspeller {

  class SpellerImpl;
  class Language;

  class SensitiveCompare;

  class DataSet {
    friend class SpellerImpl;
  private:
    CopyPtr<Language>       lang_;
    int                     attach_count_;
  private:
    PosibErr<void> attach(const Language &);
    void detach();
  public:
    class FileName {
      void copy(const FileName & other);
    public:
      String       path;
      const char * name;
      
      void clear();
      void set(ParmString);
      
      FileName() {clear();}
      explicit FileName(ParmString str) {set(str);}
      FileName(const FileName & other) {copy(other);}
      FileName & operator=(const FileName & other) {copy(other); return *this;}
    };
    class Id;
  protected:
    CopyPtr<Id> id_;
    virtual void set_lang_hook(Config *) {}
    
  public:
    //this is here because dynamic_cast in gcc 2.95.1 took too dam long
    enum BasicType {no_type, basic_word_set, basic_replacement_set, basic_multi_set};
    BasicType basic_type;

    DataSet();
    virtual ~DataSet();
    const Id & id() {return *id_;}
    PosibErr<void> check_lang(ParmString lang);
    PosibErr<void> set_check_lang(ParmString lang, Config *);
    const Language * lang() const {return lang_;};
    const char * lang_name() const;
    bool is_attached () const ;
  };

  bool operator==(const DataSet::Id & rhs, const DataSet::Id & lhs);

  inline bool operator!=(const DataSet::Id & rhs, const DataSet::Id & lhs)
  {
    return !(rhs == lhs);
  }

  struct LocalWordSetInfo;

  class LoadableDataSet : public DataSet {
  private:
    FileName file_name_;
  protected:
    PosibErr<void> set_file_name(ParmString name);
    PosibErr<void> update_file_info(FStream & f);
  public:
    bool compare(const LoadableDataSet &);
    const char * file_name() const {return file_name_.path.c_str();}
    virtual PosibErr<void> load(ParmString, Config *, SpellerImpl * = 0, const LocalWordSetInfo * li = 0) = 0;
  };

  class WritableDataSet {
  public:
    virtual PosibErr<void> merge(ParmString) = 0;
    virtual PosibErr<void> synchronize() = 0;
    virtual PosibErr<void> save_noupdate() = 0;
    virtual PosibErr<void> save_as(ParmString) = 0;
    virtual PosibErr<void> clear() = 0;
  };

  struct CompoundInfo {
    unsigned char d;
    
    CompoundInfo(unsigned char d0 = 0) : d(d0) {}

    unsigned int mid_char()       const {return d & (1<<0|1<<1);}

    void mid_char(unsigned int c) {
      assert(c < 4);
      d |= c;
    }

    bool mid_required ()       const {return d & 1<<2; }
    void mid_required (bool c)       { d |= c<<2;}
    
    bool beg () const {return d & 1<<3;}
    void beg (bool c) {d |= c<<3;}

    bool mid () const {return d & 1<<4;}
    void mid (bool c) {d |= c<<4;}

    bool end () const {return d & 1<<5;}
    void end (bool c) {d |= c<<5;}

    bool any() const {return d & (1<<3|1<<4|1<<5);}

    const char * read(const char * str, const Language & l);
    OStream & write(OStream &, const Language & l) const;

    enum Position {Orig, Beg, Mid, End};

    bool compatible(Position pos);
  };

  CompoundInfo::Position 
  new_position(CompoundInfo::Position unsplit_word, 
	       CompoundInfo::Position pos);
  
  struct SoundslikeWord {
    const char * soundslike;
    const void * word_list_pointer;

    operator bool () const {return soundslike;}
  
    SoundslikeWord() : soundslike(0) {}
    SoundslikeWord(const char * w, const void * p) 
      : soundslike(w == 0 || w[0] == 0 ? 0 : w), word_list_pointer(p) {}
  };

  static const unsigned int MaxCompoundLength = 8;

  struct BasicWordInfo {
    const char * word;
    CompoundInfo compound;
    BasicWordInfo(const char * w = 0, CompoundInfo c = 0)
      : word(w), compound(c) {}
    //operator const char * () const {return word;}
    operator bool () const {return word != 0;}
    void get_word(String & w, const ConvertWord &c) {
      w = "";
      c.convert(word, w);
    }
     
    OStream & write(OStream & o, const Language & l,
		    const ConvertWord &) const;
  };

  struct SingleWordInfo {
    const char * word;
    char middle_char;
    SingleWordInfo(const char * w = 0, char mc = '\0')
      : word(w), middle_char(mc) {}
    void clear() {word = 0;}
    void set(const char * w, char mc = '\0') {word = w; middle_char = mc;}
    void append_word(String & word, const Language & l, 
		     const ConvertWord &) const;
    operator bool() const {return word != 0;}
  };

  struct WordInfo {
    SingleWordInfo words[MaxCompoundLength + 1];
    void get_word(String & word, const Language & l, 
		  const ConvertWord &) const;
    operator bool () const {return words[0].word != 0;}
    OStream & write(OStream & o, const Language & l, 
		    const ConvertWord &) const;
  };

  struct LocalWordSetInfo 
  {
    SensitiveCompare compare;
    ConvertWord      convert;
    void set_language(const Language * l);
    void set(const Language * l, const Config * c, bool strip = false);
  };

  class SoundslikeEnumeration 
  {
  public:
    virtual SoundslikeWord next(int) = 0;
    virtual ~SoundslikeEnumeration() {}
  };

  class BasicWordSet : public LoadableDataSet, public WordList
  {
  public:
    BasicWordSet() {
      basic_type =  basic_word_set;
    }
    
    typedef VirEnumeration<BasicWordInfo>   VirEmul;
    typedef Enumeration<VirEmul>            Emul;
    typedef const char *                  Value;
    typedef unsigned int                  Size;
    typedef SoundslikeWord                SoundslikeValue;
    typedef SoundslikeEnumeration         VirSoundslikeEmul;
    StringEnumeration * elements() const;
    virtual VirEmul * detailed_elements() const = 0;
    virtual Size   size()     const = 0;
    virtual bool   empty()    const {return !size();}
  
    virtual BasicWordInfo lookup (ParmString word, 
				  const SensitiveCompare &) const = 0;
    
    // guaranteed to return all words with the soundslike 
    virtual VirEmul * words_w_soundslike(const char * sondslike) const = 0;

    // the elements returned are only guaranteed to remain valid
    // guaranteed to return all soundslike and all words 
    // however an individual soundslike may appear multiple
    // times in the list....
    virtual VirSoundslikeEmul * soundslike_elements() const = 0;

    // NOT garanteed to return all words with the soundslike
    virtual VirEmul * words_w_soundslike(SoundslikeWord soundslike) const = 0;

  };

  class WritableWordSet : public BasicWordSet,
			  public WritableDataSet
  {
  public:
    virtual PosibErr<void> add(ParmString w) = 0;
    virtual PosibErr<void> add(ParmString w, ParmString s) = 0;
  };

  struct ReplacementList {
    typedef VirEnumeration<const char *> VirEmul;
    typedef Enumeration<VirEmul>         Emul;
    typedef const char *               Value;

    const char *  misspelled_word;
    VirEmul    *  elements; // you are responable for freeing this with delete
    bool empty() const {return elements == 0;}

    ReplacementList()
      : elements(0) {}
    ReplacementList(const char * w, VirEmul * els)
      : misspelled_word(w), elements(els) {}
  };

  class BasicReplacementSet : public LoadableDataSet
  {
  public:
    BasicReplacementSet() {
      basic_type = basic_replacement_set;
    }
    
    typedef VirEnumeration<ReplacementList> VirEmul;
    typedef Enumeration<VirEmul>            Emul;
    typedef const char *                  Value;
    typedef unsigned int                  Size;
    typedef SoundslikeWord                SoundslikeValue;
    typedef SoundslikeEnumeration  VirSoundslikeEmul;

    virtual VirEmul * elements() const = 0;
    virtual Size   size()     const = 0;
    virtual bool   empty()    const {return !size();}

    virtual VirEmul * repls_w_soundslike(const char * soundslike) const = 0;
    virtual VirEmul * repls_w_soundslike(SoundslikeWord soundslike) const = 0;
    
    virtual VirSoundslikeEmul * soundslike_elements() const = 0;
  };


  class WritableReplacementSet : public BasicReplacementSet,
				 public WritableDataSet
  {
  public:
    virtual PosibErr<void> add(ParmString mis, ParmString cor) = 0;
    virtual PosibErr<void> add(ParmString mis, ParmString cor, ParmString s) = 0;
  };

  struct LocalWordSet {
    // NOTE: perhaps LoadableDataSet is too specific
    LoadableDataSet  * word_set;
    LocalWordSetInfo local_info;
    LocalWordSet() : word_set(0) {}
    LocalWordSet(LoadableDataSet * ws, LocalWordSetInfo li) 
      : word_set(ws), local_info(li) {}
    operator bool () const {return word_set != 0;}
  };
  
  class BasicMultiSet : public LoadableDataSet, public WordList
  {
  public:
    BasicMultiSet() {
      basic_type = basic_multi_set;
    }
    
    typedef LocalWordSet         Value;
    typedef VirEnumeration<Value>  VirEmul;
    typedef Enumeration<VirEmul>   Emul;
    typedef unsigned int         Size;

    virtual bool   empty()    const {return !size();}
    virtual Size   size()     const = 0;
    virtual StringEnumeration * elements() const {abort();} //FIXME

    virtual VirEmul * detailed_elements() const = 0;
  };


  typedef unsigned int DataType;
  static const DataType DT_ReadOnly     = 1<<0;
  static const DataType DT_Writable     = 1<<1;
  static const DataType DT_WritableRepl = 1<<2;
  static const DataType DT_Multi        = 1<<3;
  static const DataType DT_Any          = 0xFF;

  PosibErr<LoadableDataSet *> add_data_set(ParmString file_name,
					   Config &,
					   SpellerImpl * = 0,
					   const LocalWordSetInfo * = 0,
					   ParmString dir = 0,
					   DataType allowed = DT_Any);
  
  // implemented in readonly_ws.cc
  BasicWordSet * new_default_readonly_word_set();
  PosibErr<void> create_default_readonly_word_set(StringEnumeration * els,
                                                  Config & config);

  // implemented in multi_ws.cc
  BasicMultiSet * new_default_multi_word_set();

  // implemented in writable_ws.cc
  WritableWordSet * new_default_writable_word_set();

  // implemented in writable_repl.cc
  WritableReplacementSet * new_default_writable_replacement_set();

  
}

#endif

