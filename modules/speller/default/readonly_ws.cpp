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
//
// The lower 4 bits are used for word info.  The upper 4 bits can be used to store 
//   info on additional info.
// * have affix flags
// * have compound category flags
// Words:
//   (<8 bit frequency><8 bit: flags><8 bit: offset>
//      <8 bit: word size><word><null>
//      [<affix info><null>][<category info><null>])+
// Words with soundslike:
//   (<8 bit: offset to next item><8 bit: soundslike size><soundslike>
//      <words>)+
// An extra <null> is inserted at the end of each "group".
//

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
#include "objstack.hpp"
#include "vector.hpp"
#include "vector_hash-t.hpp"
#include "check_list.hpp"
#include "lsort.hpp"
#include "iostream.hpp"

typedef unsigned int   u32int;
static const u32int u32int_max = (u32int)-1;
typedef unsigned short u16int;
typedef unsigned char byte;

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

static byte HAVE_AFFIX_FLAGS = 1 << 7;
static byte HAVE_CATEGORY_FLAGS = 1 << 6;

static byte WORD_INFO_MASK = 0x0F;

static const int FREQUENCY_INFO_O = 4;
static const int FLAGS_O = 3;
static const int NEXT_O = 2;
static const int WORD_SIZE_O = 1;

static inline int get_word_size(const char * d) {
  return *reinterpret_cast<const byte *>(d - WORD_SIZE_O);
}

static inline byte get_flags(const char * d) {
  return *reinterpret_cast<const byte *>(d - FLAGS_O);
}

static inline byte get_offset(const char * d) {
  return *reinterpret_cast<const byte *>(d - NEXT_O);
}

static inline const char * get_next(const char * d) {
  return d + *reinterpret_cast<const byte *>(d - NEXT_O);
}

static inline const char * get_sl_words_begin(const char * d) {
  return d + *reinterpret_cast<const byte *>(d - WORD_SIZE_O) + 4;
  // FIXME: This isn't right when frequency info is stored in the table
}

// get_next might go past the end so don't JUST compare
// for equality.  Ie use while (cur < end) not (cur != end)
static inline const char * get_sl_words_end(const char * d) {
  return get_next(d) - 3;
}

static inline const char * get_affix(const char * d) {
  int word_size = get_word_size(d);
  if (get_flags(d) & HAVE_AFFIX_FLAGS) 
    return d + word_size +  1;
  else
    return d + word_size;
}

static inline const char * get_category(const char * d) {
  int word_size = get_word_size(d);
  if (get_flags(d) & HAVE_AFFIX_FLAGS & HAVE_CATEGORY_FLAGS) 
    return d + strlen(d + word_size +  1) + 1;
  else if (get_flags(d) & HAVE_CATEGORY_FLAGS)
    return d + word_size +  1;
  else
    return d + word_size;
}

namespace {

  using namespace aspeller;

  /////////////////////////////////////////////////////////////////////
  // 
  //  ReadOnlyDict
  //
    
  struct Jump
  {
    char   sl[4];
    u32int loc;
    Jump() : sl(), loc() {}
  };
  
  class ReadOnlyDict : public Dictionary
  {
      
  public: //but don't use

    struct WordLookupParms {
      const char * block_begin;
      WordLookupParms() {}
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
    const Jump * jump1;
    const Jump * jump2;
    WordLookup       word_lookup;
    const char *     word_block;
    
    ReadOnlyDict(const ReadOnlyDict&);
    ReadOnlyDict& operator= (const ReadOnlyDict&);

    struct ElementsParms;
    struct SoundslikeElements;
    struct CleanElements;

  public:
    WordEntryEnumeration * detailed_elements() const;
    Size      size()     const;
    bool      empty()    const;

    ReadOnlyDict() 
      : Dictionary(basic_dict, "ReadOnlyDict")
    {
      block = 0;
    }

    ~ReadOnlyDict() {
      if (block != 0) {
	if (block_mmaped)
	  mmap_free(block, block_size);
	else
	  free(block);
      }
    }
    
    PosibErr<void> load(ParmString, Config &, LocalDictList *, 
                        SpellerImpl *, const LocalDictInfo *);

    bool lookup(ParmString word, WordEntry &, const SensitiveCompare &) const;

    bool clean_lookup(ParmString, WordEntry &) const;

