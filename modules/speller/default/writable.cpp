#include <time.h>

#include "hash-t.hpp"
#include "data.hpp"
#include "data_util.hpp"
#include "enumeration.hpp"
#include "errors.hpp"
#include "file_util.hpp"
#include "fstream.hpp"
#include "language.hpp"
#include "speller_impl.hpp"

namespace aspell_writable {

//////////////////////////////////////////////////////////////////////
//
// WritableBase
//

using namespace std;
using namespace aspeller;
using namespace acommon;

class WritableBaseCode {
protected:
  String suffix;
  String compatibility_suffix;
    
  time_t cur_file_date;
    
  String compatibility_file_name;
    
  WritableBaseCode(const char * s, const char * cs)
    : suffix(s), compatibility_suffix(cs) {}
  virtual ~WritableBaseCode() {}
    
  virtual PosibErr<void> save(FStream &, ParmString) = 0;
  virtual PosibErr<void> merge(FStream &, ParmString, 
                               Config * config = 0) = 0;
    
  virtual const char * file_name() = 0;
  virtual PosibErr<void> set_file_name(ParmString name) = 0;
  virtual PosibErr<void> update_file_info(FStream & f) = 0;
    
  PosibErr<void> save2(FStream &, ParmString);
  PosibErr<void> update(FStream &, ParmString);
  PosibErr<void> save(bool do_update);
  PosibErr<void> update_file_date_info(FStream &);
  PosibErr<void> load(ParmString, Config *);
  PosibErr<void> merge(ParmString);
  PosibErr<void> save_as(ParmString);
};
  
template <typename Base>
class WritableBase : public Base, public WritableBaseCode 
{
protected:
  PosibErr<void> set_file_name(ParmString name) {
    return Base::set_file_name(name);
  }
  PosibErr<void> update_file_info(FStream & f) {
    return Base::update_file_info(f);
  }
public:
  WritableBase(const char * s, const char * cs) 
    : WritableBaseCode(s,cs) {}
    
  const char * file_name() {
    return Base::file_name();
  }
    
