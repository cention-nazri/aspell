// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_CONFIG___HPP
#define ASPELL_CONFIG___HPP

#include "can_have_error.hpp"
#include "key_info.hpp"
#include "posib_err.hpp"
#include "string.hpp"

namespace acommon {

  class OStream;
  class KeyInfoEnumeration;
  class StringMap;
  class StringPairEnumeration;
  class MutableContainer;

  // The Config class is used to hold configuration information.
  // it has a set of keys which it will except.  Inserting or even
  // trying to look at a key that it does not know will produce
  // an error.  Extra accepted keys can be added with the set_extra 
  // method.

  // An r in the otherdata[0] value means that when merged into
  // a config of a different type it will be renamed to
  // <config name>_<name>
  // A p in the other datavalue means that is is a placeholder
  // for when a "r" is merged.  It should start with <config name>

  struct ConfigModule {
    const char * name;
    const KeyInfo * begin;
    const KeyInfo * end;
  };

  class Notifier {
  public:
    virtual PosibErr<void> item_updated(const KeyInfo *, bool)         {return no_err;}
    virtual PosibErr<void> item_updated(const KeyInfo *, int)          {return no_err;}
    virtual PosibErr<void> item_updated(const KeyInfo *, ParmString) {return no_err;}
    virtual PosibErr<void> item_added  (const KeyInfo *, ParmString) {return no_err;}
    virtual PosibErr<void> item_removed(const KeyInfo *, ParmString) {return no_err;}
    virtual PosibErr<void> all_removed (const KeyInfo *, ParmString) {return no_err;}
    // the second paramater for all_removed should not be used
  };

  class PossibleElementsEmul;
  class NotifierEnumeration;
  class GetLine;
  class MDInfoListofLists;

  struct ConfigKeyModuleInfo {
    const KeyInfo       * main_begin;
    const KeyInfo       * main_end;
    const KeyInfo       * extra_begin;
    const KeyInfo       * extra_end;
    const ConfigModule  * modules_begin;
    const ConfigModule  * modules_end;
  };

  class Config : public CanHaveError {
    // copy and destructor provided
    friend class MDInfoListofLists;
  private:
    String      name_;
    StringMap * data_;

    bool attached_;    // if attached can't copy
    Notifier * * notifier_list;

    friend class PossibleElementsEmul;

    ConfigKeyModuleInfo kmi;

    int md_info_list_index;

  public:
    
    String temp_str;

    Config(ParmString name,
	   const KeyInfo  * mainbegin, 
	   const KeyInfo * mainend);

    Config(const Config &);
    ~Config();
    Config & operator= (const Config &);

    bool get_attached() const {return attached_;}
    void set_attached(bool a) {attached_ = a;}

    Config * clone() const;
    void assign(const Config * other);

    const char * name() const {return name_.c_str();}

    NotifierEnumeration * notifiers() const;
  
    bool add_notifier    (      Notifier *);
    bool remove_notifier (const Notifier *);
    bool replace_notifier(const Notifier *, Notifier *);

    void set_extra(const KeyInfo * begin, const KeyInfo * end);

    void set_modules(const ConfigModule * modbegin, const ConfigModule * modend);

    static const char * base_name(ParmString name);
  
    PosibErr<const KeyInfo *> keyinfo(ParmString key) const;

    KeyInfoEnumeration * possible_elements(bool include_extra = true);

    StringPairEnumeration * elements();
    
    PosibErr<String> get_default (ParmString key) const;

    PosibErr<String> retrieve    (ParmString key) const;
  
    bool have (ParmString key) const;

    PosibErr<void> retrieve_list (ParmString key, MutableContainer *) const;
    PosibErr<bool> retrieve_bool (ParmString key) const;
    PosibErr<int>  retrieve_int  (ParmString key) const;
    
    PosibErr<void> replace (ParmString, ParmString);
    PosibErr<bool> remove  (ParmString);
    
    void write_to_stream(OStream & out, bool include_extra = false);

    PosibErr<void> read_in_settings(const Config * override = 0);

    PosibErr<void> read_in(IStream & in);
    PosibErr<void> read_in_file(ParmString file);
    PosibErr<void> read_in_string(ParmString str);

    void merge(const Config &);
    //Note: if the same key is in both config's it is assumed that they
    // have the same data type.
  };

  Config * new_config();
  Config * new_basic_config(); // config which doesn't require any
			       // external symbols

  class NotifierEnumeration {
    // no copy and destructor needed
    Notifier * * i;
  public:
    NotifierEnumeration(Notifier * * b) : i(b) {}
    const Notifier * next() {
      Notifier * * temp = i;
      if (*i != 0)
	++i;
      return *temp;
    }
    bool at_end() const {return *i == 0;}
  };

  class KeyInfoEnumeration {
  public:
    typedef const KeyInfo * Value;
    virtual KeyInfoEnumeration * clone() const = 0;
    virtual void assign(const KeyInfoEnumeration *) = 0;
    virtual bool at_end() const = 0;
    virtual const KeyInfo * next() = 0;
    virtual ~KeyInfoEnumeration() {}
  };

}

#endif

