// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <locale.h>

#include "dirs.h"
#include "settings.h"

#include "asc_ctype.hpp"
#include "config.hpp"
#include "errors.hpp"
#include "file_util.hpp"
#include "fstream.hpp"
#include "getdata.hpp"
#include "itemize.hpp"
#include "mutable_container.hpp"
#include "posib_err.hpp"
#include "string_map.hpp"
#include "stack_ptr.hpp"
#include "char_vector.hpp"

//#include "iostream.hpp"

#define DEFAULT_LANG "en_US"

// FIXME: Consider eliminating KeyInfoDescript in favor of a better
//   solution for string a filters decryption

// NOTE: All filter options are now stored with he "filter-" prefix.  However
//   during lookup, the non prefix version is also recognized.

namespace acommon {

  static const ConfigModule a_module = ConfigModule();
  
  typedef Notifier * NotifierPtr;
  
  Config::Config(ParmString name,
		 const KeyInfo * mainbegin, 
		 const KeyInfo * mainend)
    : name_(name)
    , attached_(0)
    , md_info_list_index(-1)
  {
    kmi.main_begin = mainbegin;
    kmi.main_end   = mainend;
    kmi.extra_begin = 0;
    kmi.extra_end   = 0;
    kmi.filter_modules_begin = &a_module;
    kmi.filter_modules_end   = &a_module;
  }

  Config::~Config() {
    del_notifiers();
  }

  Config::Config(const Config & other) 
    : name_(other.name_), data_(other.data_), attached_(0), kmi(other.kmi),
      md_info_list_index(other.md_info_list_index)
  {
    copy_notifiers(other);
  }

  Config & Config::operator= (const Config & other)
  {
    attached_ = 0;
    kmi = other.kmi;
    data_ = other.data_;
    md_info_list_index = other.md_info_list_index;
    copy_notifiers(other);
    return *this;
  }

  Config * Config::clone() const {
    return new Config(*this);
  }

  void Config::assign(const Config * other) {
    *this = *(const Config *)(other);
  }

  void Config::set_filter_modules(const ConfigModule * modbegin, 
				  const ConfigModule * modend)
  {
    kmi.filter_modules_begin = modbegin;
    kmi.filter_modules_end   = modend;
  }

  void Config::set_extra(const KeyInfo * begin, 
			       const KeyInfo * end) 
  {
    kmi.extra_begin = begin;
    kmi.extra_end   = end;
  }

  //
  //
  //


  //
  // Notifier methods
  //

  NotifierEnumeration * Config::notifiers() const 
  {
    return new NotifierEnumeration(notifier_list);
  }

  void Config::copy_notifiers(const Config & other)
  {
    notifier_list.clear();

    Vector<Notifier *>::const_iterator i   = other.notifier_list.begin();
    Vector<Notifier *>::const_iterator end = other.notifier_list.end();

    for(; i != end; ++i) {
      Notifier * tmp = (*i)->clone(this);
      if (tmp != 0)
	notifier_list.push_back(tmp);
    }
  }

  void Config::del_notifiers()
  {
    Vector<Notifier *>::iterator i   = notifier_list.begin();
    Vector<Notifier *>::iterator end = notifier_list.end();

    for(; i != end; ++i) {
      delete (*i);
      *i = 0;
    }
    
    notifier_list.clear();
  }
  
  bool Config::add_notifier(Notifier * n) 
  {
    Vector<Notifier *>::iterator i   = notifier_list.begin();
    Vector<Notifier *>::iterator end = notifier_list.end();

    while (i != end && *i != n)
      ++i;

    if (i != end) {
    
      return false;
    
    } else {

      notifier_list.push_back(n);
      return true;

    }
  }

  bool Config::remove_notifier(const Notifier * n) 
  {
    Vector<Notifier *>::iterator i   = notifier_list.begin();
    Vector<Notifier *>::iterator end = notifier_list.end();

    while (i != end && *i != n)
      ++i;

    if (i == end) {
    
      return false;
    
    } else {

      delete *i;
      notifier_list.erase(i);
      return true;

    }
  }

