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
#include "cache.hpp"
#include "wordinfo.hpp"

using namespace acommon;

namespace acommon {
  class Config;
  class FStream;
  class OStream;
}

namespace aspeller {

  class DataSet {
    friend class SpellerImpl;
  private:
    CachePtr<const Language> lang_;
    int                      attach_count_;
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
    virtual WordEntry * next(int) = 0;
    virtual ~SoundslikeEnumeration() {}
    SoundslikeEnumeration() {}
  private:
    SoundslikeEnumeration(const SoundslikeEnumeration &);
    void operator=(const SoundslikeEnumeration &);
  };

  class BasicWordSet : public LoadableDataSet, public WordList
  {
  public:
    bool affix_compressed;
    bool have_soundslike; // only true when there is true phonet data
    bool fast_scan;  // can effectly scan for all soundslikes (or
                     // stripped words if have_soundslike is false)
                     // with an edit distance of 1 or 2
    bool fast_lookup; // can effectly find all words with a given soundslike
                      // when the SoundslikeWord is not given
    
    BasicWordSet() : affix_compressed(false), have_soundslike(false), 
                     fast_scan(false), fast_lookup(false) {
      basic_type =  basic_word_set;
    }
    
    typedef VirEnumeration<WordEntry *> VirEnum;
    typedef Enumeration<VirEnum>        Enum;
    typedef const char *                Value;
    typedef unsigned int                Size;

    StringEnumeration * elements() const;

    virtual VirEnum * detailed_elements() const = 0;
    virtual Size   size()     const = 0;
    virtual bool   empty()    const {return !size();}
  
    virtual bool lookup (ParmString word, WordEntry &,
                         const SensitiveCompare &) const = 0;

    // garanteed to be constant time
    virtual bool soundslike_lookup(const WordEntry &, WordEntry &) const = 0;
    virtual bool soundslike_lookup(const char * sondslike, WordEntry &) const = 0;

    // the elements returned are only guaranteed to remain valid
    // guaranteed to return all soundslike and all words 
    // however an individual soundslike may appear multiple
    // times in the list....
    virtual SoundslikeEnumeration * soundslike_elements() const = 0;
  };

  class WritableWordSet : public BasicWordSet,
			  public WritableDataSet
  {
  public:
    virtual PosibErr<void> add(ParmString w) = 0;
    virtual PosibErr<void> add(ParmString w, ParmString s) = 0;
  };

  class BasicReplacementSet : public BasicWordSet
  {
  public:
    BasicReplacementSet() {
      basic_type = basic_replacement_set;
    }

    virtual bool repl_lookup(const WordEntry &, WordEntry &) const = 0;
    virtual bool repl_lookup(const char * word, WordEntry &) const = 0;
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