    bool soundslike_lookup(const WordEntry &, WordEntry &) const;
    bool soundslike_lookup(ParmString, WordEntry &) const;
    
    SoundslikeEnumeration * soundslike_elements() const;
  };

  static inline void convert(const char * w, WordEntry & o) {
    o.what = WordEntry::Word;
    o.word = w;
    o.aff  = get_affix(w);
    o.word_size = get_word_size(w);
    o.word_info = get_flags(w) & WORD_INFO_MASK;
  }
    
  //
  //  
  //

  struct ReadOnlyDict::ElementsParms {
    typedef WordEntry *                Value;
    typedef WordLookup::const_iterator Iterator; 
    const ReadOnlyDict * ws;
    WordEntry data;
    ElementsParms(const ReadOnlyDict * w) : ws(w) {}
    bool endf(const Iterator & i) const {return i.at_end();}
    Value end_state() const {return 0;}
    WordEntry * deref(const Iterator & i) {
      convert(ws->word_block + *i, data);
      return & data;
    }
  };

  WordEntryEnumeration * ReadOnlyDict::detailed_elements() const {
    return new MakeEnumeration<ElementsParms>(word_lookup.begin(), ElementsParms(this));
  }

  ReadOnlyDict::Size ReadOnlyDict::size() const {
    return word_lookup.size();
  }
  
  bool ReadOnlyDict::empty() const {
    return word_lookup.empty();
  }

  static const char * const cur_check_word = "aspell default speller rowl 1.9";

  struct DataHead {
    // all sizes except the last four must to divisible by:
    static const uint align = 16;
    char check_word[64];
    u32int endian_check; // = 12345678
    char lang_hash[16];

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

    byte affix_info; // 0 = none, 1 = partially expanded, 2 = full
    byte invisible_soundslike;
    byte soundslike_root_only;
    byte compound_info; //
    byte freq_info;
  };

  PosibErr<void> ReadOnlyDict::load(ParmString f0, Config & config, 
                                    LocalDictList *, 
                                    SpellerImpl *, const LocalDictInfo *)
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
    if (pe.has_err()) {
      if (pe.prvw_err()->is_a(language_related_error))
        return pe.with_file(fn);
      else
        return pe;
    }

    if (data_head.soundslike_name_size != 0) {
      word.resize(data_head.soundslike_name_size);
      f.read(word.data(), data_head.soundslike_name_size);

      if (strcmp(word.data(), lang()->soundslike_name()) != 0)
        return make_err(bad_file_format, fn, _("Wrong soundslike."));

      word.resize(data_head.soundslike_version_size);
      f.read(word.data(), data_head.soundslike_version_size);

      if (strcmp(word.data(), lang()->soundslike_version()) != 0)
        return make_err(bad_file_format, fn, _("Wrong soundslike version."));
    }

    invisible_soundslike = data_head.invisible_soundslike;
    soundslike_root_only = data_head.soundslike_root_only;

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
      jump1 = reinterpret_cast<const Jump *>(block + data_head.jump1_offset);
      jump2 = reinterpret_cast<const Jump *>(block + data_head.jump2_offset);
    } else {
      jump1 = jump2 = 0;
    }

    word_block = block + data_head.word_offset;

    word_lookup.parms().block_begin = word_block;
    word_lookup.parms().hash .lang     = lang();
    word_lookup.parms().equal.cmp.lang = lang();
    const u32int * begin = reinterpret_cast<const u32int *>
      (block + data_head.hash_offset);
    word_lookup.vector().set(begin, begin + data_head.word_buckets);
    word_lookup.set_size(data_head.word_count);
    