  bool Config::replace_notifier(const Notifier * o, 
				      Notifier * n) 
  {
    Vector<Notifier *>::iterator i   = notifier_list.begin();
    Vector<Notifier *>::iterator end = notifier_list.end();

    while (i != end && *i != o)
      ++i;

    if (i == end) {
    
      return false;
    
    } else {

      delete *i;
      *i = n;
      return true;

    }
  }

  bool Config::have(ParmString key) const 
  {
    const char * value = data_.lookup(key);
    if (value == 0 || value[0] == '\x01') {
      return false;
    } else {
      return true;
    }
  }

  //
  // retrive methods
  //

  PosibErr<bool> Config::retrieve_bool(ParmString key) const
  {
    RET_ON_ERR_SET(retrieve(key), String, str);
    return str[0] == 't';
  }

  PosibErr<int> Config::retrieve_int(ParmString key) const
  {
    RET_ON_ERR_SET(retrieve(key), String, str);
    int i;
    sscanf(str.c_str(), "%i", &i);
    return i;
  }

  PosibErr<String> Config::retrieve(ParmString k) const
  {
    RET_ON_ERR_SET(expand_key(k), const char *, key);
    const char * value = data_.lookup(key);

    if (value != 0) {
      if (value[0] == '\x01') //FIXME: Why?
	++value;
      return String(value);
    } else {
      return get_default(key);
    }
  }

  PosibErr<void> Config::retrieve_list(ParmString k, 
				       MutableContainer * m) const
  {
    RET_ON_ERR_SET(expand_key(k), const char *, key);
    String def = get_default(key);

    const char * value = data_.lookup(key);
    String fkey = "filter-"; fkey += key;
    if (!value) value = data_.lookup(fkey);

    if (value != 0) {
      def += ',';
      def += data_.lookup(key);
    }

    RET_ON_ERR(itemize(def, *m));
    return no_err;
  }

  static const KeyInfo * find(ParmString key, 
			      const KeyInfo * i, 
			      const KeyInfo * end) 
  {
    while (i != end) {
      if (strcmp(key, i->name) == 0)
	return i;
      ++i;
    }
    return i;
  }

  static const ConfigModule * find(ParmString key, 
				   const ConfigModule * i, 
				   const ConfigModule * end) 
  {

    while (i != end) {
      if (strcmp(key, i->name) == 0){ 
	return i;
      }
      ++i;
    }
    return i;
  }

  const char * Config::base_name(ParmString name)
  {
    const char * c = strchr(name, '-');
    unsigned int p = c ? c - name : -1;
    if ((p == 3 && (strncmp(name, "add",p) == 0 
		    || strncmp(name, "rem",p) == 0))
	|| (p == 4 && strncmp(name, "dont",p) == 0)) 
      return name + p + 1;
    else
      return name;
  }

  PosibErr<const char *> Config::expand_key(ParmString k) const
  // prefix the key name with "filter-" if needed
  {
    PosibErr<const KeyInfo *> pe = keyinfo(k);
    if (!pe.has_err()) return pe.data->name;
    else               return (PosibErrBase &)pe;
  }

  PosibErr<const KeyInfo *> Config::keyinfo(ParmString key) const
  {
    typedef PosibErr<const KeyInfo *> Ret;
    {
      const KeyInfo * i;
      i = acommon::find(key, kmi.main_begin, kmi.main_end);
      if (i != kmi.main_end) return Ret(i);
      
      i = acommon::find(key, kmi.extra_begin, kmi.extra_end);
      if (i != kmi.extra_end) return Ret(i);
      
      const char * s = strncmp(key, "filter-", 7) == 0 ? key + 7 : key.str();
      const char * h = strchr(s, '-');
      if (h == 0) goto err;

      String k(s, h - s);
      const ConfigModule * j = acommon::find(k,
					     kmi.filter_modules_begin,
					     kmi.filter_modules_end);
      
      if (j == kmi.filter_modules_end) goto err;

      i = acommon::find(key, j->begin, j->end);
      if (i != j->end) return Ret(i);
      
      if (strncmp(key, "filter-", 7) != 0) k = "filter-";
      else                                 k = "";
      k += key;
      
      i = acommon::find(k, j->begin, j->end);
      if (i != j->end) return Ret(i);
    }
  err:  
    return Ret().prim_err(unknown_key, key);
  }

