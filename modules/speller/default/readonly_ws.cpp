// This file is part of The New Aspell
// Copyright (C) 2000-2001 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.

// Aspell's main word list is laid out as follows:
//
// * header
// * jump table for editdist 1
// * jump table for editdist 2
// * data block
// * hash table

// data block laid out as follows:
// WITH Soundslike data:
//   (<pad><16 bit: sounds like size><16 bit: offset to next item><soundslike>
//      (<word><null>)+<null>)+
// WITHOUT Soundslike data:
//   (<8 bit: offset><8 bit: sl offset><word><null>[<affix info>]<null>)+
// compound info is currently not used (ie broken)

#include <map>

using std::pair;

#include <string.h>
#include <stdio.h>
//#include <errno.h>

#include "settings.h"

#include "block_vector.hpp"
#include "config.hpp"
#include "data.hpp"
#include "data_util.hpp"
#include "errors.hpp"
#include "file_util.hpp"
#include "fstream.hpp"
#include "language.hpp"
#include "stack_ptr.hpp"
#include "string_buffer.hpp"
#include "vector.hpp"
#include "vector_hash-t.hpp"
#include "check_list.hpp"

#include "iostream.hpp"

typedef unsigned int   u32int;
static const u32int u32int_max = (u32int)-1;
typedef unsigned short u16int;

#ifdef HAVE_MMAP 

// POSIX headers
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#endif

#ifndef MAP_FAILED 
#define MAP_FAILED (-1)
#endif

#ifdef HAVE_MMAP

static inline char * mmap_open(unsigned int block_size, 
			       FStream & f, 
			       unsigned int offset) 
{
  f.flush();
  int fd = f.file_no();
  return static_cast<char *>
    (mmap(NULL, block_size, PROT_READ, MAP_SHARED, fd, offset));
}

static inline void mmap_free(char * block, unsigned int size) 
{
  munmap(block, size);
}

// static inline size_t page_size() 
// {
// #ifdef _SC_PAGESIZE
//  /* BSDi does not expose this limit via the sysconf function */
//   return sysconf (_SC_PAGESIZE);
// #else
//   return getpagesize ();
// #endif
//}

#else

static inline char * mmap_open(unsigned int, 
			       FStream & f, 
			       unsigned int) 
{
  return reinterpret_cast<char *>(MAP_FAILED);
}

static inline void mmap_free(char *, unsigned int) 
{
  abort();
}

// static inline size_t page_size() 
// {
//   return 1024;
// }

#endif

namespace aspeller_default_readonly_ws {

  using namespace aspeller;

  /////////////////////////////////////////////////////////////////////
  // 
  //  ReadOnlyWS
  //
    
  struct SoundslikeJump
  {
    char   sl[4];
    u32int loc;
    SoundslikeJump() {memset(this, 0, sizeof(SoundslikeJump));}
  };
  
  class ReadOnlyWS : public BasicWordSet
  {
      
  public: //but don't use

    struct WordLookupParms {
      const char * block_begin;
      WordLookupParms() {}
      //WordLookupParms(const char * b, const Language * l)
      //	: block_begin(b), hash(l), equal(l) {}
      typedef BlockVector<const u32int> Vector;
      typedef u32int                    Value;
      typedef const char *              Key;
      static const bool is_multi = true;
      Key key(Value v) const {assert (v != u32int_max);
				return block_begin + v;}
      InsensitiveHash  hash;
      InsensitiveEqual equal;
      bool is_nonexistent(Value v) const {return v == u32int_max;}
      void make_nonexistent(const Value & v) const {abort();}
    };
    typedef VectorHashTable<WordLookupParms> WordLookup;

  public: // but don't use
      
    char *           block;
    u32int           block_size;
    bool             block_mmaped;
    const SoundslikeJump * jump1;
    const SoundslikeJump * jump2;
    WordLookup       word_lookup;
    const char *     word_block;
    u32int           max_word_length;
    bool             use_soundslike;
    bool             affix_info;
    
    ReadOnlyWS(const ReadOnlyWS&);
    ReadOnlyWS& operator= (const ReadOnlyWS&);

