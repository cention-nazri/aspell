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
  class Convert;
}

namespace aspeller {

  typedef Enumeration<WordEntry *> WordEntryEnumeration;

  class Dict : public Cacheable {
    friend class SpellerImpl;
  private:
    CachePtr<const Language> lang_;
  private:
    PosibErr<void> attach(const Language &);
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
    virtual void set_lang_hook(const Config *) {}
    
  public:
    typedef Id CacheKey;
    bool cache_key_eq(const Id &);

    enum BasicType {no_type, basic_dict, replacement_dict, multi_dict};
    BasicType basic_type;

    Dict();
    virtual ~Dict();
    const Id & id() {return *id_;}
    PosibErr<void> check_lang(ParmString lang);
    PosibErr<void> set_check_lang(ParmString lang, const Config *);
    const Language * lang() const {return lang_;};
    const char * lang_name() const;
  };

  bool operator==(const Dict::Id & rhs, const Dict::Id & lhs);

  inline bool operator!=(const Dict::Id & rhs, const Dict::Id & lhs)
  {
    return !(rhs == lhs);
  }

  struct LocalDictInfo 
  {
    SensitiveCompare compare;
    ConvertWord      convert;
    void set_language(const Language * l);
    void set(const Language * l, const Config & c, bool strip = false);
    void set(const LocalDictInfo & li) {operator=(li);}
  };

  struct LocalDict : public LocalDictInfo {
    Dict * dict;
    LocalDict(Dict * d = 0) : dict(d) {}
    LocalDict(Dict * d, LocalDictInfo li)
      : LocalDictInfo(li), dict(d) {}
    operator bool () const {return dict != 0;}
  };

  class LocalDictList {
    // well a stack at the moment but it may eventually become a list
    // NOT necessarily first in first out
    Vector<LocalDict> data;
  private:
    LocalDictList(const LocalDictList &);
    void operator= (const LocalDictList &);
  public:
    // WILL take ownership of the dict
    LocalDictList() {}
    void add(const LocalDict & o) {data.push_back(o);}
    LocalDict & last() {return data.back();}
    void pop() {data.pop_back();}
    bool empty() {return data.empty();}
    ~LocalDictList() {for (; !empty(); pop()) last().dict->release();}
  };
    
  class LoadableDict : public Dict {
  private:
    FileName file_name_;
  protected:
    PosibErr<void> set_file_name(ParmString name);
    PosibErr<void> update_file_info(FStream & f);
  public:
    bool compare(const LoadableDict &);
    const char * file_name() const {return file_name_.path.c_str();}
    // returns any additional dictionaries that are also used
    virtual PosibErr<void> load(ParmString, const Config &, LocalDictList * = 0, 
                                SpellerImpl * = 0, const LocalDictInfo * = 0) = 0;
  };

  class WritableDict {
  public:
    WritableDict() {}
    virtual PosibErr<void> merge(ParmString) = 0;
    virtual PosibErr<void> synchronize() = 0;
    virtual PosibErr<void> save_noupdate() = 0;
    virtual PosibErr<void> save_as(ParmString) = 0;
    virtual PosibErr<void> clear() = 0;
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

  class BasicDict : public LoadableDict, public WordList
  {
  public:
    bool affix_compressed;
    bool have_soundslike; // only true when there is true phonet data
    bool fast_scan;  // can effectly scan for all soundslikes (or
                     // stripped words if have_soundslike is false)
                     // with an edit distance of 1 or 2
    bool fast_lookup; // can effectly find all words with a given soundslike
                      // when the SoundslikeWord is not given
    
    BasicDict() : affix_compressed(false), have_soundslike(false), 
                     fast_scan(false), fast_lookup(false) {
      basic_type =  basic_dict;
    }
    
    typedef WordEntryEnumeration        Enum;
    typedef const char *                Value;
    typedef unsigned int                Size;

    StringEnumeration * elements() const;

    virtual Enum * detailed_elements() const = 0;
    virtual Size   size()     const = 0;
    virtual bool   empty()    const {return !size();}
  
    virtual bool lookup (ParmString word, WordEntry &,
                         const SensitiveCompare &) const = 0;
    
    virtual bool stripped_lookup(const char * sondslike, WordEntry &) const {return false;}

    // garanteed to be constant time
    // FIXME: are both functions needed since a WordEntry can easily be created from
    //   just a word?
    virtual bool soundslike_lookup(const WordEntry &, WordEntry &) const = 0;
    virtual bool soundslike_lookup(const char * sondslike, WordEntry &) const = 0;

    // the elements returned are only guaranteed to remain valid
    // guaranteed to return all soundslike and all words 
    // however an individual soundslike may appear multiple
    // times in the list....
    virtual SoundslikeEnumeration * soundslike_elements() const = 0;
  };

  class WritableBasicDict : public BasicDict,
                            public WritableDict
  {
  public:
    virtual PosibErr<void> add(ParmString w) = 0;
    virtual PosibErr<void> add(ParmString w, ParmString s) = 0;
  };

  class ReplacementDict : public BasicDict
  {
  public:
    ReplacementDict() {
      basic_type = replacement_dict;
    }

    // FIXME: are both functions needed since a WordEntry can easily be created from
    //   just a word?
    virtual bool repl_lookup(const WordEntry &, WordEntry &) const = 0;
    virtual bool repl_lookup(const char * word, WordEntry &) const = 0;
  };


  class WritableReplacementDict : public ReplacementDict,
                                  public WritableDict
  {
  public:
    virtual PosibErr<void> add(ParmString mis, ParmString cor) = 0;
    virtual PosibErr<void> add(ParmString mis, ParmString cor, ParmString s) = 0;
  };

  class MultiDict : public LoadableDict, public WordList
  {
  public:
    MultiDict() {
      basic_type = multi_dict;
    }
    
    typedef LocalDict            Value;
    typedef Enumeration<Value>   Enum;
    typedef unsigned int         Size;

    virtual bool   empty()    const {return !size();}
    virtual Size   size()     const = 0;
    virtual StringEnumeration * elements() const {abort(); return 0; } //FIXME

    virtual Enum * detailed_elements() const = 0;
  };

  typedef unsigned int DataType;
  static const DataType DT_ReadOnly     = 1<<0;
  static const DataType DT_Writable     = 1<<1;
  static const DataType DT_WritableRepl = 1<<2;
  static const DataType DT_Multi        = 1<<3;
  static const DataType DT_Any          = 0xFF;

  // stores result in LocalDict
  // any new extra dictionaries that were loaded will be ii
  PosibErr<void> add_data_set(ParmString file_name,
                              const Config &,
                              LocalDict &,
                              LocalDictList * other_dicts = 0,
                              SpellerImpl * = 0,
                              const LocalDictInfo * local_info = 0,
                              ParmString dir = 0,
                              DataType allowed = DT_Any);
  
  // implemented in readonly_ws.cc
  BasicDict * new_default_readonly_basic_dict();
  
  PosibErr<void> create_default_readonly_basic_dict(StringEnumeration * els,
                                                    Config & config);
  
  // implemented in multi_ws.cc
  MultiDict * new_default_multi_dict();

  // implemented in writable.cpp
  WritableBasicDict * new_default_writable_basic_dict();

  // implemented in writable.cpp
  WritableReplacementDict * new_default_writable_replacement_dict();

  
}

#endif