  static bool proc_locale_str(ParmString lang, String & final_str)
  {
    if (lang == 0) return false;
    const char * i = lang;
    if (!(asc_islower(i[0]) && asc_islower(i[1]))) return false;
    final_str.assign(i, 2);
    i += 2;
    if (! (i[0] == '_' || i[0] == '-')) return true;
    i += 1;
    if (!(asc_isupper(i[0]) && asc_isupper(i[1]))) return true;
    final_str += '_';
    final_str.append(i, 2);
    return true;
  }

  static void get_lang_env(String & str) 
  {
    if (proc_locale_str(getenv("LC_MESSAGES"), str)) return;
    if (proc_locale_str(getenv("LANG"), str)) return;
    if (proc_locale_str(getenv("LANGUAGE"), str)) return;
    str = DEFAULT_LANG;
  }

#ifdef USE_LOCALE

  static void get_lang(String & final_str) 
  {
    // FIXME: THIS IS NOT THREAD SAFE
    String locale = setlocale (LC_ALL, NULL);
    if (locale == "C")
      setlocale (LC_ALL, "");
    const char * lang = setlocale (LC_MESSAGES, NULL);
    bool res = proc_locale_str(lang, final_str);
    if (locale == "C")
      setlocale(LC_MESSAGES, locale.c_str());
    if (!res)
      get_lang_env(final_str);
  }

#else

  static inline void get_lang(String & str) 
  {
    get_lang_env(str);
  }

#endif

  PosibErr<String> Config::get_default(ParmString key) const
  {
    RET_ON_ERR_SET(keyinfo(key), const KeyInfo *, ki);

    bool   in_replace = false;
    String final_str;
    String replace;
    const char * i = ki->def;
    if (*i == '!') { // special cases
      ++i;
    
      if (strcmp(i, "lang") == 0) {

	if (have("master")) {
	  final_str = "<unknown>";
	} else {
	  get_lang(final_str);
	}
	
      } else if (strcmp(i, "actual-lang") == 0) {
	
	unsigned int len = 0;
	final_str = retrieve("lang");
	while (len < final_str.size() && final_str[len] != '_')
	  ++len;
	final_str.resize(len);

      } else if (strcmp(i, "special") == 0) {

	// do nothing

      } else {
      
	abort(); // this should not happen
      
      }
    
    } else for(; *i; ++i) {
    
      if (!in_replace) {

	if (*i == '<') {
	  in_replace = true;
	} else {
	  final_str += *i;
	}

      } else { // in_replace
      
	if (*i == '/' || *i == ':' || *i == '|' || *i == '#' || *i == '^') {
	  char sep = *i;
	  String second;
	  ++i;
	  while (*i != '\0' && *i != '>') second += *i++;
	  if (sep == '/') {
	    String s1 = retrieve(replace);
	    String s2 = retrieve(second);
	    final_str += add_possible_dir(s1, s2);
	  } else if (sep == ':') {
	    String s1 = retrieve(replace);
	    final_str += add_possible_dir(s1, second);
	  } else if (sep == '#') {
	    String s1 = retrieve(replace);
	    assert(second.size() == 1);
	    unsigned int s = 0;
	    while (s != s1.size() && s1[s] != second[0]) ++s;
	    final_str.append(s1, s);
	  } else if (sep == '^') {
	    String s1 = retrieve(replace);
	    String s2 = retrieve(second);
	    final_str += figure_out_dir(s1, s2);
	  } else { // sep == '|'
	    assert(replace[0] == '$');
	    const char * env = getenv(replace.c_str()+1);
	    final_str += env ? env : second;
	  }
	  replace = "";
	  in_replace = false;

	} else if (*i == '>') {

	  final_str += retrieve(replace).data;
	  replace = "";
	  in_replace = false;

	} else {

	  replace += *i;

	}

      }
      
    }
    return final_str;
  }


#define notify_all(ki, value, fun)                            \
  do {                                                        \
    Vector<Notifier *>::iterator   i = notifier_list.begin(); \
    Vector<Notifier *>::iterator end = notifier_list.end();   \
    while (i != end) {                                        \
      RET_ON_ERR((*i)->fun(ki,value));                        \
      ++i;                                                    \
    }                                                         \
  } while (false)