    struct ElementsParms;
    struct SoundslikeElementsBase;
    struct SoundslikeElementsWSL;
    struct SoundslikeElementsNoSL;
    struct SoundslikeElementsAffix;
    struct SoundslikeWords;
    struct SingleSoundslikeWord;

  public:
    VirEmul * detailed_elements() const;
    Size      size()     const;
    bool      empty()    const;

    BasicWordInfo convert(const char * w) const {
      if (affix_info)
        return BasicWordInfo(w,w+*(w-1),0);
      else
        return BasicWordInfo(w,"", 0);
    }
      
    ReadOnlyWS() {
      block = 0;
    }

    ~ReadOnlyWS() {
      if (block != 0) {
	if (block_mmaped)
	  mmap_free(block, block_size);
	else
	  delete[] block;
      }
    }
      
    PosibErr<void> load(ParmString, Config *, SpellerImpl *, const LocalWordSetInfo *);
    BasicWordInfo lookup (ParmString word, const SensitiveCompare &) const;
    VirEmul * words_w_soundslike(const char * soundslike) const;
    VirEmul * words_w_soundslike(SoundslikeWord soundslike) const;

    VirSoundslikeEmul * soundslike_elements() const;
  };
    
  //
  //  
  //

  struct ReadOnlyWS::ElementsParms {
    typedef BasicWordInfo                   Value;
    typedef WordLookup::const_iterator Iterator; 
    const ReadOnlyWS * ws;
    ElementsParms(const ReadOnlyWS * w) : ws(w) {}
    bool endf(const Iterator & i) const {return i.at_end();}
    Value end_state() const {return 0;}
    Value deref(const Iterator & i) const {
      return ws->convert(ws->word_block + *i);
    }
  };

  ReadOnlyWS::VirEmul * ReadOnlyWS::detailed_elements() const {
    return new MakeVirEnumeration<ElementsParms>
      (word_lookup.begin(), ElementsParms(this));
  }

  ReadOnlyWS::Size ReadOnlyWS::size() const {
    return word_lookup.size();
  }
  
  bool ReadOnlyWS::empty() const {
    return word_lookup.empty();
  }

  struct DataHead {
    // all sizes except the last four must to divisible by:
    static const uint align = 16;
    char check_word[64];
    u32int head_size;
    u32int block_size;
    u32int jump1_offset;
    u32int jump2_offset;
    u32int word_offset;
    u32int hash_offset;

    u32int word_count;
    u32int word_buckets;
    u32int soundslike_count;
    
    u32int max_word_length;

    u32int lang_name_size;
    u32int soundslike_name_size;
    u32int soundslike_version_size;
    u32int minimal_specified;
    u32int middle_chars_size;

    u32int affix_info; // none zero if affix information is encoded in table
  };

