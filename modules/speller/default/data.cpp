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

#include "iostream.hpp"

namespace aspeller {

  //
  // DataSet impl
  //

  DataSet::Id::Id(DataSet * p, const FileName & fn)
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

  bool operator==(const DataSet::Id & rhs, const DataSet::Id & lhs)
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

  PosibErr<void> DataSet::attach(const Language &l) {
    if (lang_ && strcmp(l.name(),lang_->name()) != 0)
      return make_err(mismatched_language, lang_->name(), l.name());
    if (!lang_) lang_.copy(&l);
    ++attach_count_;
    return no_err;
  }

  void DataSet::detach() {
    --attach_count_;
  }

  DataSet::DataSet()
    : lang_(), attach_count_(0), id_(), basic_type(no_type) 
  {
    id_.reset(new Id(this));
  }

  DataSet::~DataSet() {
  }

  bool DataSet::is_attached() const {
    return attach_count_;
  }

  const char * DataSet::lang_name() const {
    return lang_->name();
  }

  PosibErr<void> DataSet::check_lang(ParmString l) {
    if (l != lang_->name())
      return make_err(mismatched_language, lang_->name(), l);
    return no_err;
  }

  PosibErr<void> DataSet::set_check_lang (ParmString l, Config * config)
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

  void DataSet::FileName::copy(const FileName & other) 
  {
    const_cast<String &      >(path) = other.path;
    const_cast<const char * &>(name) = path.c_str() + (other.name - other.path.c_str());
  }

  void DataSet::FileName::clear()
  {
    path  = "";
    name = path.c_str();
  }

  void DataSet::FileName::set(ParmString str) 
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
  // LoadableDataSet impl
  //

  PosibErr<void> LoadableDataSet::set_file_name(ParmString fn) 
  {
    file_name_.set(fn);
    *id_ = Id(this, file_name_);
    return no_err;
  }

  PosibErr<void> LoadableDataSet::update_file_info(FStream & f) 
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
  // BasicWordSet
  //

//   class BasicWordSetEnumeration : public StringEnumeration 
//   {
//     BasicWordSet::Emul real_;
//   public:
//     BasicWordSetEnumeration(BasicWordSet::VirEmul * r) : real_(r) {}

//     bool at_end() const {
//       return real_.at_end();
//     }
//     const char * next() {
//       return real_.next().word; // FIXME: It's not this simple
//     }
//     StringEnumeration * clone() const {
//       return new BasicWordSetEnumeration(*this);
//     }
//     void assign(const StringEnumeration * other) {
//       *this = *static_cast<const BasicWordSetEnumeration *>(other);
//     }
//   };

  StringEnumeration * BasicWordSet::elements() const 
  {
    abort(); // FIXME
    //return new BasicWordSetEnumeration(detailed_elements());
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

  PosibErr<LoadableDataSet *> add_data_set(ParmString fn,
					   Config & config,
					   SpellerImpl * speller,
					   const LocalWordSetInfo * local_info,
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
    DataSet::FileName file_name(fn);
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

    LoadableDataSet * ws = 0;
    if (speller != 0)
      ws = speller
	->locate(DataSet::Id(0,DataSet::FileName(true_file_name))).word_set;

    if (ws != 0) {
      //  cerr << "Warning: " << true_file_name << " already exists!" << endl;      
      return ws;
    }

    switch (actual_type) {
    case DT_ReadOnly: 
      ws = new_default_readonly_word_set();
      break;
    case DT_Multi:
      ws = new_default_multi_word_set();
      break;
    case DT_Writable: 
      ws = new_default_writable_word_set(); 
      break;
    case DT_WritableRepl:
      ws = new_default_writable_replacement_set();
      break;
    default:
      abort();
    }

    RET_ON_ERR(ws->load(true_file_name, &config, speller, local_info));
    if (speller != 0)
      speller->steal(ws, local_info);

    return ws;
    
  }

  //
  // LocalWordSetInfo
  //
  
  void LocalWordSetInfo::set_language(const Language * l)
  {
    compare.lang = l;
    convert.lang = l;
  }

  void LocalWordSetInfo::set(const Language * l, const Config * c, bool strip)
  {
    if (c->have("strip-accents"))
      strip = c->retrieve_bool("strip-accents");

    compare.lang = l;
    compare.case_insensitive = c->retrieve_bool("ignore-case");
    compare.ignore_accents   = c->retrieve_bool("ignore-accents");
    compare.strip_accents    = strip;
    convert.lang = l;
    convert.strip_accents = strip;
  }
  
}