  class NotifyListBlockChange : public MutableContainer 
  {
    const KeyInfo * key_info;
    Vector<Notifier *> & notifier_list;
  public:
    NotifyListBlockChange(const KeyInfo * ki, Vector<Notifier *> & n);
    PosibErr<bool> add(ParmString);
    PosibErr<bool> remove(ParmString);
    PosibErr<void> clear();
  };

  NotifyListBlockChange::
  NotifyListBlockChange(const KeyInfo * ki, Vector<Notifier *> & n)
    : key_info(ki), notifier_list(n) {}

  PosibErr<bool> NotifyListBlockChange::add(ParmString v) {
    notify_all(key_info, v, item_added);
    return true;
  }

  PosibErr<bool> NotifyListBlockChange::remove(ParmString v) {
    notify_all(key_info, v, item_removed);
    return true;
  }

  PosibErr<void> NotifyListBlockChange::clear() {
    notify_all(key_info, 0, all_removed);
    return no_err;
  }

  PosibErr<void> Config::replace(ParmString k, ParmString value) {
    if (strcmp(value,"<default>") == 0)
      return remove(k);

    const char * key;
    const char * i = strchr(k, '-');
    int p = (i == 0 ? -1 : i - k);
    if (((p == 3) && 
         ((strncmp(k, "add",p) == 0) ||
          (strncmp(k, "rem",p) == 0))) ||
        ((p == 4) &&
         (strncmp(k, "dont",p) == 0))) 
    {
      key = k + p + 1;
      if (strncmp(key, "all-", 4) == 0) {
	key = key + 4;
	p = 7;
      }
    } else {
      key = k;
      p = 0;
    }

    RET_ON_ERR_SET(keyinfo(key), const KeyInfo *, ki);
    key = ki->name;

    if (attached_ && !(ki->flags & KEYINFO_MAY_CHANGE))
      return make_err(cant_change_value, key);
  
    assert(ki->def != 0); // if null this key should never have values
			  // directly added to it

    int num;
    switch (ki->type) {
    
    case KeyInfoBool: {
    
      if ((p == 4) || 
          ((p == 0) &&
           (strcmp(value,"false") == 0))) {

	data_.replace(key, "false");
	notify_all(ki, false, item_updated);
	return no_err;

      } else if (p != 0) {

	return make_err(unknown_key,  k);

      } else if ((value[0] == '\0') || (strcmp(value,"true") == 0)) {

	data_.replace(key, "true");
	notify_all(ki, true, item_updated);
	return no_err;

      } else {

	return make_err(bad_value, key, value,
                         "either \"true\" or \"false\"");

      }
      break;
    }  
    case KeyInfoString: {
      
      if (p == 0) {

	data_.replace(key,value);
	notify_all(ki, value, item_updated);
	return no_err;
      
      } else {
      
	return make_err(unknown_key,  key);
      
      }
      break;
    }  
    case KeyInfoInt: {

      if (p == 0 && sscanf(value, "%i", &num) == 1 && num >= 0) {

	data_.replace(key,value);
	notify_all(ki, num, item_updated);
	return no_err;

      } else if (p != 0) {

	return make_err(unknown_key, key);

      } else {

	return make_err(bad_value, key, value, "a positive integer");

      }
      break;
    }
    case KeyInfoList: {

      char a;
      if (p == 0) {
	abort(); //FIXME
	//return ret.prim_err(list_set, key); 
      } else if (p == 7) {        // prefix must be "rem-all-"
	if (value[0] != '\0') {
	  return make_err(bad_value, k, value, "nothing");
	}
	a = '!';
      } else if (k[0] == 'a') { // prefix must be "add-"
	a = '+';
      } else {                  // prefix must be "rem-"
	a = '-';
      }

      if (a != '!') {
	i = data_.lookup(key);
	if (i == 0) i = "";
	String s = i;
	s += ',';
	s += a;
	s += value;
	data_.replace(key, s);
      } else {
	data_.replace(key, "!");
      }

      switch (a) {
      case '!': 
	notify_all(ki, value, all_removed);  
	break;
      case '+': 
	notify_all(ki, value, item_added);
	break;
      case '-': 
	notify_all(ki, value, item_removed);
	break;
      }
      }
      break;
    }

    return no_err;
  }

