// This file is part of The New Aspell
// Copyright (C) 2000-2001 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.

#include "config.hpp"
#include "convert.hpp"
#include "data.hpp"
#include "data_id.hpp"
#include "errors.hpp"
#include "file_util.hpp"
#include "fstream.hpp"
#include "language.hpp"
#include "speller_impl.hpp"
#include "cache-t.hpp"

#include "iostream.hpp"

namespace aspeller {

  GlobalCache<Dict> dict_cache("dictionary");

  //
  // Dict impl
  //

  Dict::Id::Id(Dict * p, const FileName & fn)
    : ptr(p)
  {
    file_name = fn.name;
#ifdef USE_FILE_INO
    struct stat s;
    // the file ,i
    if (file_name[0] != '\0' && stat(fn.path.c_str(), &s) == 0) {
      ino = s.st_ino;
      dev = s.st_dev;
    } else {
      ino = 0;
      dev = 0;
    }
#endif
  }

  bool operator==(const Dict::Id & rhs, const Dict::Id & lhs)
  {
    if (rhs.ptr == 0 || lhs.ptr == 0) {
      if (rhs.file_name == 0 || lhs.file_name == 0)
	return false;
#ifdef USE_FILE_INO
      return rhs.ino == lhs.ino && rhs.dev == lhs.dev;
#else
      return strcmp(rhs.file_name, lhs.file_name) == 0;
#endif
    } else {
      return rhs.ptr == lhs.ptr;
    }
  }

  PosibErr<void> Dict::attach(const Language &l) {
    if (lang_ && strcmp(l.name(),lang_->name()) != 0)
      return make_err(mismatched_language, lang_->name(), l.name());
    if (!lang_) lang_.copy(&l);
    copy();
    return no_err;
  }

  Dict::Dict()
    : Cacheable(&dict_cache), lang_(), id_(), basic_type(no_type) 
  {
    id_.reset(new Id(this));
  }

  Dict::~Dict() {
  }

  const char * Dict::lang_name() const {
    return lang_->name();
  }

  PosibErr<void> Dict::check_lang(ParmString l) {
    if (l != lang_->name())
      return make_err(mismatched_language, lang_->name(), l);
    return no_err;
  }

  PosibErr<void> Dict::set_check_lang (ParmString l, const Config * config)
  {
    if (lang_ == 0) {
      PosibErr<Language *> res = new_language(*config, l);
      if (res.has_err()) return res;
      lang_.reset(res.data);
      set_lang_hook(config);
    } else {
      if (l != lang_->name())
	return make_err(mismatched_language, l, lang_->name());
    }
    return no_err;
  }

  void Dict::FileName::copy(const FileName & other) 
  {
    const_cast<String &      >(path) = other.path;
    const_cast<const char * &>(name) = path.c_str() + (other.name - other.path.c_str());
  }

  void Dict::FileName::clear()
  {
    path  = "";
    name = path.c_str();
  }

  void Dict::FileName::set(ParmString str) 
  {
    path = str;
    int i = path.size() - 1;
    while (i >= 0) {
      if (path[i] == '/' || path[i] == '\\') {
	++i;
	break;
      }
      --i;
    }
    name = path.c_str() + i;
  }

  //
  // LoadableDict impl
  //

  PosibErr<void> LoadableDict::set_file_name(ParmString fn) 
  {
    file_name_.set(fn);
    *id_ = Id(this, file_name_);
    return no_err;
  }

  PosibErr<void> LoadableDict::update_file_info(FStream & f) 
  {
#ifdef USE_FILE_INO
    struct stat s;
    int ok = fstat(f.file_no(), &s);
    assert(ok == 0);
    id_->ino = s.st_ino;
    id_->dev = s.st_dev;
#endif
    return no_err;
  }

  //
  // BasicDict
  //

//   class BasicDictEnumeration : public StringEnumeration 
//   {
//     BasicDict::Emul real_;
//   public:
//     BasicDictEnumeration(BasicDict::VirEmul * r) : real_(r) {}

//     bool at_end() const {
//       return real_.at_end();
//     }
//     const char * next() {
//       return real_.next().word; // FIXME: It's not this simple
//     }
//     StringEnumeration * clone() const {
//       return new BasicDictEnumeration(*this);
//     }
//     void assign(const StringEnumeration * other) {
//       *this = *static_cast<const BasicDictEnumeration *>(other);
//     }
//   };