  PosibErr<void> ReadOnlyWS::load(ParmString f0, Config * config, 
				  SpellerImpl *, const LocalWordSetInfo *)
  {
    set_file_name(f0);
    const char * fn = file_name();

    FStream f;
    RET_ON_ERR(f.open(fn, "rb"));

    DataHead data_head;

    f.read(&data_head, sizeof(DataHead));

#if 0
    COUT << "Head Size: " << data_head.head_size << "\n";
    COUT << "Data Block Size: " << data_head.data_block_size << "\n";
    COUT << "Hash Block Size: " << data_head.hash_block_size << "\n";
    COUT << "Total Block Size: " << data_head.total_block_size << "\n";
#endif

    if (strcmp(data_head.check_word, "aspell default speller rowl 1.6") != 0)
      return make_err(bad_file_format, fn);

    char * word = new char[data_head.lang_name_size];
    f.read(word, data_head.lang_name_size);

    PosibErr<void> pe = set_check_lang(word,config);
    if (pe.has_err())
      return pe.with_file(fn);

    delete[] word;

    word = new char[data_head.soundslike_name_size];
    f.read(word, data_head.soundslike_name_size);

    if (strcmp(word, lang()->soundslike_name()) != 0)
      return make_err(bad_file_format, fn, "Wrong Soundslike");
    if (strcmp(word, "none") == 0)
      use_soundslike=false;
    else
      use_soundslike=true;

    delete[] word;

    word = new char[data_head.soundslike_version_size];
    f.read(word, data_head.soundslike_version_size);

    if (strcmp(word, lang()->soundslike_version()) != 0)
      return make_err(bad_file_format, fn, "Wrong Soundslike Version");

    delete[] word;

    affix_info = data_head.affix_info;

    if (data_head.minimal_specified != u32int_max) {
      word = new char[data_head.middle_chars_size];
      f.read(word, data_head.middle_chars_size);
      
      if (strcmp(word, lang()->mid_chars()) != 0)
	return make_err(bad_file_format, fn, "Different Middle Characters");
      
      delete[] word;

      if (data_head.minimal_specified != u32int_max) {
	config->replace("run-together-specified", "true");
	unsigned int m = config->retrieve_int("minimal-specified-component");
	if (data_head.minimal_specified < m) {
	  char buf[20];
	  sprintf(buf, "%i", data_head.minimal_specified);
	  config->replace("minimal-specified-component", buf);
	}
      }
    }


    block_size = data_head.block_size;
    block = mmap_open(block_size, f, data_head.head_size);
    block_mmaped = block != (char *)MAP_FAILED;
    if (!block_mmaped) {
      block = new char[block_size];
      f.seek(data_head.head_size);
      f.read(block, block_size);
    }

    jump1 = reinterpret_cast<const SoundslikeJump *>(block 
						     + data_head.jump1_offset);

    jump2 = reinterpret_cast<const SoundslikeJump *>(block 
						     + data_head.jump2_offset);

    word_block       = block + data_head.word_offset;

    word_lookup.parms().block_begin = word_block;
    word_lookup.parms().hash .lang     = lang();
    word_lookup.parms().equal.cmp.lang = lang();
    const u32int * begin = reinterpret_cast<const u32int *>
      (block + data_head.hash_offset);
    word_lookup.vector().set(begin, begin + data_head.word_buckets);
    word_lookup.set_size(data_head.word_count);
    
    max_word_length = data_head.max_word_length;
    
    return no_err;
  }

  BasicWordInfo ReadOnlyWS::lookup(ParmString word, 
				   const SensitiveCompare & c) const 
  {
    WordLookup::ConstFindIterator i = word_lookup.multi_find(word);
    for (; !i.at_end(); i.adv()) {
      const char * w = word_block + i.deref();
      if (c(word, w)) 
        return convert(w);
    }
    return 0;
  }

  struct ReadOnlyWS::SoundslikeWords : public VirEnumeration<BasicWordInfo> {
    const char * cur;
    SoundslikeWords(const char * c) : cur(c) {}
    Value next() {
      if (*cur == 0)
	return 0;
      Value tmp(cur, *(cur - 1)); 
      unsigned int len = strlen(cur);
      cur += len + 1;
      return tmp;
    }
    bool at_end() const {return *cur == 0;}
    Base * clone() const {return new SoundslikeWords(*this);}
    void assign(const Base * other) 
      {*this = *static_cast<const SoundslikeWords *>(other);}
  };

  struct ReadOnlyWS::SingleSoundslikeWord : public VirEnumeration<BasicWordInfo> {
    String cur;
    bool done;
    SingleSoundslikeWord(const char * c) : cur(c), done(false) {}
    Value next() {
      if (done) return 0;
      done = true;
      //CERR << "$$" << cur << '\n';
      return cur.c_str();
    }
    bool at_end() const {return done;}
    Base * clone() const {return new SingleSoundslikeWord(*this);}
    void assign(const Base * other) 
      {*this = *static_cast<const SingleSoundslikeWord *>(other);}
  };

  struct ReadOnlyWS::SoundslikeElementsBase : public VirSoundslikeEmul
  {
    int level;
    const ReadOnlyWS * obj;
    const SoundslikeJump * jump1;
    const SoundslikeJump * jump2;
    const char * cur;
    const char * tmp;

    virtual u16int next_pos() const = 0;

    const char * adv(int stopped_at);

    SoundslikeElementsBase(const ReadOnlyWS * o)
      : level(1), obj(o), jump1(obj->jump1), jump2(obj->jump2), cur(0) {}
  };