  PosibErr<bool> Config::remove (ParmString k) {

    RET_ON_ERR_SET(keyinfo(k), const KeyInfo *, ki);
    const char * key = ki->name;

    if (attached_ && !(ki->flags & KEYINFO_MAY_CHANGE))
      return make_err(cant_change_value, key);
  
    assert(ki->def != 0); // if null this key should never have values
    // directly added to it

    bool success = data_.remove(key);

    switch (ki->type) {

    case KeyInfoString:

      notify_all(ki, retrieve(key), item_updated);
      break;
    
    case KeyInfoBool:

      notify_all(ki, retrieve_bool(key), item_updated);
      break;

    case KeyInfoInt:

      notify_all(ki, retrieve_int(key), item_updated);
      break;

    case KeyInfoList:
    
      NotifyListBlockChange n(ki, notifier_list);
      RET_ON_ERR(retrieve_list(key, &n));
      break;
    }

    return success;
  }

  StringPairEnumeration * Config::elements() 
  {
    return data_.elements();
  }


  /////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////

  class PossibleElementsEmul : public KeyInfoEnumeration
  {
  private:
    bool include_extra;
    const Config * cd;
    const KeyInfo * i;
    const ConfigModule * m;
  public:
    PossibleElementsEmul(const Config * d, bool ic)
      : include_extra(ic), cd(d), i(d->kmi.main_begin), m(0) {}

    KeyInfoEnumeration * clone() const {
      return new PossibleElementsEmul(*this);
    }

    void assign(const KeyInfoEnumeration * other) {
      *this = *(const PossibleElementsEmul *)(other);
    }

    const char * active_filter_module_name(void){
      if (m != 0)
        return m->name;
      return "";
    }

    const KeyInfo * next() {
      if (i == cd->kmi.main_end) {
	if (include_extra)
	  i = cd->kmi.extra_begin;
	else
	  i = cd->kmi.extra_end;
      }
      
      if (i == cd->kmi.extra_end) {
	m = cd->kmi.filter_modules_begin;
	if (m == cd->kmi.filter_modules_end) return 0;
	else i = m->begin;
      }

      if (m == 0){
	return i++;
      }

      if (m == cd->kmi.filter_modules_end){
	return 0;
      }

      while (i == m->end) {
	++m;
	if (m == cd->kmi.filter_modules_end) return 0;
	else i = m->begin;
      }

      return i++;
    }

    bool at_end() const {
      return (m == cd->kmi.filter_modules_end);
    }
  };

  KeyInfoEnumeration *
  Config::possible_elements(bool include_extra)
  {
    return new PossibleElementsEmul(this, include_extra);
  }

  class ListDump : public MutableContainer 
  {
    OStream & out;
    const char * name;
  public:
    ListDump(OStream & o, ParmString n) 
      : out(o), name(n) {}
    PosibErr<bool> add(ParmString d) {
      out << "add-" << name << ' ' << d << '\n';
      return true;
    }
    PosibErr<bool> remove(ParmString d) {
      out << "rem-" << name << ' ' << d << '\n';
      return true;
    }
    PosibErr<void> clear() {
      out << "rem-all-" << name << '\n';
      return no_err;
    }
  };