  StringEnumeration * BasicDict::elements() const 
  {
    abort(); // FIXME
    //return new BasicDictEnumeration(detailed_elements());
  }

#define write_conv(s) do { \
    if (!c) {o << s;} \
    else {ParmString ss(s); buf.clear(); c->convert(ss.str(), ss.size(), buf); o.write(buf.data(), buf.size()-1);} \
  } while (false)

  OStream & WordEntry::write (OStream & o,
                              const Language & l,
                              const ConvertWord & cw,
                              Convert * c) const
  {
    String w;
    CharVector buf;
    cw.convert(word, w);
    write_conv(w);
    if (aff && *aff) {
      o << '/';
      write_conv(aff);
    }
    return o;
  }

  PosibErr<void> add_data_set(ParmString fn,
                              const Config & config,
                              LocalDict & res,
                              LocalDictList * new_dicts,
                              SpellerImpl * speller,
                              const LocalDictInfo * local_info,
                              ParmString dir,
                              DataType allowed)
  {
    static const char * suffix_list[] = {"", ".multi", ".alias", 
					 ".spcl", ".special",
					 ".pws", ".prepl"};
    FStream in;
    const char * * suffix;
    const char * * suffix_end 
      = suffix_list + sizeof(suffix_list)/sizeof(const char *);
    String dict_dir = config.retrieve("dict-dir");
    String true_file_name;
    Dict::FileName file_name(fn);
    const char * d = dir;
    do {
      if (d == 0) d = dict_dir.c_str();
      suffix = suffix_list;
      do {
	true_file_name = add_possible_dir(d, ParmString(file_name.path)
					  + ParmString(*suffix));
	in.open(true_file_name, "r").ignore_err();
	++suffix;
      } while (!in && suffix != suffix_end);
      if (d == dict_dir.c_str()) break;
      d = 0;
    } while (!in);
    if (!in) {
      true_file_name = add_possible_dir(dir ? dir.str() : d, file_name.path);
      return make_err(cant_read_file, true_file_name);
    }
    DataType actual_type;
    if ((true_file_name.size() > 5
	 && true_file_name.substr(true_file_name.size() - 6, 6) == ".spcl") 
	||
	(true_file_name.size() > 6 
	 && (true_file_name.substr(true_file_name.size() - 6, 6) == ".multi" 
	     || true_file_name.substr(true_file_name.size() - 6, 6) == ".alias")) 
	||
	(true_file_name.size() > 8
	 && true_file_name.substr(true_file_name.size() - 6, 6) == ".special")) 
    {

      actual_type = DT_Multi;

    } else {
      
      char head[32];
      in.read(head, 32);
      if      (strncmp(head, "aspell default speller rowl", 27) ==0)
	actual_type = DT_ReadOnly;
      else if (strncmp(head, "personal_repl", 13) == 0)
	actual_type = DT_WritableRepl;
      else if (strncmp(head, "personal_ws", 11) == 0)
	actual_type = DT_Writable;
      else
	return make_err(bad_file_format, true_file_name);

    }
    
    if (actual_type & ~allowed)
      return make_err(bad_file_format, true_file_name
		      , _("is not one of the allowed types"));

    Dict::Id id(0,Dict::FileName(true_file_name));

    if (speller != 0) {
      const LocalDict * d = speller->locate(id);
      if (d != 0) {
        res = *d;
        return no_err;
      }
    }

    res.dict = 0;

    if (actual_type == DT_ReadOnly) { // try to get it from the cache
      res.dict = dict_cache.find(id);
    }

    if (!res.dict) {

      StackPtr<LoadableDict> w;
      switch (actual_type) {
      case DT_ReadOnly: 
        w = new_default_readonly_basic_dict();
        break;
      case DT_Multi:
        w = new_default_multi_dict();
        break;
      case DT_Writable: 
        w = new_default_writable_basic_dict();
        break;
      case DT_WritableRepl:
        w = new_default_writable_replacement_dict();
        break;
      default:
        abort();
      }

      RET_ON_ERR(w->load(true_file_name, config, new_dicts, speller, local_info));

      if (actual_type == DT_ReadOnly)
        dict_cache.add(w);
      
      res.dict = w.release();

    } else {
      
      res.dict->copy();
      
    }

    if (local_info) {
      res.set(*local_info);
      res.set_language(res.dict->lang());
    } else
      res.set(res.dict->lang(), config);
    if (new_dicts)
      new_dicts->add(res);
    
    return no_err;
  }

  //
  // LocalDictInfo
  //
  
  void LocalDictInfo::set_language(const Language * l)
  {
    compare.lang = l;
    convert.lang = l;
  }

  void LocalDictInfo::set(const Language * l, const Config & c, bool strip)
  {
    if (c.have("strip-accents"))
      strip = c.retrieve_bool("strip-accents");

    compare.lang = l;
    compare.case_insensitive = c.retrieve_bool("ignore-case");
    compare.ignore_accents   = c.retrieve_bool("ignore-accents");
    compare.strip_accents    = strip;
    convert.lang = l;
    convert.strip_accents = strip;
  }
  
}