    return no_err;
  }

  bool ReadOnlyDict::lookup(ParmString word, WordEntry & o,
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

  struct ReadOnlyDict::SoundslikeElements : public SoundslikeEnumeration
  {
    WordEntry data;
    const ReadOnlyDict * obj;
    const Jump * jump1;
    const Jump * jump2;
    const char * cur;
    int level;
    bool invisible_soundslike;

    WordEntry * next(int stopped_at);

    SoundslikeElements(const ReadOnlyDict * o)
      : obj(o), jump1(obj->jump1), jump2(obj->jump2), cur(0), 
        level(1), invisible_soundslike(o->invisible_soundslike) {
      data.what = o->invisible_soundslike ? WordEntry::Word : WordEntry::Soundslike;}
  };

  WordEntry * ReadOnlyDict::SoundslikeElements::next(int stopped_at) {

    //CERR << level << ":" << stopped_at << "  :";
    //CERR << jump1->data << ":" << jump2->sl << "\n";

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
      level = 3;

    } else if (get_offset(cur) == 0) {

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

    } 

    cur = get_next(cur); // this will be the NEXT item looked at

    data.word = tmp;
    data.word_size = get_word_size(tmp);
    if (invisible_soundslike) {
      convert(tmp, data);
      data.what = WordEntry::Word;
    } 
    data.intr[0] = (void *)tmp;
    return &data;

  jquit:
    if (!*tmp) return 0;
    data.word = tmp;
    data.word_size = !tmp[1] ? 1 : !tmp[2] ? 2 : 3;
    data.intr[0] = 0;
    if (invisible_soundslike) {
      data.what = WordEntry::Clean;
      data.aff  = 0;
    }
    return &data;
  }

  struct ReadOnlyDict::CleanElements : public SoundslikeEnumeration
  {
    WordEntry data;
    const char * cur;

    WordEntry * next(int stopped_at);

    CleanElements(const ReadOnlyDict * o)
      : cur(o->word_block + 3) {data.what = WordEntry::Word;}
  };

  WordEntry * ReadOnlyDict::CleanElements::next(int) {

    const char * tmp = cur;
    cur = get_next(cur);
    if (cur == tmp) return 0;
    data.intr[0] = (void *)tmp;
    convert(tmp, data);
    return &data;

  }

  SoundslikeEnumeration * ReadOnlyDict::soundslike_elements() const {

    if (jump1)
      return new SoundslikeElements(this);
    else
      return new CleanElements(this);

  }
    
  static void soundslike_next(WordEntry * w)
  {
    const char * cur = (const char *)(w->intr[0]);
    const char * end = (const char *)(w->intr[1]);
    convert(cur, *w);
    cur = get_next(cur);
    w->intr[0] = (void *)cur;
    if (cur >= end) w->adv_ = 0;
  }

  //static 
  //void ReadOnlyDict::clean_next(WordEntry * w) {}

  bool ReadOnlyDict::clean_lookup(ParmString sl, WordEntry & w) const
  {
    w.clear();
    WordLookup::ConstFindIterator i = word_lookup.multi_find(sl);
    if (i.at_end()) return false;
    convert(word_block + i.deref(), w);
    return true;
    // FIXME deal with multiple words 
    //   the lookup should point to the first one of the kind ....
  }
    
  bool ReadOnlyDict::soundslike_lookup(const WordEntry & s, WordEntry & w) const 
  {
    if (s.intr[0] == 0) {

      return false;

    } else if (!invisible_soundslike) {
      
      w.clear();
      w.what = WordEntry::Word;
      w.intr[0] = (void *)get_sl_words_begin(s.word);
      w.intr[1] = (void *)get_sl_words_end(s.word);
      w.adv_ = soundslike_next;
      soundslike_next(&w);
      return true;
      
    } else {

      w.clear();
      w.what = WordEntry::Word;
      convert(s.word, w);
      return true;

    }
  }

  bool ReadOnlyDict::soundslike_lookup(ParmString s, WordEntry & w) const 
  {
    if (invisible_soundslike) {
      return ReadOnlyDict::clean_lookup(s,w);
    } else {
      return false;
    }
  }

}  

namespace aspeller {

  Dictionary * new_default_readonly_dict() {
    return new ReadOnlyDict();
  }
  
}

namespace {

  // Possible:
  //   No Affix Compression:
  //     no soundslike
  //     invisible soundslike
  //     with soundslike
  //   Affix Compression:
  //     group by root:
  //       no soundslike
  //       invisible soundslike
  //       with soundslike
  //     expand prefix:  
  //       no soundslike
  //       invisible soundslike

  using namespace aspeller;

  struct WordData {
    WordData * next;
    char * sl;
    char * aff;
    byte word_size;
    byte sl_size;
    byte data_size;
    byte flags;
    char word[];
  };

  struct SoundslikeLess {
    bool operator() (WordData * x, WordData * y) const {
      int res = strcmp(x->sl, y->sl);
      if (res != 0) return res < 0;
      return strcmp(x->word, y->word) < 0;
    }
  };