  PosibErr<void> load(ParmString f, Config * c, 
                      SpellerImpl *, const LocalWordSetInfo *) { 
    return WritableBaseCode::load(f,c);
  };
  PosibErr<void> merge(ParmString f) {
    return WritableBaseCode::merge(f);
  };
  PosibErr<void> save_as(ParmString f) {
    return WritableBaseCode::save_as(f);
  }
  PosibErr<void> synchronize() {
    return WritableBaseCode::save(true);
  }
  PosibErr<void> save_noupdate() {
    return WritableBaseCode::save(false);
  }
};

PosibErr<void> WritableBaseCode::update_file_date_info(FStream & f) {
  RET_ON_ERR(update_file_info(f));
  cur_file_date = get_modification_time(f);
  return no_err;
}
  
PosibErr<void> WritableBaseCode::load(ParmString f0, Config * config)
{
  set_file_name(f0);
  const String f = file_name();
  FStream in;
    
  if (file_exists(f)) {
      
    RET_ON_ERR(open_file_readlock(in, f));
    RET_ON_ERR(merge(in, f, config));
      
  } else if (f.substr(f.size()-suffix.size(),suffix.size()) 
             == suffix) {
      
    compatibility_file_name = f.substr(0,f.size() - suffix.size());
    compatibility_file_name += compatibility_suffix;
      
    {
      PosibErr<void> pe = open_file_readlock(in, compatibility_file_name);
      if (pe.has_err()) {compatibility_file_name = ""; return pe;}
    } {
      PosibErr<void> pe = merge(in, compatibility_file_name, config);
      if (pe.has_err()) {compatibility_file_name = ""; return pe;}
    }
      
  } else {
      
    return make_err(cant_read_file,f);
      
  }

  return update_file_date_info(in);
}

PosibErr<void> WritableBaseCode::merge(ParmString f0) {
  FStream in;
  DataSet::FileName fn(f0);
  RET_ON_ERR(open_file_readlock(in, fn.path));
  RET_ON_ERR(merge(in, fn.path));
  return no_err;
}

PosibErr<void> WritableBaseCode::update(FStream & in, ParmString fn) {
  typedef PosibErr<void> Ret;
  {
    Ret pe = merge(in, fn);
    if (pe.has_err() && compatibility_file_name.empty()) return pe;
  } {
    Ret pe = update_file_date_info(in);
    if (pe.has_err() && compatibility_file_name.empty()) return pe;
  }
  return no_err;
}
    
PosibErr<void> WritableBaseCode::save2(FStream & out, ParmString fn) {
  truncate_file(out, fn);
      
  RET_ON_ERR(save(out,fn));

  out.flush();

  return no_err;
}

PosibErr<void> WritableBaseCode::save_as(ParmString fn) {
  compatibility_file_name = "";
  set_file_name(fn);
  FStream inout;
  RET_ON_ERR(open_file_writelock(inout, file_name()));
  RET_ON_ERR(save2(inout, file_name()));
  RET_ON_ERR(update_file_date_info(inout));
  return no_err;
}

PosibErr<void> WritableBaseCode::save(bool do_update) {
  FStream inout;
  RET_ON_ERR_SET(open_file_writelock(inout, file_name()),
                 bool, prev_existed);

  if (do_update
      && prev_existed 
      && get_modification_time(inout) > cur_file_date)
    RET_ON_ERR(update(inout, file_name()));

  RET_ON_ERR(save2(inout, file_name()));
  RET_ON_ERR(update_file_date_info(inout));
    
  if (compatibility_file_name.size() != 0) {
    remove_file(compatibility_file_name.c_str());
    compatibility_file_name = "";
  }

  return no_err;
}

/////////////////////////////////////////////////////////////////////
// 
//  Common Stuff
//

typedef const char * Str;

struct Hash {
  InsensitiveHash f;
  Hash(const Language * l) : f(l) {}
  size_t operator() (Str s) const {
    return f(s);
  }
};

struct Equal {
  InsensitiveEqual f;
  Equal(const Language * l) : f(l) {}
  bool operator() (Str a, Str b) const
  {
    return f(a, b);
  }
};

typedef Vector<Str> StrVector;

typedef hash_set<Str,Hash,Equal> WordLookup;
typedef hash_map<Str,StrVector>  SoundslikeLookup;

static void soundslike_next(WordEntry * w)
{
  const char * const * i   = (const char * const *)(w->intr[0]);
  const char * const * end = (const char * const *)(w->intr[1]);
  w->word = *i;
  ++i;
  if (i == end) w->adv_ = 0;
}

static void sl_init(const StrVector * tmp, WordEntry & o)
{
  const char * const * i   = tmp->pbegin();
  const char * const * end = tmp->pend();
  o.word = *i;
  o.aff  = "";
  ++i;
  if (i != end) {
    o.intr[0] = (void *)i;
    o.intr[1] = (void *)end;
    o.adv_ = soundslike_next;
  } else {
     o.intr[0] = 0;
  }
}

struct SoundslikeElements : public SoundslikeEnumeration {

  typedef SoundslikeLookup::const_iterator Itr;

  Itr i;
  Itr end;

  WordEntry d;

  SoundslikeElements(Itr i0, Itr end0) : i(i0), end(end0) {
    d.what = WordEntry::Soundslike;
  }

  WordEntry * next(int) {
    if (i == end) return 0;
    d.word = i->first;
    d.intr[0] = (void *)(&i->second);
    ++i;
    return &d;
  }
};

struct StrippedElements : public SoundslikeEnumeration {

  typedef WordLookup::const_iterator Itr;

  Itr i;
  Itr end;

  WordEntry d;

  StrippedElements(Itr i0, Itr end0) : i(i0), end(end0) {
    d.what = WordEntry::Word;
    d.aff  = "";
  }

  WordEntry * next(int) {
    if (i == end) return 0;
    d.word = *i;
    ++i;
    return &d;
  }
};

struct ElementsParms {
  typedef WordEntry *                Value;
  typedef WordLookup::const_iterator Iterator;
  Iterator end_;
  WordEntry data;
  ElementsParms(Iterator e) : end_(e) {}
  bool endf(Iterator i) const {return i==end_;}
  Value deref(Iterator i) {data.word = *i; return &data;}
  static Value end_state() {return 0;}
};

/////////////////////////////////////////////////////////////////////
// 
//  WritableWS
//

class WritableWS : public WritableBase<WritableWordSet>
{
public: //but don't use
  StackPtr<WordLookup> word_lookup;
  SoundslikeLookup     soundslike_lookup_;
  ObjStack             buffer;