  struct ReadOnlyWS::SoundslikeElementsWSL : public SoundslikeElementsBase
  {
    SoundslikeElementsWSL(const ReadOnlyWS * o)
      : SoundslikeElementsBase(o) {}

    u16int next_pos() const
      {return *reinterpret_cast<const u16int *>(cur - 2);}

    SoundslikeWord next(int stopped_at) {
      const char * r = adv(stopped_at);
      if (r) return SoundslikeWord(r,0);
      return SoundslikeWord(tmp,tmp);
    }
  };

  struct ReadOnlyWS::SoundslikeElementsNoSL : public SoundslikeElementsBase
  {
    CharVector buf;
    const Language * lang;

    SoundslikeElementsNoSL(const ReadOnlyWS * o, const Language * l)
      : SoundslikeElementsBase(o), lang(l) {}

    u16int next_pos() const
      {return *reinterpret_cast<const unsigned char *>(cur - 2);}

    SoundslikeWord next(int stopped_at) {
      const char * r = adv(stopped_at);
      if (r) return SoundslikeWord(r,0);
      buf.clear();
      to_stripped(*lang, tmp, buf);
      buf.append('\0');
      return SoundslikeWord(buf.data(), tmp);
    }
  };

  struct ReadOnlyWS::SoundslikeElementsAffix : public SoundslikeElementsBase
  {
    CharVector buf;
    const Language * lang;
    CheckList cl;
    const CheckInfo * ci_cur;

    SoundslikeElementsAffix(const ReadOnlyWS * o, const Language * l)
      : SoundslikeElementsBase(o), lang(l), ci_cur(0) {}

    u16int next_pos() const
      {return *reinterpret_cast<const unsigned char *>(cur - 2);}

    SoundslikeWord next(int stopped_at) {
      if (!ci_cur) {
        const char * r = adv(stopped_at);
        //if (r) CERR << ",," << r << '\n';
        if (r) return SoundslikeWord(r,0);
        BasicWordInfo w = obj->convert(tmp);
        //CERR << ".." << w.word << '/' << w.affixes << '\n';
        cl.reset();
        lang->affix()->expand(w.word, w.affixes, &cl);
        ci_cur = cl.data + 1;
      }

      const CheckInfo * ci_tmp = ci_cur;
      ci_cur = ci_cur->next;

      buf.clear();
      to_stripped(*lang, ci_tmp->word, buf);
      buf.append('\0');
      buf.append(ci_tmp->word, strlen(ci_tmp->word));
      buf.append('\0');
      //CERR << "::" << ci_tmp->word << ' ' << buf.data() << '\n';
      return SoundslikeWord(buf.data(), tmp);
    }
  };

  const char * ReadOnlyWS::SoundslikeElementsBase::adv(int stopped_at) {

    //COUT << level << ":" << stopped_at << "  ";
    //COUT << jump1->sl << " " << jump2->sl << "\n";

    tmp = cur;

    if (level == 1 && stopped_at < 2) {

      ++jump1;
      return jump1->sl;
	  
    } else if (level == 2 && stopped_at < 3) {

      ++jump2;
      if (jump2[-1].sl[1] != jump2[0].sl[1]) {
        ++jump1;
        level = 1;
        return jump1->sl;
      } else {
        return jump2->sl;
      }
      
    } else if (level == 1) {

      level = 2;
      jump2 = obj->jump2 + jump1->loc;
      return jump2->sl;

    } else if (level == 2) {

      cur = tmp = obj->word_block + jump2->loc;
      cur += next_pos(); // next pos uses cur
      level = 3;

    } else if (next_pos() == 0) {

      //cur += 2;
      cur = 0; // REMOVE ME When sure that cur it not used...
      level = 2;
      ++jump2;
      if (jump2[-1].sl[1] != jump2[0].sl[1]) {
        level = 1;
        ++jump1;
        return jump1->sl;
      } else {
        return jump2->sl;
      }

    } else {

      cur += next_pos();
      
    }

    return 0;
  }

    
  ReadOnlyWS::VirSoundslikeEmul * ReadOnlyWS::soundslike_elements() const {

    if (use_soundslike) {
      
      return new SoundslikeElementsWSL(this);

    } else if (affix_info) {

      return new SoundslikeElementsAffix(this, lang());

    } else {

      return new SoundslikeElementsNoSL(this, lang());
      
    }
  }
    