  struct WordLookupParms {
    const char * block_begin;
    WordLookupParms() {}
    typedef Vector<u32int>      Vector;
    typedef u32int              Value;
    typedef const char *        Key;
    static const bool is_multi = true;
    Key key(Value v) const {assert (v != u32int_max);
                            return block_begin + v;}
    InsensitiveHash  hash;
    InsensitiveEqual equal;
    bool is_nonexistent(Value v) const {return v == u32int_max;}
    void make_nonexistent(Value & v) const {v = u32int_max;}
  };
  typedef VectorHashTable<WordLookupParms> WordLookup;

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

    bool full_soundslike = (strcmp(lang.soundslike_name(), "none") != 0 && 
                            strcmp(lang.soundslike_name(), "simple"));

    bool affix_compress = (lang.affix() && 
                           config.retrieve_bool("affix-compress"));

    bool partially_expand = (affix_compress &&
                             !full_soundslike &&
                             config.retrieve_bool("partially-expand"));

    bool invisible_soundslike = false;
    if (partially_expand)
      invisible_soundslike = true;
    else if (config.have("invisible-soundslike"))
      invisible_soundslike = config.retrieve_bool("invisible-soundslike");
    else if (!full_soundslike)
      invisible_soundslike = true;


    ConvEC iconv;
    if (!config.have("norm-strict"))
      config.replace("norm-strict", "true");
    if (config.have("encoding"))
      RET_ON_ERR(iconv.setup(config, config.retrieve("encoding"), lang.charmap(),NormFrom));
    else
      RET_ON_ERR(iconv.setup(config, lang.data_encoding(), lang.charmap(), NormFrom));

    String base = config.retrieve("master-path");

    DataHead data_head;
    memset(&data_head, 0, sizeof(data_head));
    strcpy(data_head.check_word, cur_check_word);

    data_head.endian_check = 12345678;

    data_head.dict_name_size = 1;
    data_head.lang_name_size = strlen(lang.name()) + 1;
    data_head.soundslike_name_size    = strlen(lang.soundslike_name()) + 1;
    data_head.soundslike_version_size = strlen(lang.soundslike_version()) + 1;
    data_head.head_size  = sizeof(DataHead);
    data_head.head_size += data_head.dict_name_size;
    data_head.head_size += data_head.lang_name_size;
    data_head.head_size += data_head.soundslike_name_size;
    data_head.head_size += data_head.soundslike_version_size;
    data_head.head_size  = round_up(data_head.head_size, DataHead::align);

    data_head.affix_info = affix_compress ? partially_expand ? 1 : 2 : 0;
    data_head.invisible_soundslike = invisible_soundslike;
    data_head.soundslike_root_only = affix_compress  && !partially_expand ? 1 : 0;

//#if 0
    CERR.printl("FLAGS:  ");
    if (full_soundslike) CERR.printl("  full soundslike");
    if (invisible_soundslike) CERR.printl("  invisible soundslike");
    if (data_head.soundslike_root_only) CERR.printl("  soundslike root only");
    if (affix_compress) CERR.printl("  affix compress");
    if (partially_expand) CERR.printl("  partially expand");
    CERR.printl("---");
//#endif
    
    String temp;

    int num_entries = 0;
    
    ObjStack buf(16*1024);
    String sl_buf;

    WordData * first = 0;