  PosibErr<void> save(FStream &, ParmString);
  PosibErr<void> merge(FStream &, ParmString, Config * config = 0);

protected:
  void set_lang_hook(Config *) {
    word_lookup.reset(new WordLookup(10, Hash(lang()), Equal(lang())));
  }
    
public:

  WritableWS() : WritableBase<WritableWordSet>(".pws", ".per") {
    have_soundslike = true; fast_lookup = true;
  }

  Size   size()     const;
  bool   empty()    const;
  
  PosibErr<void> add(ParmString w);
  PosibErr<void> add(ParmString w, ParmString s);
  PosibErr<void> clear() {abort();} // FIXME

  bool lookup (ParmString word, WordEntry &, const SensitiveCompare &) const;

  bool stripped_lookup(const char * sondslike, WordEntry &) const;

  bool soundslike_lookup(const char * soundslike, WordEntry &) const;
  bool soundslike_lookup(const WordEntry & soundslike, WordEntry &) const;

  WordEntryEnumeration * detailed_elements() const;

  SoundslikeEnumeration * soundslike_elements() const;
};

WritableWS::Size WritableWS::size() const 
{
  return word_lookup->size();
}

bool WritableWS::empty() const 
{
  return word_lookup->empty();
}

bool WritableWS::lookup(ParmString word, WordEntry & o,
                        const SensitiveCompare & c) const
{
  o.clear();
  pair<WordLookup::iterator, WordLookup::iterator> p(word_lookup->equal_range(word));
  while (p.first != p.second) {
    if (c(word,*p.first)) {
      o.what = WordEntry::Word;
      o.word = *p.first;
      o.aff  = "";
      return true;
    }
    ++p.first;
  }
  return false;
}

bool WritableWS::stripped_lookup(const char * sl, WordEntry & o) const
{
  o.clear();
  pair<WordLookup::iterator, WordLookup::iterator> p(word_lookup->equal_range(sl));
  if (p.first == p.second) return false;
  o.what = WordEntry::Word;
  o.word = *p.first;
  o.aff  = "";
  return true;
  // FIXME: Deal with multiple entries
}  

bool WritableWS::soundslike_lookup(const WordEntry & word, WordEntry & o) const 
{
  if (have_soundslike) {
    const StrVector * tmp 
      = (const StrVector *)(word.intr[0]);
    o.clear();
    o.what = WordEntry::Word;
    sl_init(tmp, o);
  } else {
    o.what = WordEntry::Word;
    o.word = word.word;
    o.aff  = "";
  }
  return true;
}

bool WritableWS::soundslike_lookup(const char * soundslike, WordEntry & o) const {
  if (have_soundslike) {
    o.clear();
    SoundslikeLookup::const_iterator i = soundslike_lookup_.find(soundslike);
    if (i == soundslike_lookup_.end()) {
      return false;
    } else {
      o.what = WordEntry::Word;
      sl_init(&i->second, o);
      return true;
    }
  } else {
    return WritableWS::stripped_lookup(soundslike, o);
  }
}

SoundslikeEnumeration * WritableWS::soundslike_elements() const {
  if (have_soundslike)
    return new SoundslikeElements(soundslike_lookup_.begin(), 
                                  soundslike_lookup_.end());
  else
    return new StrippedElements(word_lookup->begin(),
                                word_lookup->end());
}

WritableWS::Enum * WritableWS::detailed_elements() const {
  return new MakeEnumeration<ElementsParms>
    (word_lookup->begin(),ElementsParms(word_lookup->end()));
}
  
PosibErr<void> WritableWS::add(ParmString w) {
  return WritableWS::add(w, have_soundslike ? lang()->to_soundslike(w) : "");
}

PosibErr<void> WritableWS::add(ParmString w, ParmString s) {
  RET_ON_ERR(check_if_valid(*lang(),w));
  SensitiveCompare c(lang());
  WordEntry we;
  if (WritableWS::lookup(w,we,c)) return no_err;
  const char * w2 = buffer.dup(w);
  word_lookup->insert(w2);
  if (have_soundslike)
    soundslike_lookup_[buffer.dup(s)].push_back(w2);
  return no_err;
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

  SoundslikeLookup::const_iterator i = soundslike_lookup_.begin();
  SoundslikeLookup::const_iterator e = soundslike_lookup_.end();
    
  StrVector::const_iterator j;
  
  for (;i != e; ++i) {
    for (j = i->second.begin(); j != i->second.end(); ++j) {
      out << *j << '\n';
    }
  }
  return no_err;
}

/////////////////////////////////////////////////////////////////////
// 
//  WritableReplList
//

static inline StrVector * get_vector(Str s) 
{
  return (StrVector *)(s - sizeof(StrVector));
}

class WritableReplS : public WritableBase<WritableReplacementSet>
{
private:
  StackPtr<WordLookup> word_lookup;
  SoundslikeLookup         soundslike_lookup_;
  StringBuffer             buffer;

