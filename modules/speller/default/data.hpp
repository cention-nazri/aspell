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

  struct LocalDict;
  struct LocalDictInfo;
  class LocalDictList;
  typedef Enumeration<WordEntry *> WordEntryEnumeration;
  typedef Enumeration<const LocalDict *> DictsEnumeration;

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

  class Dictionary : public Cacheable, public WordList {
    friend class SpellerImpl;
  private:
    CachePtr<const Language> lang_;
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
    const BasicType basic_type;
    const char * const class_name;

  protected:
    Dictionary(BasicType,const char *);
  public:
    virtual ~Dictionary();

    const Id & id() {return *id_;}
    PosibErr<void> check_lang(ParmString lang);
    PosibErr<void> set_check_lang(ParmString lang, const Config *);
    const LangImpl * lang() const {return lang_;};
    const Language * language() const {return lang_;};
    const char * lang_name() const;

  private:
    FileName file_name_;
  protected:
    PosibErr<void> set_file_name(ParmString name);
    PosibErr<void> update_file_info(FStream & f);
  public:
    bool compare(const Dictionary &);

    const char * file_name() const {return file_name_.path.c_str();}
    // returns any additional dictionaries that are also used
    virtual PosibErr<void> load(ParmString, const Config &, LocalDictList * = 0, 
                                SpellerImpl * = 0, const LocalDictInfo * = 0);

    virtual PosibErr<void> merge(ParmString);
    virtual PosibErr<void> synchronize();
    virtual PosibErr<void> save_noupdate();
    virtual PosibErr<void> save_as(ParmString);
    virtual PosibErr<void> clear();

    bool affix_compressed;
    bool have_soundslike; // only true when there is true phonet data
    bool fast_scan;  // can effectly scan for all soundslikes (or
                     // clean words if have_soundslike is false)
                     // with an edit distance of 1 or 2
    bool fast_lookup; // can effectly find all words with a given soundslike
                      // when the SoundslikeWord is not given
    
    typedef WordEntryEnumeration        Enum;
    typedef const char *                Value;
    typedef size_t                      Size;

    StringEnumeration * elements() const;

    virtual Enum * detailed_elements() const;
    virtual Size   size()     const;
    virtual bool   empty()    const;
  
    virtual bool lookup (ParmString word, WordEntry &,
                         const SensitiveCompare &) const;
    
    virtual bool clean_lookup(ParmString, WordEntry &) const;

    virtual bool soundslike_lookup(const WordEntry &, WordEntry &) const;
    virtual bool soundslike_lookup(ParmString, WordEntry & o) const;

    // the elements returned are only guaranteed to remain valid
    // guaranteed to return all soundslike and all words 
    // however an individual soundslike may appear multiple
    // times in the list....
    virtual SoundslikeEnumeration * soundslike_elements() const;

    virtual PosibErr<void> add(ParmString w, ParmString s);
    PosibErr<void> add(ParmString w);

    virtual PosibErr<void> remove(ParmString w);

    virtual bool repl_lookup(const WordEntry &, WordEntry &) const;
    virtual bool repl_lookup(ParmString, WordEntry &) const;

    virtual PosibErr<void> add_repl(ParmString mis, ParmString cor, ParmString s);
    PosibErr<void> add_repl(ParmString mis, ParmString cor);

    virtual PosibErr<void> remove_repl(ParmString mis, ParmString cor);

    virtual DictsEnumeration * dictionaries() const;
  };

  typedef Dictionary Dict;
  typedef Dictionary ReplacementDict;
  typedef Dictionary MultiDict;

  bool operator==(const Dictionary::Id & rhs, const Dictionary::Id & lhs);

  inline bool operator!=(const  Dictionary::Id & rhs, const Dictionary::Id & lhs)
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
  Dictionary * new_default_readonly_dict();
  
  PosibErr<void> create_default_readonly_dict(StringEnumeration * els,
                                              Config & config);
  
  // implemented in multi_ws.cc
  MultiDict * new_default_multi_dict();

  // implemented in writable.cpp
  Dictionary * new_default_writable_dict();

  // implemented in writable.cpp
  ReplacementDict * new_default_replacement_dict();
}

#endif