    //
    // Read in Wordlist
    //
    {
      const char * w0;
      ObjStack exp_buf;
      WordAff * exp_list;
      WordAff single;
      single.next = 0;
      Vector<WordAff> af_list;
      WordData * * prev = &first;

      while ( (w0 = els->next()) != 0) {

	unsigned int s = strlen(w0);
	CharVector tstr;
	tstr.append(w0, s+1);
	RET_ON_ERR_SET(iconv(tstr.data(), tstr.size()), char *, w);
        s = strlen(w);

        if (s > 240)
          return make_err(invalid_word, MsgConv(lang)(w),
                          _("The total word length is larger than 240 characters."));

        char * p0 = strchr(w, '/');

        if (p0) { // affix info
	  s = p0 - w;
          *p0 = '\0';
          ++p0;
        }

        const char * affixes = p0;

	RET_ON_ERR(check_if_valid(lang,w));

        if (affixes && !lang.affix())
          return make_err(other_error, 
                          _("Affix flags found in word but no affix file given."));

        if (affixes && !affix_compress) {
          exp_buf.reset();
          exp_list = lang.affix()->expand(w, affixes, exp_buf);
        } else if (affixes && partially_expand) {
          // expand any affixes which will effect the first
          // 3 letters of a word.  This is needed so that the
          // jump tables will function correctly
          exp_buf.reset();
          exp_list = lang.affix()->expand(w, affixes, exp_buf, 3);
        } else {
          single.word.str = w;
          single.word.size = strlen(w);
          single.aff = (const byte *)(affixes ? affixes : "");
          exp_list = &single;
        }

        // iterate through each expanded word
        
        for (WordAff * p = exp_list; p; p = p->next)
        {
          const char * w = p->word.str;
          s = p->word.size;
          
          unsigned total_size = sizeof(WordData);
          unsigned data_size = s + 1;
          unsigned aff_size = strlen((const char *)p->aff);
          if (aff_size > 0) data_size += aff_size + 1;
          total_size += data_size;
          lang.to_soundslike(sl_buf, w);
          const char * sl = sl_buf.str();
          unsigned sl_size = sl_buf.size();
          if (strcmp(sl,w) == 0) sl = w;
          if (sl != w) total_size += sl_size + 1;

          if (total_size - sizeof(WordData) > 240)
            return make_err(invalid_word, MsgConv(lang)(w),
                            _("The total word length, with soundslike data, is larger than 240 characters."));

          WordData * b = (WordData *)buf.alloc(total_size);
          *prev = b;
          b->next = 0;
          prev = &b->next;
          
          b->word_size = s;
          b->sl_size = strlen(sl);
          b->data_size = data_size;
          b->flags = lang.get_word_info(w);

          char * z = b->word;

          memcpy(z, w, s + 1);
          z += s + 1;

          if (aff_size > 0) {
            b->flags |= HAVE_AFFIX_FLAGS;
            b->aff = z;
            memcpy(z, p->aff, aff_size + 1);
            z += aff_size + 1;
          } else {
            b->aff = 0;
          }

          if (sl != w) {
            memcpy(z, sl, sl_size + 1);
            b->sl = z;
          } else {
            b->sl = b->word;
          }

          ++num_entries;
        }
      }
      delete els;
    }

    //
    // sort WordData linked list based on (sl, word)
    //

    first = sort(first, SoundslikeLess());

    //
    // duplicate check
    // 
    WordData * prev = first;
    WordData * cur = first->next;
    while (cur) {
      if (strcmp(prev->word, cur->word) == 0) {
        // merge affix info if necessary
        if (!prev->aff && cur->aff) {
          prev->flags |= HAVE_AFFIX_FLAGS;
          prev->aff = cur->aff;
          prev->data_size += strlen(prev->aff) + 1;
        } else if (prev->aff && cur->aff) {
          unsigned l1 = strlen(prev->aff);
          unsigned l2 = strlen(cur->aff);
          char * aff = (char *)buf.alloc(l1 + l2 + 1);
          memcpy(aff, prev->aff, l1);
          prev->aff = aff;
          aff += l1;
          for (const char * p = cur->aff; *p; ++p) {
            if (memchr(prev->aff, l1, *p)) continue;
            *aff = *p;
            ++aff;
          }
          *aff = '\0';
          prev->data_size = prev->word_size + (aff - prev->aff) + 2;
        }
        prev->next = cur->next;
      } else {
        prev = cur;
      }
      cur = cur->next;
    }

    //
    //
    //

    unsigned data_size = 16;
    WordData * p = first;
    if (invisible_soundslike) {
      
      for (; p; p = p->next)
        data_size += 3 + p->data_size;

    } else {

      while (p)
      {
        unsigned ds = 2 + p->sl_size + 1;

        char * prev = p->sl;

        do {
          
          ds += 3 + p->data_size;
          p->sl = prev;
          p = p->next;

        } while (p && strcmp(prev, p->sl) == 0 && ds + 3 + p->data_size < 255);

        data_size += ds;

      }

    }

    //
    // Create the final data structures
    //

    CharVector     data;
    data.reserve(data_size);
    data.write32(0); // to avoid nasty special cases
    unsigned int prev_pos = data.size();
    data.write32(0);

    WordLookup lookup(num_entries * 4 / 5);
    lookup.parms().block_begin = data.begin();
    lookup.parms().hash .lang     = &lang;
    lookup.parms().equal.cmp.lang = &lang;

    Vector<Jump> jump1;
    Vector<Jump> jump2;