  void Config::write_to_stream(OStream & out, 
			       bool include_extra) 
  {
    KeyInfoEnumeration * els = possible_elements(include_extra);
    const KeyInfo * i;
    CharVector buf;

    while ((i = els->next()) != 0) {
      if (i->desc == 0) continue;
      if (i->type == KeyInfoDescript) {
        int prefix_end = 0;
        if(strncmp(i->name,"filter-",7) == 0)
          prefix_end = 7;
        out << "###  " << &(i->name)[prefix_end] << " Filter: " << _(i->desc)
            << "\n###    " << _("configured as follows") << "\n\n\n";
        continue;
      }
      out << "# " << (i->type ==  KeyInfoList ? "add|rem-" : "") << i->name
	  << " descrip: " << (i->def == 0 ? "(action option) " : "") << _(i->desc)
	  << '\n';
      if (i->def != 0) {
	buf.resize(strlen(i->def) * 2 + 1);
	escape(buf.data(), i->def);
	out << "# " << i->name << " default: " << buf.data() << '\n';
	String val = retrieve(i->name);
	buf.resize(val.size() * 2 + 1);
	escape(buf.data(), val.c_str());
	if (i->type != KeyInfoList) {
	  out << "# " << i->name << " current: " << buf.data() << "\n";
	  if (have(i->name))
	    out << i->name << " " <<  buf.data() << "\n";
	} else {
	  const char * value = data_.lookup(i->name);
	  if (value != 0) {
	    ListDump ld(out, i->name);
	    itemize(value, ld);
	  }
	}
      }
      out << "\n\n";
    }
    delete els;
  }

  PosibErr<void> Config::read_in(IStream & in, ParmString id) 
  {
    String buf;
    DataPair dp;
    while (getdata_pair(in, dp, buf)) {
      unescape(dp.value);
      PosibErrBase pe = replace(dp.key, dp.value);
      if (pe.has_err()) {
        if (id.empty())
          return pe;
        else
          return pe.with_file(id, dp.line_num);
      }
    }
    return no_err;
  }

  PosibErr<void> Config::read_in_file(ParmString file) {
    FStream in;
    RET_ON_ERR(in.open(file, "r"));
    return read_in(in, file);
  }

  PosibErr<void> Config::read_in_string(ParmString str) {
    StringIStream in(str);
    return read_in(in);
  }

  void Config::merge(const Config & other) {
    StackPtr<KeyInfoEnumeration> els(possible_elements());
    const KeyInfo * k;
    const KeyInfo * other_k;
    const char * other_name;
    String this_value;
    String other_value;
    while ( (k = els->next()) != 0) {
      if (k->type == KeyInfoDescript) continue; // FIXME: hack

      other_name = k->name;
      other_k = other.keyinfo(other_name);

      if (other_k != 0 &&
          strcmp(k->def, other_k->def) == 0
          && !other.have(other_name))
        continue;
      {
        PosibErr<String> pe = other.retrieve(other_name);
        if (pe.get_err() != 0) continue;
        // if an err then this key does not exist in the other
        // table.
        other_value = pe;
      }
      if (other_value == "(default)") continue;
      this_value = retrieve(k->name);
      if (this_value == other_value &&
          !other.have(k->name)) continue;
      // if the two values match there is no need to insert it into the
      // table unless the other value is specificly set
      if (k->type != KeyInfoList) {
        data_.replace(k->name, other_value);
      } else {
        String new_value;
        if (other_value[0] != '!') {
          new_value  = this_value;
          new_value += ',';
        }
        new_value += other_value;
        data_.replace(k->name, new_value);
      }
    }
  }

  PosibErr<void> Config::read_in_settings(const Config * override)
  {
    // FIXME: make this more robust.  
    // Catch errors and atatched there source of origin

    {
      PosibErrBase pe = read_in_file(retrieve("conf-path"));
      if (pe.has_err() && !pe.has_err(cant_read_file)){
	return pe;
      }
    }

    {
      PosibErrBase pe = read_in_file(retrieve("per-conf-path"));
      if (pe.has_err() && !pe.has_err(cant_read_file)){
	return pe;
      }
    }
    const char * env = getenv("ASPELL_CONF");
    if (env != 0){
      RET_ON_ERR(read_in_string(env));
    }

    if (override != 0){
      merge(*override);
    }

    return no_err;
  }


#ifdef ENABLE_WIN32_RELOCATABLE
#  define HOME_DIR "<prefix>"
#  define PERSONAL "<actual-lang>.pws"
#  define REPL     "<actual-lang>.prepl"
#else
#  define HOME_DIR "<$HOME|./>"
#  define PERSONAL ".aspell.<actual-lang>.pws"
#  define REPL     ".aspell.<actual-lang>.prepl"
#endif