  ReadOnlyWS::VirEmul * 
  ReadOnlyWS::words_w_soundslike(const char * soundslike) const {

    abort();

  }
  
  ReadOnlyWS::VirEmul *
  ReadOnlyWS::words_w_soundslike(SoundslikeWord w) const {

    if (w.word_list_pointer == 0) {

      return new SoundslikeWords("");

    } else if (use_soundslike) {
      
      u16int sl_size = *reinterpret_cast<const u16int *>(w.soundslike-4);
      
      return new SoundslikeWords(w.soundslike + sl_size + 1);

    } else if (affix_info) {

      return new SingleSoundslikeWord(w.soundslike + strlen(w.soundslike) + 1);
      
    } else {
      
      return new SingleSoundslikeWord(static_cast<const char *>
                                      (w.word_list_pointer));
    }

  }

}  

namespace aspeller {

  BasicWordSet * new_default_readonly_word_set() {
    return new aspeller_default_readonly_ws::ReadOnlyWS();
  }
  
}

namespace aspeller_default_readonly_ws {

  using namespace aspeller;

  struct WordData {
    static const int size = 3;
    char compound_info;
    char offset;
    char affix_offset;
    char data[];
    const char * affix_data() const {return data + affix_offset;}
  };
  
  struct WordLookupParms {
    typedef Vector<const WordData *> Vector;
    typedef const WordData *         Value;
    typedef const char *             Key;
    static const bool is_multi = true;
    Key key(Value v) const {return v->data;}
    InsensitiveHash hash_;
    size_t hash(Key k) const {return hash_(k);}
    InsensitiveEqual equal_;
    bool equal(Key rhs, Key lhs) const {
      return equal_(rhs, lhs);
    }
    bool is_nonexistent(const Value & v) const {return v == 0;}
    void make_nonexistent(Value & v) const {v = 0;}
  };

  typedef VectorHashTable<WordLookupParms> WordHash;
  
  struct SoundslikeList {
    struct Word {
      const WordData * d;
      u32int hash_idx;
    };
    union {
      Word * list;
      Word   single;
    } d;
    u16int   size;
    u16int   num_inserted;
  };

  struct SoundslikeLt
  {
    InsensitiveCompare cmp;
    SoundslikeLt(const Language * l = 0)
      : cmp(l) {}
    bool operator()(const char * s1, const char * s2) const {
      if (cmp)
        return cmp(s1, s2) < 0;
      else
        return strcmp(s1, s2) < 0;
    }
  };

  typedef std::multimap<const char *, SoundslikeList, SoundslikeLt> SoundMap;

  static inline unsigned int round_up(unsigned int i, unsigned int size) {
    return ((i + size - 1)/size)*size;
  }

  static void advance_file(FStream & out, int pos) {
    int diff = pos - out.tell();
    assert(diff >= 0);
    for(; diff != 0; --diff)
      out << '\0';
  }

