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

static inline size_t page_size() 
{
#ifdef _SC_PAGESIZE
 /* BSDi does not expose this limit via the sysconf function */
  return sysconf (_SC_PAGESIZE);
#else
  return getpagesize ();
#endif
}

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

static inline size_t page_size() 
{
  return 1024;
}

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

  private:
      
    char *           block;
    u32int           block_size;
    bool             block_mmaped;
    const SoundslikeJump * jump1;
    const SoundslikeJump * jump2;
    WordLookup       word_lookup;
    const char *     word_block;
    u32int           max_word_length;
    bool             use_soundslike;
    
    ReadOnlyWS(const ReadOnlyWS&);
    ReadOnlyWS& operator= (const ReadOnlyWS&);

    struct ElementsParms;
    struct SoundslikeElements;
    friend class SoundslikeElements;
    struct SoundslikeWords;

  public:
    VirEmul * detailed_elements() const;
    Size      size()     const;
    bool      empty()    const;
      
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
    const char * word_block_begin;
    ElementsParms(const char * b) : word_block_begin(b) {}
    bool endf(const Iterator & i) const {return i.at_end();}
    Value end_state() const {return 0;}
    Value deref(const Iterator & i) const {
      return Value(word_block_begin + *i, *(word_block_begin + *i - 1));
    }
  };

  ReadOnlyWS::VirEmul * ReadOnlyWS::detailed_elements() const {
    return new MakeVirEnumeration<ElementsParms>
      (word_lookup.begin(), ElementsParms(block));
  }

  ReadOnlyWS::Size ReadOnlyWS::size() const {
    return word_lookup.size();
  }
  
  bool ReadOnlyWS::empty() const {
    return word_lookup.empty();
  }

  struct DataHead {
    // all sizes except the last four must to divisible by
    // page_size()
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

    if (strcmp(data_head.check_word, "aspell default speller rowl 1.5") != 0)
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
	return BasicWordInfo(w,*(w-1));
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

  struct ReadOnlyWS::SoundslikeElements : public VirSoundslikeEmul
  {
    int level;
    const ReadOnlyWS * obj;
    const SoundslikeJump * jump1;
    const SoundslikeJump * jump2;
    const char * cur;

    CharVector buf;
    const Language * lang;

    u16int next_pos() const 
      {return *reinterpret_cast<const u16int *>(cur - 2);}

    SoundslikeElements(const ReadOnlyWS * o, const Language * l = 0)
      : level(1), obj(o), 
	jump1(obj->jump1), jump2(obj->jump2), 
	cur(0), lang(l) {}

    SoundslikeWord next(int stopped_at) {

      const char * tmp = cur;

      //COUT << level << ":" << stopped_at << "  ";
      //COUT << jump1->sl << " " << jump2->sl << "\n";

      if (level == 1 && stopped_at < 2) {

	++jump1;
	return SoundslikeWord(jump1->sl, 0);
	  
      } else if (level == 2 && stopped_at < 3) {

	++jump2;
	if (jump2[-1].sl[1] != jump2[0].sl[1]) {
	  ++jump1;
	  level = 1;
	  return SoundslikeWord(jump1->sl, 0);
	} else {
	  return SoundslikeWord(jump2->sl, 0);
	}
	
      } else if (level == 1) {

	level = 2;
	jump2 = obj->jump2 + jump1->loc;
	return SoundslikeWord(jump2->sl, 0);

      } else if (level == 2) {

	cur = tmp = obj->word_block + jump2->loc;
	cur += next_pos(); // next pos uses cur
	level = 3;

      } else if (next_pos() == 0) {

	cur += 2;
	level = 2;
	++jump2;
	if (jump2[-1].sl[1] != jump2[0].sl[1]) {
	  level = 1;
	  ++jump1;
	  return SoundslikeWord(jump1->sl, 0);
	} else {
	  return SoundslikeWord(jump2->sl, 0);
	}

      } else {

	cur += next_pos();
      
      }

      //COUT << "T:" << tmp << "\n";

      if (!lang) {
	return SoundslikeWord(tmp, tmp);
      } else {
	buf.clear();
	to_stripped(*lang, tmp, buf);
	buf.append('\0');
	return SoundslikeWord(buf.data(), tmp);
      }
    }
  };

    
  ReadOnlyWS::VirSoundslikeEmul * ReadOnlyWS::soundslike_elements() const {

    if (use_soundslike) {
      
      return new SoundslikeElements(this);

    } else {

      return new SoundslikeElements(this, lang());
      
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

    } else {
      
      return new SoundslikeWords(static_cast<const char *>
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

  struct WordLookupParms {
    typedef Vector<const char *> Vector;
    typedef const char *         Value;
    typedef const char *         Key;
    static const bool is_multi = true;
    const Key & key(const Value & v) const {return v;}
    InsensitiveHash hash_;
    size_t hash(const Key & k) const {return hash_(k);}
    InsensitiveEqual equal_;
    bool equal(const Key & rhs, const Key & lhs) const {
      return equal_(rhs, lhs);
    }
    bool is_nonexistent(const Value & v) const {return v == 0;}
    void make_nonexistent(Value & v) const {v = 0;}
  };

  typedef VectorHashTable<WordLookupParms> WordHash;

  struct SoundslikeList {
    struct Word {
      const char * str;
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

  typedef std::map<const char *, SoundslikeList, SoundslikeLt> SoundMap;

  static inline unsigned int round_up(unsigned int i, unsigned int size) {
    return ((i + size - 1)/size)*size;
  }

  static void advance_file(FStream & OUT, int pos) {
    int diff = pos - OUT.tell();
    assert(diff >= 0);
    for(; diff != 0; --diff)
      OUT << '\0';
  }

  PosibErr<void> create (ParmString base, 
			 StringEnumeration * els,
			 const Language & lang) 
  {
    size_t page_size = ::page_size();
    
    assert(sizeof(u16int) == 2);
    assert(sizeof(u32int) == 4);

    bool use_soundslike=true;
    if (strcmp(lang.soundslike_name(),"none") == 0)
      use_soundslike=false;

    const char * mid_chars = lang.mid_chars();

    DataHead data_head;
    memset(&data_head, 0, sizeof(data_head));
    strcpy(data_head.check_word, "aspell default speller rowl 1.5");

    data_head.lang_name_size          = strlen(lang.name()) + 1;
    data_head.soundslike_name_size    = strlen(lang.soundslike_name()) + 1;
    data_head.soundslike_version_size = strlen(lang.soundslike_version()) + 1;
    data_head.middle_chars_size       = strlen(mid_chars) + 1;
    data_head.head_size  = sizeof(DataHead);
    data_head.head_size += data_head.lang_name_size;
    data_head.head_size += data_head.soundslike_name_size;
    data_head.head_size += data_head.soundslike_version_size;
    data_head.head_size  = round_up(data_head.head_size, page_size);

    data_head.minimal_specified = u32int_max;

    String temp;

    // these are the final two data structures
    CharVector     data;
    Vector<u32int> final_hash;


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

      int z = 0;
      while ( (w0 = els->next()) != 0) {

	unsigned int s = strlen(w0);
	CharVector tstr;
	tstr.append(w0, s+1);
	char * w = tstr.data();
	
	char * p = strchr(w, ':');
	if (p == 0) {
	  p = w + s;
	} else {
	  s = p - w;
	  *p = '\0';
	  ++p;
	}
	
	check_if_valid(lang,w);

	// Read in compound info
	
	CompoundInfo c;
	if (*c.read(p, lang) != '\0')
	  return make_err(invalid_flag, w, p);
	
	// Check if it already has been inserted

	for (j =word_hash->multi_find(w); !j.at_end(); j.adv())
	  if (strcmp(w, j.deref())==0) break;

	// If already insert deal with compound info

	bool reinsert=false;

	if (!j.at_end()) {
	  CompoundInfo c0(static_cast<unsigned char>(*(j.deref() - 1)));
	  if (c.any() && !c0.any())
	    reinsert = true;
	  else if (!c.any() || !c0.any())
	    ;
	  else if (c.d != c0.d)
	    abort(); // FIXME
	    //return make_err(conflicting_flags, w, c0, c, lang);
	}
	
	// Finally insert the word into the dictionary

	if (j.at_end() || reinsert) {
	  if(s > data_head.max_word_length)
	    data_head.max_word_length = s;
	  char * b;
	  if (c.any()) {
	    if (s < data_head.minimal_specified)
	      data_head.minimal_specified = s;
	    b = buf.alloc(s + 2);
	    *b = static_cast<char>(c.d);
	    ++b;
	  } else {
	    b = buf.alloc(s + 1);
	  }
	  strncpy(b, w, s+1);
	  word_hash->insert(b);
	}
	++z;
      }
      delete els;
    }

    word_hash->resize(word_hash->size()*4/5);

    //
    // 
    //
    {
      for (unsigned int i = 0; i != word_hash->vector().size(); ++i) {
	
	const char * value = word_hash->vector()[i];
	
	if (word_hash->parms().is_nonexistent(value)) continue;

	temp = lang.to_soundslike(value);
	SoundMap::iterator j = sound_map.find(temp.c_str());
	if (j == sound_map.end()) {
	  
	  if (use_soundslike) {
	    SoundMap::value_type
	      to_insert(buf.alloc(temp.size()+1), SoundslikeList());
	    strncpy(const_cast<char *>(to_insert.first),
		    temp.c_str(),
		    temp.size() + 1);
	    sound_map.insert(to_insert).first->second.size = 1;
	  } else {
	    SoundMap::value_type to_insert(value, 
					   SoundslikeList());
	    sound_map.insert(to_insert).first->second.size = 1;
	  }
	  
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
	
	const char * value = word_hash->vector()[i];
	
	if (word_hash->parms().is_nonexistent(value)) continue;

	temp = lang.to_soundslike(value);
	SoundMap::iterator j = sound_map.find(temp.c_str());
	assert(j != sound_map.end());
	if (j->second.num_inserted == 0 && j->second.size != 1) {
	  j->second.d.list = &*sl_list_buf.begin() + p;
	  p += j->second.size;
	} 
	if (j->second.size == 1) {
	  j->second.d.single.str      = value;
	  j->second.d.single.hash_idx = i;
	} else {
	  j->second.d.list[j->second.num_inserted].str      = value;
	  j->second.d.list[j->second.num_inserted].hash_idx = i;
	}
	++j->second.num_inserted;
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
	} else {
	  sl.clear();
	  to_stripped(lang, i->first, sl);
	}

	while (data.size() % 2 != 0)
	  data.write('\0');

	if (use_soundslike)
	  data.write16(sl.size());
	data.write16(0); // place holder for offset to next item
	
	if (strncmp(prev_sl.c_str(), sl.c_str(), 3)) {

	  SoundslikeJump jump;
	  strncpy(jump.sl, sl.c_str(), 3);
	  jump.loc = data.size();
	  jump2.push_back(jump);

	  if (strncmp(prev_sl.c_str(), sl.c_str(), 2)) {
	    SoundslikeJump jump;
	    strncpy(jump.sl, sl.c_str(), 2);
	    jump.loc = jump2.size() - 1;
	    jump1.push_back(jump);
	  }

	  data.at16(prev_pos - 2) = 0;

	} else {
	  
	  data.at16(prev_pos - 2) = data.size() - prev_pos;

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
	  for (;k != end; ++k) {
	    // write compound info (if any)
	    if (*(k->str - 1))
	      data << *(k->str - 1);

	    final_hash[k->hash_idx] = data.size();

	    data << k->str << '\0';
	  }
	  data << '\0';
	}

	prev_sl = sl;
      }

      // add special end case
      data.write16(0);
      data.write16(0);
      data.at16(prev_pos - 2) |= data.size() - prev_pos;
      data.at32(4) = 0;
      jump2.push_back(SoundslikeJump());
      jump1.push_back(SoundslikeJump());
    }

    data_head.soundslike_count   = sound_map.size();

    FStream OUT;
    OUT.open(base, "wb");

    // Write jump1 table
    advance_file(OUT, data_head.head_size);
    data_head.jump1_offset = OUT.tell() - data_head.head_size;
    OUT.write(jump1.data(), jump1.size() * sizeof(SoundslikeJump));

    // Write jump2 table
    advance_file(OUT, round_up(OUT.tell(), page_size));
    data_head.jump2_offset = OUT.tell() - data_head.head_size;
    OUT.write(jump2.data(), jump2.size() * sizeof(SoundslikeJump));

    // Write data block
    advance_file(OUT, round_up(OUT.tell(), page_size));
    data_head.word_offset = OUT.tell() - data_head.head_size;
    OUT.write(data.data(), data.size());

    // Write hash
    advance_file(OUT, round_up(OUT.tell(), page_size));
    data_head.hash_offset = OUT.tell() - data_head.head_size;
    OUT.write(&final_hash.front(), final_hash.size() * 4);
    
    advance_file(OUT, round_up(OUT.tell(), page_size));
    data_head.block_size = OUT.tell() - data_head.head_size;

    // write data head to file
    OUT.seek(0);
    OUT.write(&data_head, sizeof(DataHead));
    OUT.write(lang.name(), data_head.lang_name_size);
    OUT.write(lang.soundslike_name(), data_head.soundslike_name_size);
    OUT.write(lang.soundslike_version(), data_head.soundslike_version_size);
    OUT.write(mid_chars, data_head.middle_chars_size); 

    return no_err;
  }

}

namespace aspeller {
  PosibErr<void> create_default_readonly_word_set(StringEnumeration * els,
                                                  Config & config)
  {
    Language lang;
    RET_ON_ERR(lang.setup("",&config));
    aspeller_default_readonly_ws::create(config.retrieve("master-path"),
				       els,lang);
    return no_err;
  }
}
