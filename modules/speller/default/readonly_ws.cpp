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
// WITHOUT Soundslike data
//   (<8 bit: offset><8 bit: sl offset><word><null>[<affix info>]<null>)+
// compound info is currently not used (ie broken)

// TODO:
//   consider adding flags to speed up checking
//     1 bit: all lower case
//     1 bit: no accent info
//   which leaves 6 bits for other info which could possible be used
//   to flag special words.   Such as those which are abbreviations
//   or prefixes and can only appear before a word if there is a
//   '-'.

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
    
    ReadOnlyWS(const ReadOnlyWS&);
    ReadOnlyWS& operator= (const ReadOnlyWS&);

    struct ElementsParms;
    struct SoundslikeElements;
    struct StrippedElements;

  public:
    VirEnum * detailed_elements() const;
    Size      size()     const;
    bool      empty()    const;

    static const char * get_affix(const char * w) {return w+(unsigned char)*(w-1);}
    
    void convert(const char * w, WordEntry & o) const {
      o.what = WordEntry::Word;
      o.word = w;
      o.aff  = !have_soundslike ? get_affix(w) : w - 1; // w - 1 is NULL
    }
    
    ReadOnlyWS() {
      block = 0;
    }

    ~ReadOnlyWS() {
      if (block != 0) {
	if (block_mmaped)
	  mmap_free(block, block_size);
	else
	  free(block);
      }
    }
      
    PosibErr<void> load(ParmString, Config *, SpellerImpl *, const LocalWordSetInfo *);

    bool lookup(ParmString word, WordEntry &, const SensitiveCompare &) const;

    bool striped_lookup(const char * sondslike, WordEntry &) const;

    bool soundslike_lookup(const WordEntry &, WordEntry &) const;
    bool soundslike_lookup(const char * sondslike, WordEntry &) const;
    
    SoundslikeEnumeration * soundslike_elements() const;
  };
    
  //
  //  
  //

  struct ReadOnlyWS::ElementsParms {
    typedef WordEntry *                Value;
    typedef WordLookup::const_iterator Iterator; 
    const ReadOnlyWS * ws;
    WordEntry data;
    ElementsParms(const ReadOnlyWS * w) : ws(w) {}
    bool endf(const Iterator & i) const {return i.at_end();}
    Value end_state() const {return 0;}
    WordEntry * deref(const Iterator & i) {
      ws->convert(ws->word_block + *i, data);
      return & data;
    }
  };

  ReadOnlyWS::VirEnum * ReadOnlyWS::detailed_elements() const {
    return new MakeVirEnumeration<ElementsParms>
      (word_lookup.begin(), ElementsParms(this));
  }

  ReadOnlyWS::Size ReadOnlyWS::size() const {
    return word_lookup.size();
  }
  
  bool ReadOnlyWS::empty() const {
    return word_lookup.empty();
  }

  static const char * const cur_check_word = "aspell default speller rowl 1.7";

  struct DataHead {
    // all sizes except the last four must to divisible by:
    static const uint align = 16;
    char check_word[64];
    u32int endian_check; // = 12345678

    u32int head_size;
    u32int block_size;
    u32int jump1_offset;
    u32int jump2_offset;
    u32int word_offset;
    u32int hash_offset;

    u32int word_count;
    u32int word_buckets;
    u32int soundslike_count;

    u32int dict_name_size;
    u32int lang_name_size;
    u32int soundslike_name_size;
    u32int soundslike_version_size;

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

    if (strcmp(data_head.check_word, cur_check_word) != 0)
      return make_err(bad_file_format, fn);

    if (data_head.endian_check != 12345678)
      return make_err(bad_file_format, fn, _("Wrong endian order."));

    CharVector word;

    word.resize(data_head.dict_name_size);
    f.read(word.data(), data_head.dict_name_size);

    word.resize(data_head.lang_name_size);
    f.read(word.data(), data_head.lang_name_size);

    PosibErr<void> pe = set_check_lang(word.data(),config);
    if (pe.has_err())
      return pe.with_file(fn);

    if (data_head.soundslike_name_size != 0) {
      have_soundslike = true;

      word.resize(data_head.soundslike_name_size);
      f.read(word.data(), data_head.soundslike_name_size);

      if (strcmp(word.data(), lang()->soundslike_name()) != 0)
        return make_err(bad_file_format, fn, _("Wrong soundslike"));

      word.resize(data_head.soundslike_version_size);
      f.read(word.data(), data_head.soundslike_version_size);

      if (strcmp(word.data(), lang()->soundslike_version()) != 0)
        return make_err(bad_file_format, fn, _("Wrong soundslike version"));
    }

    affix_compressed = data_head.affix_info;

    block_size = data_head.block_size;
    block = mmap_open(block_size, f, data_head.head_size);
    block_mmaped = block != (char *)MAP_FAILED;
    if (!block_mmaped) {
      block = (char *)malloc(block_size);
      f.seek(data_head.head_size);
      f.read(block, block_size);
    }

    if (data_head.jump2_offset) {
      fast_scan = true;
      jump1 = reinterpret_cast<const SoundslikeJump *>(block 
						       + data_head.jump1_offset);
      jump2 = reinterpret_cast<const SoundslikeJump *>(block 
						       + data_head.jump2_offset);
    } else {
      jump1 = jump2 = 0;
    }

    word_block       = block + data_head.word_offset;

    word_lookup.parms().block_begin = word_block;
    word_lookup.parms().hash .lang     = lang();
    word_lookup.parms().equal.cmp.lang = lang();
    const u32int * begin = reinterpret_cast<const u32int *>
      (block + data_head.hash_offset);
    word_lookup.vector().set(begin, begin + data_head.word_buckets);
    word_lookup.set_size(data_head.word_count);
    
    return no_err;
  }

  bool ReadOnlyWS::lookup(ParmString word, WordEntry & o,
                          const SensitiveCompare & c) const 
  {
    o.clear();
    o.what = WordEntry::Word;
    WordLookup::ConstFindIterator i = word_lookup.multi_find(word);
    for (; !i.at_end(); i.adv()) {
      const char * w = word_block + i.deref();
      if (c(word, w)) {
        convert(w,o);
        return true;
      }
    }
    return false;
  }

  struct ReadOnlyWS::SoundslikeElements : public SoundslikeEnumeration
  {
    WordEntry data;
    const ReadOnlyWS * obj;
    const SoundslikeJump * jump1;
    const SoundslikeJump * jump2;
    const char * cur;
    int level;
    bool sl;

    u16int next_pos() const {
      if (sl) return *reinterpret_cast<const u16int *>(cur - 2);
      else return *reinterpret_cast<const unsigned char *>(cur - 2);
    }

    WordEntry * next(int stopped_at);

    SoundslikeElements(const ReadOnlyWS * o)
      : obj(o), jump1(obj->jump1), jump2(obj->jump2), cur(0), 
        level(1), sl(o->have_soundslike) {
      data.what = o->have_soundslike ? WordEntry::Soundslike : WordEntry::Word;}
  };
  
  WordEntry * ReadOnlyWS::SoundslikeElements::next(int stopped_at) {

    //CERR << level << ":" << stopped_at << "  :";
    //CERR << jump1->sl << ":" << jump2->sl << "\n";

    const char * tmp = cur;

    if (level == 1 && stopped_at < 2) {

      ++jump1;
      tmp = jump1->sl;
      goto jquit;
	  
    } else if (level == 2 && stopped_at < 3) {

      ++jump2;
      if (jump2[-1].sl[1] != jump2[0].sl[1]) {
        ++jump1;
        level = 1;
        tmp = jump1->sl;
      } else {
        tmp = jump2->sl;
      }
      goto jquit;
      
    } else if (level == 1) {

      level = 2;
      jump2 = obj->jump2 + jump1->loc;
      tmp = jump2->sl;
      goto jquit;

    } else if (level == 2) {

      cur = tmp = obj->word_block + jump2->loc;
      cur += next_pos(); // next pos uses cur
      level = 3;

    } else if (next_pos() == 0) {

      level = 2;
      ++jump2;
      if (jump2[-1].sl[1] != jump2[0].sl[1]) {
        level = 1;
        ++jump1;
        tmp = jump1->sl;
      } else {
        tmp = jump2->sl;
      }
      goto jquit;

    } else {

      cur += next_pos();
      
    }

    data.word = tmp;
    if (!sl) {
      data.what = WordEntry::Word;
      data.aff  = obj->get_affix(tmp);
    }
    data.intr[0] = (void *)tmp;
    return &data;

  jquit:
    if (!*tmp) return 0;
    data.word = tmp;
    data.intr[0] = 0;
    if (!sl) {
      data.what = WordEntry::Stripped;
      data.aff  = 0;
    }
    return &data;
  }

  struct ReadOnlyWS::StrippedElements : public SoundslikeEnumeration
  {
    WordEntry data;
    const char * cur;

    u16int next_pos() const {
      return *reinterpret_cast<const unsigned char *>(cur - 2);
    }

    WordEntry * next(int stopped_at);

    StrippedElements(const ReadOnlyWS * o)
      : cur(o->word_block + 2) {data.what = WordEntry::Word;}
  };

  WordEntry * ReadOnlyWS::StrippedElements::next(int) {

    const char * tmp = cur;
    cur += next_pos();
    if (cur == tmp) return 0;
    data.intr[0] = (void *)tmp;
    data.word = tmp;
    data.aff  = ReadOnlyWS::get_affix(tmp);
    return &data;

  }

  SoundslikeEnumeration * ReadOnlyWS::soundslike_elements() const {

    if (jump1)
      return new SoundslikeElements(this);
    else
      return new StrippedElements(this);

  }
    
  static void soundslike_next(WordEntry * w)
  {
    const char * cur = static_cast<const char *>(w->intr[0]);
    w->word = cur;
    unsigned int len = strlen(cur);
    cur += len + 1;
    w->intr[0] = (void *)cur;
    if (*cur == 0) w->adv_ = 0;
  }

  //static 
  //void ReadOnlyWS::stripped_next(WordEntry * w) {}

  bool ReadOnlyWS::striped_lookup(const char * sl, WordEntry & w) const
  {
    w.clear();
    WordLookup::ConstFindIterator i = word_lookup.multi_find(sl);
    if (i.at_end()) return false;
    convert(word_block + i.deref(), w);
    return true;
    // FIXME deal with multiple words 
    //   the lookup should point to the first one of the kind ....
  }
    
  bool ReadOnlyWS::soundslike_lookup(const char * sl, WordEntry & w) const 
  {
    if (!have_soundslike) {
      return ReadOnlyWS::striped_lookup(sl,w);
    } else {
      return false;
    }
  }

  bool ReadOnlyWS::soundslike_lookup(const WordEntry & s, WordEntry & w) const 
  {
    w.clear();

    if (s.intr[0] == 0) {

      return false;

    } else if (have_soundslike) {
      
      w.what = WordEntry::Word;
      u16int sl_size = *reinterpret_cast<const u16int *>(s.word-4);
      w.intr[0] = (void *)(s.word + sl_size + 1);
      w.adv_ = soundslike_next;
      soundslike_next(&w);
      return true;
      
    } else {

      w.what = WordEntry::Word;
      convert(s.word, w);
      return true;

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
    char total_size; // including null chars
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

  PosibErr<void> create (StringEnumeration * els,
			 const Language & lang,
                         Config & config) 
  {
    assert(sizeof(u16int) == 2);
    assert(sizeof(u32int) == 4);

    bool affix_compress = (lang.affix() && 
                           config.retrieve_bool("affix-compress"));
    
    bool use_soundslike = (lang.have_soundslike() &&
                           config.retrieve_bool("use-soundslike"));
   
    bool use_jump_tables = use_soundslike || config.retrieve_bool("use-jump-tables");

    CERR << (affix_compress ? "  AFFIX COMPRESS" : "")
         << (use_soundslike ? "  USING SOUNDSLIKE" : "") 
         << (use_jump_tables ? "  USING JUMP TABLES" : "")
         << "\n";

    assert(!(affix_compress && use_soundslike)); // FIXME: return error

    String base = config.retrieve("master-path");

    DataHead data_head;
    memset(&data_head, 0, sizeof(data_head));
    strcpy(data_head.check_word, cur_check_word);

    data_head.endian_check = 12345678;

    data_head.dict_name_size = 1;
    data_head.lang_name_size = strlen(lang.name()) + 1;
    if (use_soundslike) {
      data_head.soundslike_name_size    = strlen(lang.soundslike_name()) + 1;
      data_head.soundslike_version_size = strlen(lang.soundslike_version()) + 1;
    }
    data_head.head_size  = sizeof(DataHead);
    data_head.head_size += data_head.dict_name_size;
    data_head.head_size += data_head.lang_name_size;
    data_head.head_size += data_head.soundslike_name_size;
    data_head.head_size += data_head.soundslike_version_size;
    data_head.head_size  = round_up(data_head.head_size, DataHead::align);

    data_head.affix_info = affix_compress;

    String temp;

    // these are the final data structures
    CharVector     data;
    Vector<u32int> final_hash;
    Vector<SoundslikeJump> jump1;
    Vector<SoundslikeJump> jump2;

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

        if (p0) { // affix info
	  s = p0 - w;
          *p0 = '\0';
          ++p0;
        }

        const char * affixes = p0;

	check_if_valid(lang,w);

        if (affixes && !lang.affix())
          abort(); // FIXME return error

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
          
            WordData * b = (WordData *)buf.alloc(s + WordData::size + 2);
            b->total_size = s + 2;
            b->affix_offset = s + 1;
            memcpy(b->data, w, s+1);
            b->data[s+1] = '\0'; // needed to keep things simple if sl data not used
            word_hash->insert(b);
            ++num_entries;
          }

        } else {

          int n;
          WordAff * p, * end;
          WordAff wa_buf;

          if (use_jump_tables) {
            // expand any affixes which will effect the first
            // 3 letters of a word.  This is needed so that the
            // jump tables will function correctly
            af_list.resize(strlen(affixes) * strlen(affixes) + 1);
            n = lang.affix()->expand(w,affixes,3,af_list.data());
            p = af_list.data();
            end = p + n;
          } else {
            wa_buf.word = w;
            wa_buf.af   = affixes;
            p = & wa_buf;
            end = p + 1;
          }

          // now interate through the list and insert
          // the words 
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
                // FIXME: deal with duplicates properly
              }
            }
            if (dup) continue;

            WordData * b;
            b = (WordData *)buf.alloc(s + WordData::size + p->af.size() + 2);
            b->total_size = s + 1 + p->af.size() + 1;
            b->affix_offset = s + 1;

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

      data_head.soundslike_count = sound_map.size();
      
    } else if (use_jump_tables) {
      
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
    
    //
    // Create the data block which includes both the word and soundlike
    //  data
    //

    if (use_jump_tables) {

      word_hash.del(); // the word_hash is no longer needed so free the space

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
          if (data.size() % 2 != 0) data.write('\0');
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
	    final_hash[k->hash_idx] = data.size();

            if (!use_soundslike) {
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
      if (data.size() % 2 != 0) data.write('\0');
      data.write16(0);
      data.write16(0);
      if (use_soundslike)
        data.at16(prev_pos - 2) |= data.size() - prev_pos;
      else
        data[prev_pos - 2] |= (unsigned char)(data.size() - prev_pos);
      data.at32(4) = 0;
      jump2.push_back(SoundslikeJump());
      jump1.push_back(SoundslikeJump());

    } else {

      // No jump tables, just write the words in the order that
      // they appear in the hash table.

      for (unsigned int i = 0; i != word_hash->vector().size(); ++i) {

        const WordData * value = word_hash->vector()[i];
        
        if (word_hash->parms().is_nonexistent(value)) continue;

        data.write(value->total_size + 2);
        data.write(value->affix_offset);

        final_hash[i] = data.size();

        data.write(value->data, value->total_size);
        
      }

      data.write(0);
      data.write(0);
      data.write(0);

    }

    FStream out;
    out.open(base, "wb");

    advance_file(out, data_head.head_size);

    if (use_jump_tables) {
      // Write jump1 table
      data_head.jump1_offset = out.tell() - data_head.head_size;
      out.write(jump1.data(), jump1.size() * sizeof(SoundslikeJump));
      
      // Write jump2 table
      advance_file(out, round_up(out.tell(), DataHead::align));
      data_head.jump2_offset = out.tell() - data_head.head_size;
      out.write(jump2.data(), jump2.size() * sizeof(SoundslikeJump));
    } 

    // Write data block
    advance_file(out, round_up(out.tell(), DataHead::align));
    data_head.word_offset = out.tell() - data_head.head_size;
    out.write(data.data(), data.size());

    // Write hash
    advance_file(out, round_up(out.tell(), DataHead::align));
    data_head.hash_offset = out.tell() - data_head.head_size;
    out.write(&final_hash.front(), final_hash.size() * 4);
    
    // calculate block size
    advance_file(out, round_up(out.tell(), DataHead::align));
    data_head.block_size = out.tell() - data_head.head_size;

    // write data head to file
    out.seek(0);
    out.write(&data_head, sizeof(DataHead));
    out.write(" ", 1);
    out.write(lang.name(), data_head.lang_name_size);
    if (use_soundslike) {
      out.write(lang.soundslike_name(), data_head.soundslike_name_size);
      out.write(lang.soundslike_version(), data_head.soundslike_version_size);
    }

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
    lang->set_lang_defaults(config);
    aspeller_default_readonly_ws::create(els,*lang,config);
    return no_err;
  }
}