  PosibErr<void> create (ParmString base, 
			 StringEnumeration * els,
			 const Language & lang) 
  {
    assert(sizeof(u16int) == 2);
    assert(sizeof(u32int) == 4);

    bool affix_compress = lang.affix_compress();
    
    bool use_soundslike=true;
    if (strcmp(lang.soundslike_name(),"none") == 0)
      use_soundslike=false;

    assert(!(affix_compress && use_soundslike));
    // FIXME: return error

    const char * mid_chars = lang.mid_chars();

    DataHead data_head;
    memset(&data_head, 0, sizeof(data_head));
    strcpy(data_head.check_word, "aspell default speller rowl 1.6");

    data_head.lang_name_size          = strlen(lang.name()) + 1;
    data_head.soundslike_name_size    = strlen(lang.soundslike_name()) + 1;
    data_head.soundslike_version_size = strlen(lang.soundslike_version()) + 1;
    data_head.middle_chars_size       = strlen(mid_chars) + 1;
    data_head.head_size  = sizeof(DataHead);
    data_head.head_size += data_head.lang_name_size;
    data_head.head_size += data_head.soundslike_name_size;
    data_head.head_size += data_head.soundslike_version_size;
    data_head.head_size  = round_up(data_head.head_size, DataHead::align);

    data_head.minimal_specified = u32int_max;

    data_head.affix_info = affix_compress;

    String temp;

    // these are the final two data structures
    CharVector     data;
    Vector<u32int> final_hash;

    int num_entries = 0;
    
    StringBuffer   buf;
    SoundslikeLt   soundslike_lt;
    if (!use_soundslike)
      soundslike_lt.cmp.lang = &lang;
    SoundMap       sound_map(soundslike_lt);
    Vector<SoundslikeList::Word> sl_list_buf;
    int                          sl_list_buf_size = 0;

    StackPtr<WordHash> word_hash(new WordHash);

    //
    // Read in Wordlist from stdin and create initial Word Hash
    //
    {
      word_hash->parms().hash_ .lang = &lang;
      word_hash->parms().equal_.cmp.lang = &lang;
      const char * w0;
      WordHash::MutableFindIterator j;
      StackPtr<CheckList> cl(new_check_list());
      Vector<WordAff> af_list;

      while ( (w0 = els->next()) != 0) {

	unsigned int s = strlen(w0);
	CharVector tstr;
	tstr.append(w0, s+1);
	char * w = tstr.data();

        char * p0 = strchr(w, '/');
	char * p1 = strchr(w, ':');

        if (p0 && p1 && !(p0 < p1))
          abort(); // FIXME return error

        if (p1) { // compound info, handled first to get size (s) right
          s = p1 - w;
	  *p1 = '\0';
	  ++p1;
	}
	
        if (p0) { // affix info
	  s = p0 - w;
          *p0 = '\0';
          ++p0;
        }

        const char * affixes = p0;

	check_if_valid(lang,w);

        if (affixes && !lang.affix())
          abort(); // FIXME return error

	// Read in compound info
	
	CompoundInfo c;
        if (p1 && *c.read(p1, lang) != '\0')
	  return make_err(invalid_flag, w, p1);
	
        if (!affixes || (affixes && !affix_compress)) {

          // Now expand any affix compression
          
          if (affixes) {
            lang.affix()->expand(w, affixes, cl);
          } else {
            cl->reset();
            CheckInfo * ci = cl->gi.add();
            ci->word = strdup(w);
          }

          // iterate through each expanded word
          
          for (const CheckInfo * ci = check_list_data(cl); ci; ci = ci->next)
          {
            const char * w = ci->word;
            s = strlen(w);
          
            bool dup = false;
            for (j =word_hash->multi_find(w); !j.at_end(); j.adv()) {
              if (strcmp(w, j.deref()->data)==0) {
                dup = true;
              }
            }
            if (dup) continue;
          
            if(s > data_head.max_word_length)
              data_head.max_word_length = s;
            if (c.any() && s < data_head.minimal_specified)
              data_head.minimal_specified = s;
            WordData * b = (WordData *)buf.alloc(s + WordData::size + 2);
            b->compound_info = static_cast<char>(c.d);
            b->offset = s + 2; // this only used when no sl data
            b->affix_offset = s + 1;
            memcpy(b->data, w, s+1);
            b->data[s+1] = '\0'; // needed to keep things simple if sl data not used
            word_hash->insert(b);
            ++num_entries;
          }

        } else {

          // expand any affixes which will effect the first
          // 3 letters of a word.  This is needed so that the
          // jump tables will function correctly

          af_list.resize(strlen(affixes) * strlen(affixes) + 1);
          int n = lang.affix()->expand(w,affixes,3,af_list.data());

          // now interate through the expanded list and insert
          // the words 

          WordAff * p = af_list.data();
          WordAff * end = p + n;
          for (; p != end; ++p)
          {
            //CERR << p->word << '/' << p->af << '\n';
            s = p->word.size();
            const char * w = p->word.c_str();
            bool dup = false;
            for (j =word_hash->multi_find(w); !j.at_end(); j.adv()) {
              if (strcmp(w, j.deref()->data)==0) {
                if (!p->af.empty())
                  CERR << "WARNING: Ignoring duplicate: " << w << '\n';
                dup = true;
                // FiMXE: deal with duplicates properly
              }
            }
            if (dup) continue;

            if(s > data_head.max_word_length)
              data_head.max_word_length = s;
            if (c.any() && s < data_head.minimal_specified)
              data_head.minimal_specified = s;
            
            WordData * b;
            b = (WordData *)buf.alloc(s + WordData::size + p->af.size() + 2);
            b->offset = s + 1 + p->af.size() + 1;
            b->affix_offset = s + 1;

            b->compound_info = static_cast<char>(c.d);
            memcpy(b->data, w, s + 1);
            memcpy(b->data + s + 1, p->af.c_str(), p->af.size() + 1);

            word_hash->insert(b);
            ++num_entries;
          }
        }
      }
      delete els;
    }

    word_hash->resize(word_hash->size()*4/5);

    //
    // 
    //
    if (use_soundslike) {

      for (unsigned int i = 0; i != word_hash->vector().size(); ++i) {
        
        const WordData * value = word_hash->vector()[i];
        
        if (word_hash->parms().is_nonexistent(value)) continue;
        
        temp = lang.to_soundslike(value->data);
        SoundMap::iterator j = sound_map.find(temp.c_str());
        if (j == sound_map.end()) {
          
          SoundMap::value_type
            to_insert(buf.alloc(temp.size()+1), SoundslikeList());
          strncpy(const_cast<char *>(to_insert.first),
                  temp.c_str(),
                  temp.size() + 1);
          sound_map.insert(to_insert)->second.size = 1;
          
        } else {
          
          if (j->second.size == 1)
            sl_list_buf_size++;
          
          j->second.size++;
          sl_list_buf_size++;
	  
        }
        
      }

      sl_list_buf.resize(sl_list_buf_size);
      int p = 0;
      
      for (unsigned int i = 0; i != word_hash->vector().size(); ++i) {
	
	const WordData * value = word_hash->vector()[i];
	
	if (word_hash->parms().is_nonexistent(value)) continue;

        SoundMap::iterator j;
        temp = lang.to_soundslike(value->data);
        j = sound_map.find(temp.c_str());
        assert(j != sound_map.end());
	if (j->second.num_inserted == 0 && j->second.size != 1) {
	  j->second.d.list = &*sl_list_buf.begin() + p;
	  p += j->second.size;
	} 
	if (j->second.size == 1) {
	  j->second.d.single.d        = value;
	  j->second.d.single.hash_idx = i;
	} else {
	  j->second.d.list[j->second.num_inserted].d        = value;
	  j->second.d.list[j->second.num_inserted].hash_idx = i;
	}
	++j->second.num_inserted;
      }
      
    } else {
      
      for (unsigned int i = 0; i != word_hash->vector().size(); ++i) {

        const WordData * value = word_hash->vector()[i];
        
        if (word_hash->parms().is_nonexistent(value)) continue;

        SoundMap::value_type to_insert(value->data, 
                                       SoundslikeList());
        SoundslikeList & sl = sound_map.insert(to_insert)->second;
        sl.size = 1;
        sl.d.single.d        = value;
        sl.d.single.hash_idx = i;
        sl.num_inserted      = 1;
      }
    }

    data_head.word_count   = word_hash->size();
    data_head.word_buckets = word_hash->bucket_count();

    final_hash.insert(final_hash.begin(), 
		      word_hash->bucket_count(), u32int_max);
    
    word_hash.del(); // the word_hash is no longer needed so free the space
    
    //
    // Create the data block which includes both the word and soundlike
    //  data
    //

    Vector<SoundslikeJump> jump1;
    Vector<SoundslikeJump> jump2;
    {
      SoundMap::iterator i   = sound_map.begin();
      SoundMap::iterator end = sound_map.end();

      data.write16(0); // to avoid nasty special cases
      data.write16(0);
      unsigned int prev_pos = data.size();
      data.write32(0);
      String sl;
      String prev_sl = "";

      for (; i != end; ++i) {

	if (use_soundslike) {

	  sl = i->first;
          while (data.size() % 2 != 0)
            data.write('\0');
	  data.write16(sl.size());
          data.write16(0); // place holder for offset to next item

	} else {

	  sl.clear();
	  to_stripped(lang, i->first, sl);
          data.write('\0');
          data.write('\0');

	}

	if (strncmp(prev_sl.c_str(), sl.c_str(), 3) != 0) {

	  SoundslikeJump jump;
	  strncpy(jump.sl, sl.c_str(), 3);
	  jump.loc = data.size();
	  jump2.push_back(jump);

	  if (strncmp(prev_sl.c_str(), sl.c_str(), 2) != 0) {
	    SoundslikeJump jump;
	    strncpy(jump.sl, sl.c_str(), 2);
	    jump.loc = jump2.size() - 1;
	    jump1.push_back(jump);
	  }

          if (use_soundslike)
            data.at16(prev_pos - 2) = 0;
          else
            data[prev_pos - 2] = 0;

	} else {
	  
          if (use_soundslike)
            data.at16(prev_pos - 2) = data.size() - prev_pos;
          else
            data[prev_pos - 2] = (unsigned char)(data.size() - prev_pos);

	}

	prev_pos = data.size();

	// Write soundslike
	if (use_soundslike)
	  data << sl << '\0';

	// Write words
	{
	  SoundslikeList::Word * k 
	    = i->second.size == 1 ? &(i->second.d.single) : i->second.d.list;
	  SoundslikeList::Word * end = k + i->second.size;
          assert(use_soundslike || i->second.size == 1);
	  for (;k != end; ++k) {
	    //if (k->d->compound_info)
	    //  data << k->d->compound_info;

            //CERR << (int)k->d->affix_offset << '\n';
	    final_hash[k->hash_idx] = data.size();

            if (!use_soundslike) {
              //data[prev_pos - 2] = k->d->offset;
              data[prev_pos - 1] = k->d->affix_offset;
            }

            data << k->d->data << '\0';

            if (!use_soundslike)
              data << k->d->affix_data() << '\0';
	  }
          if (use_soundslike)
            data << '\0';
	}

	prev_sl = sl;
      }

      // add special end case
      data.write16(0);
      data.write16(0);
      if (use_soundslike)
        data.at16(prev_pos - 2) |= data.size() - prev_pos;
      else
        data[prev_pos - 2] |= (unsigned char)(data.size() - prev_pos);
      data.at32(4) = 0;
      jump2.push_back(SoundslikeJump());
      jump1.push_back(SoundslikeJump());
    }

    data_head.soundslike_count   = sound_map.size();

    FStream out;
    out.open(base, "wb");

    // Write jump1 table
    advance_file(out, data_head.head_size);
    data_head.jump1_offset = out.tell() - data_head.head_size;
    out.write(jump1.data(), jump1.size() * sizeof(SoundslikeJump));

    // Write jump2 table
    advance_file(out, round_up(out.tell(), DataHead::align));
    data_head.jump2_offset = out.tell() - data_head.head_size;
    out.write(jump2.data(), jump2.size() * sizeof(SoundslikeJump));

    // Write data block
    advance_file(out, round_up(out.tell(), DataHead::align));
    data_head.word_offset = out.tell() - data_head.head_size;
    out.write(data.data(), data.size());

    // Write hash
    advance_file(out, round_up(out.tell(), DataHead::align));
    data_head.hash_offset = out.tell() - data_head.head_size;
    out.write(&final_hash.front(), final_hash.size() * 4);
    
    advance_file(out, round_up(out.tell(), DataHead::align));
    data_head.block_size = out.tell() - data_head.head_size;

    // write data head to file
    out.seek(0);
    out.write(&data_head, sizeof(DataHead));
    out.write(lang.name(), data_head.lang_name_size);
    out.write(lang.soundslike_name(), data_head.soundslike_name_size);
    out.write(lang.soundslike_version(), data_head.soundslike_version_size);
    out.write(mid_chars, data_head.middle_chars_size); 

    return no_err;
  }

}

namespace aspeller {
  PosibErr<void> create_default_readonly_word_set(StringEnumeration * els,
                                                  Config & config)
  {
    CachePtr<Language> lang;
    PosibErr<Language *> res = new_language(config);
    if (!res) return res;
    lang.reset(res.data);
    aspeller_default_readonly_ws::create(config.retrieve("master-path"),
                                         els,*lang);
    return no_err;
  }
}