  char mode_string[128] = "filter mode";

  // FIXME: CANT_CHANGE should be the default once attached

  static const KeyInfo config_keys[] = {
    // the description should be under 50 chars
    {"actual-dict-dir", KeyInfoString, "<dict-dir^master>", 0}
    , {"actual-lang",     KeyInfoString, "!actual-lang", 0}
    , {"conf",     KeyInfoString, "aspell.conf",
       /* TRANSLATORS: The remaing strings in config.cpp should be kept
          under 50 characters, begin with a lower case character and not
          include any trailing punctuation marks. */
       N_("main configuration file")}
    , {"conf-dir", KeyInfoString, CONF_DIR,
       N_("location of main configuration file")}
    , {"conf-path",     KeyInfoString, "<conf-dir/conf>",     0}
    , {"data-dir", KeyInfoString, DATA_DIR,
       N_("location of language data files")}
    , {"dict-dir", KeyInfoString, DICT_DIR,
       N_("location of the main word list")}
    , {"encoding",   KeyInfoString, "iso8859-1",
       N_("encoding to expect data to be in")}
    , {"filter",   KeyInfoList  , "url",
       N_("add or removes a filter"), KEYINFO_MAY_CHANGE}
    , {"filter-path", KeyInfoList, FILTER_DIR,
       N_("path(es) aspell looks for filters")}
    , {"option-path", KeyInfoList, FILTER_OPT_DIR,
       N_("path(es) aspell looks for options descriptions")}
    , {"mode",     KeyInfoString, "url",
       mode_string}
    , {"extra-dicts", KeyInfoList, "",
       N_("extra dictionaries to use")}
    , {"home-dir", KeyInfoString, HOME_DIR,
       N_("location for personal files")}
    , {"ignore",   KeyInfoInt   , "1",
       N_("ignore words <= n chars"), KEYINFO_MAY_CHANGE}
    , {"ignore-accents" , KeyInfoBool, "false",
       N_("ignore accents when checking words"), KEYINFO_MAY_CHANGE}
    , {"ignore-case", KeyInfoBool  , "false",
       N_("ignore case when checking words"), KEYINFO_MAY_CHANGE}
    , {"ignore-repl", KeyInfoBool  , "false",
       N_("ignore commands to store replacement pairs"), KEYINFO_MAY_CHANGE}
    , {"jargon",     KeyInfoString, "",
       N_("extra information for the word list")}
    , {"keyboard", KeyInfoString, "standard",
       N_("keyboard definition to use for typo analysis")}
    , {"lang", KeyInfoString, "<language-tag>",
       N_("language code")}
    , {"language-tag", KeyInfoString, "!lang",
       N_("deprecated, use lang instead")}
    , {"local-data-dir", KeyInfoString, "<actual-dict-dir>",
       N_("location of local language data files")     }
    , {"master",        KeyInfoString, "",
       N_("base name of the main dictionary to use")}
    , {"master-flags",  KeyInfoString, "", 0}
    , {"master-path",   KeyInfoString, "<dict-dir/master>",   0}
    , {"module",        KeyInfoString, "default",
       N_("set module name")}
    , {"module-search-order", KeyInfoList, "",
       N_("search order for modules")}
    , {"per-conf", KeyInfoString, ".aspell.conf",
       N_("personal configuration file")}
    , {"per-conf-path", KeyInfoString, "<home-dir/per-conf>", 0}
    , {"personal", KeyInfoString, PERSONAL,
       N_("personal dictionary file name")}
    , {"personal-path", KeyInfoString, "<home-dir/personal>", 0}
    , {"prefix",   KeyInfoString, PREFIX,
       N_("prefix directory")}
    , {"repl",     KeyInfoString, REPL,
       N_("replacements list file name") }
    , {"repl-path",     KeyInfoString, "<home-dir/repl>",     0}
    , {"run-together",        KeyInfoBool,  "false",
       N_("consider run-together words legal"), KEYINFO_MAY_CHANGE}
    , {"run-together-limit",  KeyInfoInt,   "8",
       N_("maxium numbers that can be strung together"), KEYINFO_MAY_CHANGE}
    , {"run-together-min",    KeyInfoInt,   "3",
       N_("minimal length of interior words"), KEYINFO_MAY_CHANGE}
    , {"save-repl", KeyInfoBool  , "true",
       N_("save replacement pairs on save all"), KEYINFO_MAY_CHANGE}
    , {"set-prefix", KeyInfoBool, "true",
       N_("set the prefix based on executable location")}
    , {"size",          KeyInfoString, "+60",
       N_("size of the word list")}
    , {"spelling",   KeyInfoString, "",
       N_("no longer used")}
    , {"strip-accents" , KeyInfoBool, "false",
       N_("strip accents from word lists")}
    , {"sug-mode",   KeyInfoString, "normal",
       N_("suggestion mode"), KEYINFO_MAY_CHANGE}
    , {"sug-edit-dist", KeyInfoInt, "1",
       N_("edit distance to use, override sug-mode default")}
    , {"sug-typo-analysis", KeyInfoBool, "true",
       N_("use typo analysis, override sug-mode default")}
    , {"sug-repl-table", KeyInfoBool, "true",
       N_("use replacement tables, override sug-mode default")}
    , {"sug-split-chars", KeyInfoString, " -",
       N_("characters to insert when a word is split"), KEYINFO_UTF8}
    , {"word-list-path", KeyInfoList, DATA_DIR,
       N_("search path for word list information files")}
    , {"affix-char",          KeyInfoString, "/", 
       N_("indicator for affix flags in word lists"), KEYINFO_UTF8}
    , {"flag-char",           KeyInfoString, ":",
       N_("indicator for additional flags in word lists"), KEYINFO_UTF8}
    , {"use-other-dicts", KeyInfoBool, "true",
       N_("use personal, replacement & session dictionaries")}
    
    //
    // These options are only used when creating dictionaries
    // and may also be specified in the language data file
    //
    , {"use-soundslike", KeyInfoBool, "true",
       N_("encode soundslike infomation when creating dicts")}
    , {"use-jump-tables", KeyInfoBool, "true",
       N_("use jump tables when creating dictionaries")}
    , {"affix-compress", KeyInfoBool, "false",
       N_("use affix compression when creating dictionaries")}
    
    //
    // These options are specific to the "aspell" utility.  They are
    // here so that they can be specified in configuration files.
    //
    , {"backup",  KeyInfoBool, "true",
       N_("create a backup file by appending \".bak\"")}
    , {"guess", KeyInfoBool, "false",
       // FIXME: shorten description
       N_("make possible root/affix combinations not in the dictionary"), KEYINFO_MAY_CHANGE} 
    , {"keymapping", KeyInfoString, "aspell",
       N_("keymapping for check mode, aspell or ispell")}
    , {"reverse", KeyInfoBool, "false",
       N_("reverse the order of the suggest list")}
    , {"suggest", KeyInfoBool, "true",
       N_("suggest possible replacements"), KEYINFO_MAY_CHANGE}
    , {"time"   , KeyInfoBool, "false",
       N_("time load time and suggest time in pipe mode"), KEYINFO_MAY_CHANGE}
  };

  const KeyInfo * config_impl_keys_begin = config_keys;
  const KeyInfo * config_impl_keys_end   
  = config_keys + sizeof(config_keys)/sizeof(KeyInfo);

  Config * new_basic_config() {
    return new Config("aspell",
		      config_impl_keys_begin,
		      config_impl_keys_end);
  }
  
}
