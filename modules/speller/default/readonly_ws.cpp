// This file is part of The New Aspell
// Copyright (C) 2000-2001 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.

// Aspell's main word list data is stored in 4 large blocks of memory
//
// * The Word Hash Table
// * The Word List
// * The Soundslike Hash Table
// * The Soundslike List
//
// 1a) The Word Hash Table
// This consists of an open address hash table which contains pointers
// to the actual words in the word list
//
// 1b) The Word List
// This consists of the actual word list and is layed out as follows:
//   <Word1><null char><Word2><null char>...
//
// 2a) The Soundslike Hash Table
// This consists of an open address hash table which contains pointers
// to a soundslike object.
//
// 2b) The Soundslike Object
// The soundslike object is layed out as follow:
//  What:  <Word1 pointer><Word2 p.>...<Num of Words><Soundslike><null char>
//  Types: <const char *><const char *>...<unsigned short int><char[]><char>

//         <unsigned int><unsigned int>...<unsigned short int><char[]><char>
// The pointer to the object points to the beginning of the Soundslike string
// The Word pointers consists of the the words which have the same 
//   soundslike pattern
//
// 2c) The Soundslike List
// This consists of Soundslike Objects back to back:
//  <Soundslike object 1><Soundslike object 2> ...
// There is no delimiter between the objects
//
//
//                          Format of the *.wrd files
//
// (This part is in ascii format)
// <"master_wl"><ws><lang name><ws><# words><ws>
//     <hash size><ws><size of list block><\n>
// (The rest is in binary format>
// <Wordlist>
// <Word Hash Table>
//
// The word hash table is a vector of unsigned its which contains an offset
// of where they can be found in the word list.
//
//                          Format of the *.sl files
//
// (This part is in ascii format)
// <"master_wl"><ws><lang name><ws><# words><ws>
//     <hash size><ws><size of list block><\n>
// (The rest is in binary format>
// <Soundslike object list>
// <Soundslike Hash Table>
//
// Soundslike oject is laid out as follows:
//   <Num of Words><Word 1 offset>...<Soundslike><\0>
//   <unsigned short int><unsigned int>...<char[]><char>
// And like the .wrd file the hash table contains offsets not pointers.
//

#include <vector>

using std::vector;
using std::pair;

#include <string.h>
#include <stdio.h>
//#include <errno.h>

#include "settings.h"