  WritableReplS(const WritableReplS&);
  WritableReplS& operator=(const WritableReplS&);

protected:
  void set_lang_hook(Config *) {
    word_lookup.reset(new WordLookup(10, Hash(lang()), Equal(lang())));
  }

public:
  WritableReplS() : WritableBase<WritableReplacementSet>(".prepl",".rpl") 
  {
    have_soundslike = true;
    fast_lookup = true;
  }
  ~WritableReplS();

  Size   size()     const;
  bool   empty()    const;

  bool lookup(ParmString, WordEntry &, const SensitiveCompare &) const;

  bool stripped_lookup(const char * sondslike, WordEntry &) const;

  bool soundslike_lookup(const WordEntry &, WordEntry &) const;
  bool soundslike_lookup(const char * soundslike, WordEntry &) const;

  bool repl_lookup(const WordEntry &, WordEntry &) const;
  bool repl_lookup(const char * word, WordEntry &) const;
      
  WordEntryEnumeration * detailed_elements() const;
  SoundslikeEnumeration * soundslike_elements() const;
      
  PosibErr<void> add(ParmString mis, ParmString cor);
  PosibErr<void> add(ParmString mis, ParmString cor, ParmString s);
  PosibErr<void> clear() {abort();} // FIXME

private:
  PosibErr<void> save(FStream &, ParmString );
  PosibErr<void> merge(FStream &, ParmString , Config * config = 0);
};

WritableReplS::Size WritableReplS::size() const 
{
  return word_lookup->size();
}

bool WritableReplS::empty() const 
{
  return word_lookup->empty();
}
    
bool WritableReplS::lookup(ParmString word, WordEntry & o,
                           const SensitiveCompare & c) const
{
  o.clear();
  pair<WordLookup::iterator, WordLookup::iterator> p(word_lookup->equal_range(word));
  while (p.first != p.second) {
    if (c(word,*p.first)) {
      o.what = WordEntry::Misspelled;
      o.word = *p.first;
      o.intr[0] = (void *)*p.first;
      return true;
    }
    ++p.first;
  }
  return false;
}

bool WritableReplS::stripped_lookup(const char * sl, WordEntry & o) const
{
  o.clear();
  pair<WordLookup::iterator, WordLookup::iterator> p(word_lookup->equal_range(sl));
  if (p.first == p.second) return false;
  o.what = WordEntry::Misspelled;
  o.word = *p.first;
  o.intr[0] = (void *)*p.first;
  return true;
  // FIXME: Deal with multiple entries
}  

bool WritableReplS::soundslike_lookup(const WordEntry & word, WordEntry & o) const 
{
  if (have_soundslike) {
    const StrVector * tmp = (const StrVector *)(word.intr[0]);
    o.clear();
    o.what = WordEntry::Misspelled;
    sl_init(tmp, o);
  } else {
    o.what = WordEntry::Misspelled;
    o.word = word.word;
  }
  return true;
}

bool WritableReplS::soundslike_lookup(const char * soundslike, WordEntry & o) const {
  if (have_soundslike) {
    o.clear();
    SoundslikeLookup::const_iterator i = soundslike_lookup_.find(soundslike);
    if (i == soundslike_lookup_.end()) {
      return false;
    } else {
      o.what = WordEntry::Misspelled;
      sl_init(&(i->second), o);
      return true;
    }
  } else {
    return WritableReplS::stripped_lookup(soundslike, o);
  }
}

SoundslikeEnumeration * WritableReplS::soundslike_elements() const {
  if (have_soundslike)
    return new SoundslikeElements(soundslike_lookup_.begin(), 
                                  soundslike_lookup_.end());
  else
    return new StrippedElements(word_lookup->begin(),
                                word_lookup->end());
}

WritableReplS::Enum * WritableReplS::detailed_elements() const {
  return new MakeEnumeration<ElementsParms>
    (word_lookup->begin(),ElementsParms(word_lookup->end()));
}

static void repl_next(WordEntry * w)
{
  const Str * i   = (const Str *)(w->intr[0]);
  const Str * end = (const Str *)(w->intr[1]);
  w->word = *i;
  ++i;
  if (i == end) w->adv_ = 0;
}

static void repl_init(const StrVector * tmp, WordEntry & o)
{
  o.what = WordEntry::Word;
  const Str * i   = tmp->pbegin();
  const Str * end = tmp->pend();
  o.word = *i;
  o.aff  = "";
  ++i;
  if (i != end) {
    o.intr[0] = (void *)i;
    o.intr[1] = (void *)end;
    o.adv_ = repl_next;
  } else {
    o.intr[0] = 0;
  }
}
  
bool WritableReplS::repl_lookup(const WordEntry & w, WordEntry & o) const 
{
  const StrVector * repls;
  if (w.intr[0] && !w.intr[1]) { // the intr are not for the sl iter
    repls = get_vector(w.word);
  } else {
    SensitiveCompare c(lang()); // FIXME: This is not exactly right
    WordEntry tmp;
    WritableReplS::lookup(w.word, tmp, c);
    repls = get_vector(tmp.word);
    if (!repls) return false;
  }
  o.clear();
  repl_init(repls, o);
  return true;
}

bool WritableReplS::repl_lookup(const char * word, WordEntry & o) const
{
  WordEntry w;
  w.word = word;
  return WritableReplS::repl_lookup(w, o);
}

PosibErr<void> WritableReplS::add(ParmString mis, ParmString cor) 
{
  return WritableReplS::add(mis, cor, have_soundslike ? lang()->to_soundslike(mis) : "");
}

PosibErr<void> WritableReplS::add(ParmString mis, ParmString cor, ParmString sl) 
{
  Str m, c, s;
  SensitiveCompare cmp(lang()); // FIXME: I don't think this is completely correct
  WordEntry we;

  pair<WordLookup::iterator, WordLookup::iterator> p0(word_lookup->equal_range(mis));
  WordLookup::iterator p = p0.first;

  for (; p != p0.second && !cmp(mis,*p); ++p);

  if (p == p0.second) {
    char * m0  = (char *)buffer.alloc(sizeof(StrVector) + mis.size() + 1, sizeof(void *));
    new (m0) StrVector;
    m0 += sizeof(StrVector);
    memcpy(m0, mis.str(), mis.size() + 1);
    m = m0;
    p = word_lookup->insert(m).first;
  } else {
    m = *p;
  }

  StrVector * v = get_vector(m);

  for (StrVector::iterator i = v->begin(); i != v->end(); ++i)
    if (cmp(cor, *i)) return no_err;
    
  c = buffer.dup(cor);
  get_vector(m)->push_back(c);

  if (have_soundslike) {
    s = buffer.dup(sl);
    soundslike_lookup_[s].push_back(m);
  }

  return no_err;
}

PosibErr<void> WritableReplS::save (FStream & out, ParmString file_name) 
{
  out << "personal_repl-1.1" << ' ' << lang_name() <<  " 0 \n";
  
  WordLookup::iterator i = word_lookup->begin();
  WordLookup::iterator e = word_lookup->end();
  
  for (;i != e; ++i) 
  {
    StrVector * v = get_vector(*i);
    for (StrVector::iterator j = v->begin(); j != v->end(); ++j)
    {
      out << *i << ' ' << *j << '\n';
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
      WritableReplS::add(mis, repl);
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
          WritableReplS::add(mis, repl);
        }
      }
    }

  }
  return no_err;
}

WritableReplS::~WritableReplS()
{
  WordLookup::iterator i = word_lookup->begin();
  WordLookup::iterator e = word_lookup->end();
  
  for (;i != e; ++i) 
    get_vector(*i)->~StrVector();
}

}

using namespace aspell_writable;

namespace aspeller {

  WritableWordSet * new_default_writable_word_set() {
    return new WritableWS();
  }

  WritableReplacementSet * new_default_writable_replacement_set() {
    return new WritableReplS();
  }

}