    const int head_size = invisible_soundslike ? 3 : 2;

    const char * prev_sl = "";
    p = first;
    while (p)
    {
      if (invisible_soundslike) {

        data.write(p->flags); // flags  
        data.write('\0'); // place holder for offset to next item
        data.write(p->word_size);

      } else {

        data.write('\0'); // place holder for offset to next item
        data.write(p->sl_size);

      }
        
      if (strncmp(prev_sl, p->sl, 3) != 0) {
        
        Jump jump;
        strncpy(jump.sl, p->sl, 3);
        jump.loc = data.size();
        jump2.push_back(jump);
        
        if (strncmp(prev_sl, p->sl, 2) != 0) {
          Jump jump;
          strncpy(jump.sl, p->sl, 2);
          jump.loc = jump2.size() - 1;
          jump1.push_back(jump);
        }

        data[prev_pos - NEXT_O] = (byte)(data.size() - prev_pos - head_size + 1);
        // when advanced to this position the offset byte will
        // be null (since it will point to the null terminator
        // of the last word) and will thus signal the end of the
        // group
        
      } else {
        
        data[prev_pos - NEXT_O] = (byte)(data.size() - prev_pos);
        
      }
        
      prev_pos = data.size();
      prev_sl = p->sl;

      if (invisible_soundslike) {
        
        unsigned pos = data.size();
        data.write(p->word, p->word_size + 1);
        if (p->aff) data.write(p->aff, p->data_size - p->word_size - 1);
        lookup.insert(pos);

        p = p->next;

      } else {

        data.write(p->sl, p->sl_size + 1);

        // write all word entries with the same soundslike

        const char * prev;

        do {
          data.write(p->flags);
          data.write(p->data_size + 3);
          data.write(p->word_size);

          unsigned pos = data.size();
          data.write(p->word, p->word_size + 1);
          if (p->aff) data.write(p->aff, p->data_size - p->word_size - 1);
          lookup.insert(pos);

          prev = p->sl;

          p = p->next;

        } while (p && prev == p->sl); // yes I really mean to use pointer compare here
      }
    }

    // add special end case
    if (data.size() % 2 != 0) data.write('\0');
    data.write16(0);
    data.write16(0);
    data[prev_pos - NEXT_O] |= (byte)(data.size() - prev_pos);
    data.at32(4) = 0;
    
    jump2.push_back(Jump());
    jump1.push_back(Jump());
    
    data.write(0);
    data.write(0);
    data.write(0);

    data_head.word_count   = lookup.size();
    data_head.word_buckets = lookup.bucket_count();

    FStream out;
    out.open(base, "wb");

    advance_file(out, data_head.head_size);

    // Write jump1 table
    data_head.jump1_offset = out.tell() - data_head.head_size;
    out.write(jump1.data(), jump1.size() * sizeof(Jump));
    
    // Write jump2 table
    advance_file(out, round_up(out.tell(), DataHead::align));
    data_head.jump2_offset = out.tell() - data_head.head_size;
    out.write(jump2.data(), jump2.size() * sizeof(Jump));

    // Write data block
    advance_file(out, round_up(out.tell(), DataHead::align));
    data_head.word_offset = out.tell() - data_head.head_size;
    out.write(data.data(), data.size());

    // Write hash
    advance_file(out, round_up(out.tell(), DataHead::align));
    data_head.hash_offset = out.tell() - data_head.head_size;
    out.write(&lookup.vector().front(), lookup.vector().size() * 4);
    
    // calculate block size
    advance_file(out, round_up(out.tell(), DataHead::align));
    data_head.block_size = out.tell() - data_head.head_size;

    // write data head to file
    out.seek(0);
    out.write(&data_head, sizeof(DataHead));
    out.write(" ", 1);
    out.write(lang.name(), data_head.lang_name_size);
    out.write(lang.soundslike_name(), data_head.soundslike_name_size);
    out.write(lang.soundslike_version(), data_head.soundslike_version_size);

    return no_err;
  }

}

namespace aspeller {
  PosibErr<void> create_default_readonly_dict(StringEnumeration * els,
                                              Config & config)
  {
    CachePtr<Language> lang;
    PosibErr<Language *> res = new_language(config);
    if (res.has_err()) return res;
    lang.reset(res.data);
    lang->set_lang_defaults(config);
    RET_ON_ERR(create(els,*lang,config));
    return no_err;
  }
}