#include "fstream.hpp"
#include "vector_hash-t.hpp"
#include "block_vector.hpp"
#include "data.hpp"
#include "file_util.hpp"
#include "data_util.hpp"
#include "language.hpp"
#include "config.hpp"
#include "string_buffer.hpp"
#include "errors.hpp"

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
    
    struct SoundslikeLookupParms {
      const char * block_begin;
      SoundslikeLookupParms() {}
      SoundslikeLookupParms(const char * b) : block_begin(b) {}
      typedef BlockVector<const u32int> Vector;
      typedef u32int                    Value;
      typedef const char *              Key;
      static const bool is_multi = false;
      Key key(Value v) const {return block_begin + v;}
      hash<const char *> hash;
      bool equal(Key rhs, Key lhs) const {return strcmp(rhs,lhs) == 0;}
      bool is_nonexistent(Value v) const {return v == u32int_max;}
      void make_nonexistent(const Value & v) const {abort();}
    };
    typedef VectorHashTable<SoundslikeLookupParms> SoundslikeLookup;

  private:
      
    char *           block;
    u32int           block_size;
    bool             block_mmaped;
    WordLookup       word_lookup;
    const char *     word_block;
    u32int           max_word_length;
    bool             use_soundslike;
    SoundslikeLookup soundslike_lookup;
    const char *     soundslike_block;
    
    ReadOnlyWS(const ReadOnlyWS&);
    ReadOnlyWS& operator= (const ReadOnlyWS&);

    struct ElementsParms;
    struct SoundslikeElementsParms;
    struct SoundslikeWordsParms;

    struct SoundslikeElementsParmsNoSL;
    struct SoundslikeWordsParmsNoSL;
    struct SoundslikeWordsEmulSingle;

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
    u32int total_block_size;
    u32int word_block_size;
    u32int word_count;
    u32int word_buckets;
    u32int word_size;
    u32int max_word_length;
    u32int soundslike_block_size;
    u32int soundslike_count;
    u32int soundslike_buckets;
    u32int soundslike_size;
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

    if (strcmp(data_head.check_word, "aspell default speller rowl 1.4") != 0)
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

    block_size = data_head.total_block_size;
    block = mmap_open(block_size, f, data_head.head_size);
    block_mmaped = block != (char *)MAP_FAILED;
    if (!block_mmaped) {
      block = new char[block_size];
      f.seek(data_head.head_size, SEEK_SET);
      f.read(block, block_size);
    }

    word_block       = block;

    word_lookup.parms().block_begin = word_block;
    word_lookup.parms().hash .lang   = lang();
    word_lookup.parms().equal.lang   = lang();
    const u32int * begin = reinterpret_cast<const u32int *>
      (word_block + data_head.word_block_size);
    word_lookup.vector().set(begin, begin + data_head.word_buckets);
    word_lookup.set_size(data_head.word_count);
    
    max_word_length = data_head.max_word_length;
    
    if (use_soundslike) {
      soundslike_block = block + data_head.word_block_size + data_head.word_size;
      soundslike_lookup.parms().block_begin = soundslike_block;
      begin = reinterpret_cast<const u32int *>
	(soundslike_block + data_head.soundslike_block_size);
      soundslike_lookup.vector().set(begin,
				     begin + data_head.soundslike_buckets);
      soundslike_lookup.set_size(data_head.soundslike_count);
    }

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

  struct ReadOnlyWS::SoundslikeWordsParms {
    typedef BasicWordInfo                   Value;
    typedef const u32int *             Iterator;
    const char * word_block_begin;
    Iterator     end;
    SoundslikeWordsParms(const char * b, Iterator e) 
      : word_block_begin(b), end(e) {}
    bool endf(Iterator i) const {return i == end;}
    Value end_state() const {return 0;}
    Value deref(Iterator i) const {
      return Value(word_block_begin + *i, *(word_block_begin + *i - 1));
    }
  };

  struct ReadOnlyWS::SoundslikeElementsParms {
    typedef SoundslikeWord                   Value;
    typedef SoundslikeLookup::const_iterator Iterator;

    const char * soundslike_block_begin;
      
    SoundslikeElementsParms(const char * b) 
      : soundslike_block_begin(b) {}
      
    bool endf(Iterator i) const {return i.at_end();}
    
    Value deref(Iterator i) {
      return Value(soundslike_block_begin + *i, 0);
    }

    Value end_state() {return Value(0,0);}
  };

  struct ReadOnlyWS::SoundslikeElementsParmsNoSL {
    typedef SoundslikeWord              Value;
    typedef WordLookup::const_iterator  Iterator; 
    const char * word_block_begin;
    const Language * lang;
    vector<char> buf;
    SoundslikeElementsParmsNoSL(u32int max_len, const char * b, 
				const Language * l) 
      : word_block_begin(b), lang(l)
    {
      buf.resize(max_len + 1);
    }
    bool endf(const Iterator & i) const {return i.at_end();}
    Value end_state() const {return Value(0,0);}
    Value deref(const Iterator & i) 
    {
      Value v(&*buf.begin(), word_block_begin + *i);
      const char * w = static_cast<const char *>(v.word_list_pointer);
      int j = 0;
      for (;w[j] != '\0'; ++j)
	buf[j] = lang->to_stripped(w[j]);
      buf[j] = '\0';
      return v;
    }
  };

  struct ReadOnlyWS::SoundslikeWordsParmsNoSL {
    typedef BasicWordInfo                       Value;
    typedef WordLookup::ConstFindIterator  Iterator; 
    const char * word_block_begin;
    SoundslikeWordsParmsNoSL(const char * b) 
      : word_block_begin(b) {}
    bool endf(const Iterator & i) const {return i.at_end();}
    Value end_state() const {return 0;}
    Value deref(const Iterator & i) const 
    {
      return Value(word_block_begin + i.deref(), 
		   *(word_block_begin + i.deref() - 1));
    }
  };

  struct ReadOnlyWS::SoundslikeWordsEmulSingle : public ReadOnlyWS::VirEmul
  {
  private:
    const char * word;
  public:
    SoundslikeWordsEmulSingle(const char * w) : word(w) {}
    VirEmul * clone() const {return new SoundslikeWordsEmulSingle(*this);}
    void assign(const VirEmul * other) {
      *this = *static_cast<const SoundslikeWordsEmulSingle *>(other);
    }
    bool at_end() const {return word == 0;}
    BasicWordInfo next() {
      const char * w = word;
      if (w != 0) {
	word = 0;
	return BasicWordInfo(w, *(w-1));
      } else {
	return BasicWordInfo();
      }
    }
  };
  
    
  ReadOnlyWS::VirSoundslikeEmul * ReadOnlyWS::soundslike_elements() const {

    if (use_soundslike) {
      
      return new MakeVirEnumeration<SoundslikeElementsParms>
	(soundslike_lookup.begin(), soundslike_block);

    } else {

      return new MakeVirEnumeration<SoundslikeElementsParmsNoSL>
	(word_lookup.begin(), 
	 SoundslikeElementsParmsNoSL(max_word_length,block,lang()));
      
    }
  }
    
  ReadOnlyWS::VirEmul * 
  ReadOnlyWS::words_w_soundslike(const char * soundslike) const {

    if (use_soundslike) {

      SoundslikeLookup::const_iterator i = soundslike_lookup.find(soundslike);
      if (i == soundslike_lookup.end()) { 
	return new MakeAlwaysEndEnumeration<BasicWordInfo>();
      } else {
	return ReadOnlyWS::words_w_soundslike
	  (SoundslikeWord(soundslike_block + *i, 0));
      }

    } else {

      WordLookup::ConstFindIterator i = word_lookup.multi_find(soundslike);
      return new MakeVirEnumeration<SoundslikeWordsParmsNoSL>(i, block);
      
    }
    
  }
  
  ReadOnlyWS::VirEmul *
  ReadOnlyWS::words_w_soundslike(SoundslikeWord w) const {

    if (use_soundslike) {
    
      const u32int * end = reinterpret_cast<const u32int *>(w.soundslike - 2);
      u16int size = *reinterpret_cast<const u16int *>(end);
      
      return new MakeVirEnumeration<SoundslikeWordsParms>
	(end - size, SoundslikeWordsParms(word_block, end));

    } else {
      
      return new SoundslikeWordsEmulSingle
	(static_cast<const char *>(w.word_list_pointer));
      
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
    typedef vector<const char *> Vector;
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

  struct SoundslikeLookupParms {
    typedef const char *                Key;
    struct List {
      union {
	u32int * list;
	u32int   single;
      } d;
      u16int   size;
      u16int   num_inserted;
    };
    typedef pair<Key, List> Value;
    typedef vector<Value>   Vector;
    static const bool is_multi = false;
    const Key & key(const Value & v) const {return v.first;}
    hash<const char *>  hash;
    bool equal(const Key & rhs, const Key & lhs) const {return strcmp(rhs,lhs) == 0;}
    bool is_nonexistent(const Value & v) const {return v.first == 0;}
    void make_nonexistent(Value & v) const {
      memset(&v, 0, sizeof(Value));
    }
  };

  typedef VectorHashTable<SoundslikeLookupParms> SoundHash;

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

    FStream OUT;
   
    OUT.open(base, "wb");

    DataHead data_head;
    memset(&data_head, 0, sizeof(data_head));
    strcpy(data_head.check_word, "aspell default speller rowl 1.4");

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

    SoundHash sound_prehash;
    StringBuffer   sound_prehash_char_buf;
    vector<u32int> sound_prehash_list_buf;
    int            sound_prehash_list_buf_size = 0;
    {
      WordHash word_hash;
      StringBuffer buf;
      word_hash.parms().hash_ .lang = &lang;
      word_hash.parms().equal_.lang = &lang;
      const char * w0;
      WordHash::MutableFindIterator j;

      int z = 0;
      //
      // Reading in Wordlist from stdin and creating Word Hash
      //
      while ( (w0 = els->next()) != 0) {

	unsigned int s = strlen(w0);
	vector<char> tstr(w0, w0+s+1);
	char * w = &tstr[0];
	
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

	for (j =word_hash.multi_find(w); !j.at_end(); j.adv())
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
	  word_hash.insert(b);
	}
	++z;
      }
      delete els;
      
      word_hash.resize(word_hash.size()*4/5);
      
      //
      // Witting word data, creating Final Hash, creating sounds Pre Hash
      //
      
      advance_file(OUT, data_head.head_size);
      std::streampos start = data_head.head_size;

      if (use_soundslike)
	sound_prehash.resize(word_hash.bucket_count());
      
      vector<u32int> final_hash(word_hash.bucket_count(), u32int_max);
      
      OUT << '\0';
      for (unsigned int i = 0; i != word_hash.vector().size(); ++i) {
	
	const char * value = word_hash.vector()[i];
	
	if (word_hash.parms().is_nonexistent(value)) continue;

	// write compound info
	if (*(value - 1) != '\0')
	  OUT << *(value-1);

	final_hash[i] = OUT.tell() - start;

	OUT << value << '\0';
	if (use_soundslike) {

	  temp = lang.to_soundslike(value);
	  SoundHash::iterator j = sound_prehash.find(temp.c_str());
	  if (j == sound_prehash.end()) {

	    SoundHash::value_type to_insert;
	    to_insert.first = sound_prehash_char_buf.alloc(temp.size()+1);
	    strncpy(const_cast<char *>(to_insert.first), 
		    temp.c_str(), 
		    temp.size() + 1);
	    sound_prehash.insert(to_insert).first->second.size = 1;

	  } else {

	    if (j->second.size == 1)
	      sound_prehash_list_buf_size++;
	    
	    j->second.size++;
	    sound_prehash_list_buf_size++;

	  }
	}
      }

      if (use_soundslike) {

	sound_prehash_list_buf.resize(sound_prehash_list_buf_size);
	int p = 0;

	for (unsigned int i = 0; i != word_hash.vector().size(); ++i) {
	  
	  const char * value = word_hash.vector()[i];
	  
	  if (word_hash.parms().is_nonexistent(value)) continue;
	  
	  temp = lang.to_soundslike(value);
	  SoundHash::iterator j = sound_prehash.find(temp.c_str());
	  //assert(j != sound_prehash.end());
	  if (j->second.num_inserted == 0 && j->second.size != 1) {
	    j->second.d.list = &*sound_prehash_list_buf.begin() + p;
	    p += j->second.size;
	  } 
	  if (j->second.size == 1) {
	    j->second.d.single = final_hash[i];
	  } else {
	    j->second.d.list[j->second.num_inserted] = final_hash[i];
	  }
	  ++j->second.num_inserted;
	}
      }
      
      data_head.word_block_size = round_up(OUT.tell() - start + 1l, 
					   page_size);
      data_head.total_block_size = data_head.word_block_size;

      advance_file(OUT, data_head.head_size + data_head.total_block_size);

      // Writting final hash
      OUT.write(reinterpret_cast<const char *>(&final_hash.front()),
		final_hash.size() * 4);

      data_head.word_count   = word_hash.size();
      data_head.word_buckets = word_hash.bucket_count();
      data_head.word_size    
	= round_up(word_hash.bucket_count() * 4, page_size);
      data_head.total_block_size += data_head.word_size;
      advance_file(OUT, data_head.head_size + data_head.total_block_size);
    }
    
    if (use_soundslike) {
      sound_prehash.resize(sound_prehash.size()*4/5);
    
      vector<u32int> final_hash(sound_prehash.bucket_count(), u32int_max);

      std::streampos start = OUT.tell();
      
      //
      // Writting soundslike words, creating soundslike Final Hash
      //
      for (unsigned int i = 0; i != sound_prehash.vector().size(); ++i) {
	
	const SoundHash::value_type & value = sound_prehash.vector()[i];
	
	if (sound_prehash.parms().is_nonexistent(value)) continue;

	u16int count = value.second.size;

	if (count == 1) {
	  OUT.write(reinterpret_cast<const char *>(&value.second.d.single), 
		    4);
	} else {
	  OUT.write(reinterpret_cast<const char *>(value.second.d.list),
		    count * 4);
	}

	OUT.write(reinterpret_cast<char *>(&count),2);

	final_hash[i] = OUT.tell() - start;

	OUT << value.first << '\0';

	advance_file(OUT, round_up(OUT.tell(), 4));
      }
      data_head.soundslike_block_size 
	= round_up(OUT.tell() - start, page_size);
      data_head.total_block_size += data_head.soundslike_block_size;

      // Witting Final soundslike Hash
      advance_file(OUT, data_head.head_size + data_head.total_block_size);
      OUT.write(reinterpret_cast<char *>(&final_hash.front()),
		final_hash.size() * 4);

      data_head.soundslike_count   = sound_prehash.size();
      data_head.soundslike_buckets = sound_prehash.bucket_count();
      data_head.soundslike_size    
	= round_up(final_hash.size() * 4, page_size);
      data_head.total_block_size += data_head.soundslike_size;
    
    }

    advance_file(OUT, data_head.head_size + data_head.total_block_size);

    // write data head to file
    OUT.restart();
    OUT.write((char *)&data_head, sizeof(DataHead));
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
