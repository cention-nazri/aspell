// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

// suggest.cc Suggestion code for Aspell

// The magic behind my spell checker comes from merging Lawrence
// Philips excellent metaphone algorithm and Ispell's near miss
// strategy which is inserting a space or hyphen, interchanging two
// adjacent letters, changing one letter, deleting a letter, or adding
// a letter.
// 
// The process goes something like this.
// 
// 1.     Convert the misspelled word to its soundslike equivalent (its
//        metaphone for English words).
// 
// 2.     Find words that have the same soundslike pattern.
//
// 3.     Find words that have similar soundslike patterns. A similar
//        soundlike pattern is a pattern that is obtained by
//        interchanging two adjacent letters, changing one letter,
//        deleting a letter, or adding a letter.
//
// 4.     Score the result list and return the words with the lowest
//        score. The score is roughly the weighed average of the edit
//        distance of the word to the misspelled word, the soundslike
//        equivalent of the two words, and the phoneme of the two words.
//        The edit distance is the weighed total of the number of
//        deletions, insertions, exchanges, or adjacent swaps needed to
//        make one string equivalent to the other.
//
// Please note that the soundlike equivalent is a rough approximation
// of how the words sounds. It is not the phoneme of the word by any
// means.  For more information on the metaphone algorithm please see
// the file metaphone.cc which included a detailed description of it.


// NOTES TO SELF
// Consider jump starting the distance alogo. when to words have the same
// prefix
// Consider short circling the distance algo. when to words have the same
// suffix

#include "getdata.hpp"

#include "fstream.hpp"

#include "amanager.hpp"
#include "asuggest.hpp"
#include "basic_list.hpp"
#include "clone_ptr-t.hpp"
#include "config.hpp"
#include "data.hpp"
#include "editdist.hpp"
#include "editdist2.hpp"
#include "file_data_util.hpp"
#include "errors.hpp"
#include "hash-t.hpp"
#include "language.hpp"
#include "leditdist.hpp"
#include "manager_impl.hpp"
#include "suggest.hpp"

//#define DEBUG_SUGGEST

using namespace aspell;
using namespace pcommon;
  
namespace aspell_default_suggest {

  typedef std::vector<String> NearMissesFinal;

  template <class Iterator>
  inline Iterator preview_next (Iterator i) {
    return ++i;
  }
  
  //
  // OriginalWord stores infomation about the original misspelled word
  //   for convince and speed.
  //
  struct OriginalWord {
    String   word;
    String   word_stripped;
    String   soundslike;
    String   phoneme;
    CasePattern  case_pattern;
    OriginalWord() {}
    OriginalWord (const String &w, const String &sl, const String &p)
      : word(w), soundslike(sl), phoneme(p) {}
    OriginalWord (const String &w, const String &sl, const String &p,
		 const String &l, CasePattern cp)
      : word(w), word_stripped(l), soundslike(sl), phoneme(p), case_pattern(cp)
    {}
  };

  //
  // struct ScoreWordSound - used for storing the possible words while
  //   they are being processed.
  //

  struct ScoreWordSound {
    const char *  word;
    const char *  word_stripped;
    int           score;
    int           soundslike_score;
    bool          count;
    ReplacementList::VirEmul * repl_list;
    ScoreWordSound() {repl_list = 0;}
    ~ScoreWordSound() {delete repl_list;}
  };

  inline int compare (const ScoreWordSound &lhs, 
		      const ScoreWordSound &rhs) 
  {
    int temp = lhs.score - rhs.score;
    if (temp) return temp;
    return strcmp(lhs.word,rhs.word);
  }

  inline bool operator < (const ScoreWordSound & lhs, 
			  const ScoreWordSound & rhs) {
    return compare(lhs, rhs) < 0;
  }

  inline bool operator <= (const ScoreWordSound & lhs, 
			   const ScoreWordSound & rhs) {
    return compare(lhs, rhs) <= 0;
  }

  inline bool operator == (const ScoreWordSound & lhs, 
			   const ScoreWordSound & rhs) {
    return compare(lhs, rhs) == 0;
  }

  typedef BasicList<ScoreWordSound> NearMisses;
 
  class Score {
  protected:
    const Language * lang;
    OriginalWord      original_word;
    SuggestParms     parms;

  public:
    Score(const Language *l, const String &w, const SuggestParms & p)
      : lang(l), original_word(w, l->to_soundslike(w.c_str()), 
			      l->to_phoneme(w.c_str()),
			      to_stripped(*l, w),
			      case_pattern(*l, w)),
      parms(p)
    {}
    String fix_case(const String & word) {
      return aspell::fix_case(*lang,original_word.case_pattern,word);
    }
  };

  class Working : public Score {
   
    int threshold;

    unsigned int max_word_length;

    ManagerImpl  *     manager;
    NearMisses         scored_near_misses;
    NearMisses         near_misses;
    NearMissesFinal  * near_misses_final;

    BasicList<String>      strings;

    static const bool do_count = true;
    static const bool dont_count = false;
    static const bool do_need_alloc = true;
    static const bool dont_need_alloc = false;

    void try_sound(const char *, int ms);
    void add_nearmiss(const char * word, int ms, bool count, 
		      bool need_alloc, ReplacementList::VirEmul * rl = 0) {
      near_misses.push_front(ScoreWordSound());
      ScoreWordSound & d = near_misses.front();
      if (need_alloc) {
	strings.push_front(word);
	d.word = strings.front().c_str();
      } else {
	d.word = word;
      }

      if (parms.use_typo_analysis) {

	unsigned int l = strlen(word);
	if (l > max_word_length) max_word_length = l;
	
      } else {

	if (!is_stripped(*lang,word)) {
	  strings.push_front(to_stripped(*lang,word));
	  d.word_stripped = strings.front().c_str();
	} else {
	  d.word_stripped = d.word;
	}

      }
      d.soundslike_score = ms;
      d.count = count;
      d.repl_list = rl;
    }
    int needed_level(int want, int soundslike_score) {
      int n = (100*want - parms.soundslike_weight*soundslike_score)
	/(parms.word_weight*parms.edit_distance_weights.min);
      return n > 0 ? n : 0;
    }
    int weighted_average(int soundslike_score, int word_score) {
      return (parms.word_weight*word_score 
	      + parms.soundslike_weight*soundslike_score)/100;
    }
    int skip_first_couple(NearMisses::iterator & i) {
      int k = 0;
      while (preview_next(i) != scored_near_misses.end()) 
	// skip over the first couple of items as they should
	// not be counted in the threshold score.
      {
	if (!i->count) {
	  ++i;
	} else if (k == parms.skip) {
	  break;
	} else {
	  ++k;
	  ++i;
	}
      }
      return k;
    }

    void try_others();
    void score_list();
    void transfer();
  public:
    Working(ManagerImpl * m, const Language *l,
	    const String & w, const SuggestParms & p)
      : Score(l,w,p), threshold(1), max_word_length(0), manager(m) {}
    void get_suggestions(NearMissesFinal &sug);
    void get_suggestions_ultra(NearMissesFinal &sug);
  };

  //
  // try_sound - tries the soundslike string if there is a match add 
  //    the possable words to near_misses
  //

  void Working::try_sound (const char * m, int ms)  
  {
    // sound is the object in the list which is a lot smaller than m

    for (ManagerImpl::DataSetCollection::const_iterator i 
	   = manager->data_set_collection().begin();
	 i != manager->data_set_collection().end();
	 ++i) {
      
      if (!i->use_to_suggest) continue;

      if (i->data_set->basic_type == DataSet::basic_word_set) {

	BasicWordSet::Emul e = static_cast<const BasicWordSet *>
	  (i->data_set)->words_w_soundslike(m);
	BasicWordInfo w;
	String word;
	while ((w = e.next())) {
	  w.get_word(word, i->local_info.convert);
	  add_nearmiss(word.c_str(), ms, do_count, do_need_alloc);
	}
	
      } else {

	BasicReplacementSet::Emul e = static_cast<const BasicReplacementSet *>(i->data_set)->repls_w_soundslike(m);
	ReplacementList repl;
	while (! (repl = e.next()).empty() )
	  add_nearmiss(repl.misspelled_word, ms, 
		       dont_count, dont_need_alloc, repl.elements);	  
      }
    }
  }

  //
  // try_others - tries to come up with possible suggestions
  //
  
  void Working::try_others () {

    const char *replace_list = lang->soundslike_chars();

    const String & word       = original_word.word;
    const String & soundslike = original_word.soundslike;
    
    String::size_type i;
    
    String new_soundslike;
    new_soundslike.reserve(soundslike.size() + 1);

    char a,b;
    const char * c;

    // Insert a space or hyphone

    if (word.size() >= 4) {

      char * new_word = new char[word.size() + 2];
      strncpy(new_word, word.data(), word.size());
      new_word[word.size() + 1] = '\0';
      new_word[word.size() + 0] = new_word[word.size() - 1];

      for (i = word.size() - 2; i >= 2; --i) {
	new_word[i+1] = new_word[i];
	new_word[i] = '\0';
	
	if (manager->check(new_word) && manager->check(new_word + i + 1)) {
	  new_word[i] = ' ';
	  add_nearmiss(new_word, parms.edit_distance_weights.del2,
		       dont_count, do_need_alloc);

	  new_word[i] = '-';
	  add_nearmiss(new_word, parms.edit_distance_weights.del2,
		       dont_count, do_need_alloc);
	}
      }
      
      delete[] new_word;
    }

    if (parms.soundslike_level == 1) {
      
      try_sound(soundslike.c_str(), 0);

      // Change one letter
      
      new_soundslike = soundslike;

      for (i = 0; i != soundslike.size(); ++i) {
	for (c=replace_list; *c; ++c) {
	  if (*c == soundslike[i]) continue;
	  new_soundslike[i] = *c;
	  try_sound(new_soundslike.c_str(),parms.edit_distance_weights.sub);
	}
	new_soundslike[i] = soundslike[i];
      }

      // Interchange two adjacent letters.

      for (i = 0; i+1 != soundslike.size(); ++i) {
	a = new_soundslike[i];
	b = new_soundslike[i+1];
	new_soundslike[i] = b;
	new_soundslike[i+1] = a;
	try_sound(new_soundslike.c_str(),parms.edit_distance_weights.swap);
	new_soundslike[i] = a;
	new_soundslike[i+1] = b;
      }

      // Add one letter

      new_soundslike += ' ';
      i = new_soundslike.size()-1;
      while(true) {
	for (c=replace_list; *c; ++c) {
	  new_soundslike[i] = *c;
	  try_sound(new_soundslike.c_str(),parms.edit_distance_weights.del1);
	}
	if (i == 0) break;
	new_soundslike[i] = new_soundslike[i-1];
	--i;
      }
    
      // Delete one letter

      if (soundslike.size() > 1) {
	new_soundslike = soundslike;
	a = new_soundslike[new_soundslike.size() - 1];
	new_soundslike.resize(new_soundslike.size() - 1);
	i = new_soundslike.size();
	while (true) {
	  try_sound(new_soundslike.c_str(),parms.edit_distance_weights.del2);
	  if (i == 0) break;
	  b = a;
	  a = new_soundslike[i-1];
	  new_soundslike[i-1] = b;
	  --i;
	}
      }

    } else {

      const char * original_soundslike = original_word.soundslike.c_str();

      for (ManagerImpl::DataSetCollection::const_iterator i 
	     = manager->data_set_collection().begin();
	   i != manager->data_set_collection().end();
	   ++i) {

	if (!i->use_to_suggest) continue;
      
	if (i->data_set->basic_type == DataSet::basic_word_set) {

	  const BasicWordSet * data_set 
	    = static_cast<const BasicWordSet *>(i->data_set);

	  BasicWordSet::SoundslikeEmul els = data_set->soundslike_elements();
    
	  SoundslikeWord sw;
	  while ( (sw = els.next()) == true) {

	    int score = limit2_edit_distance(original_soundslike, 
					     sw.soundslike,
					     parms.edit_distance_weights);
	    if (score < LARGE_NUM) {
	      BasicWordSet::Emul e = data_set->words_w_soundslike(sw);
	      BasicWordInfo bw;
	      String word;
	      while ((bw = e.next())) {
		bw.get_word(word, i->local_info.convert);
		add_nearmiss(word.c_str(), score, do_count, do_need_alloc);
	      }
	    }
	  }

	} else {
	
	  const BasicReplacementSet * repl_set
	    = static_cast<const BasicReplacementSet *>(i->data_set);

	  BasicWordSet::SoundslikeEmul els = repl_set->soundslike_elements();
	
	  SoundslikeWord w;
	  while ( (w = els.next()) == true) {
	    int score = limit2_edit_distance(original_soundslike, 
					     w.soundslike,
					     parms.edit_distance_weights);
	  
	    if (score < LARGE_NUM) {
	      BasicReplacementSet::Emul e = repl_set->repls_w_soundslike(w);
	      ReplacementList repl;
	      while (! (repl = e.next()).empty() )
		add_nearmiss(repl.misspelled_word, score, 
			     dont_count, dont_need_alloc, repl.elements);
	    }
	  
	  }
	
	}
      }
    }
  }

  void Working::score_list() {
    if (near_misses.empty()) return;

    bool no_soundslike = strcmp(manager->lang().soundslike_name(), "none") != 0;

    if (parms.use_typo_analysis) {
      
      parms.set_original_word_size(original_word.word.size());
      
      NearMisses::iterator i;
      int word_score;
      
      unsigned int j;
      std::vector<unsigned char> original(original_word.word.size() + 1);
      for (j = 0; j != original_word.word.size(); ++j)
	original[j] = lang->to_normalized(original_word.word[j]);
      original[j] = 0;
      std::vector<unsigned char> word(max_word_length + 1);
      
      for (i = near_misses.begin(); i != near_misses.end(); ++i) {
	for (j = 0; (i->word)[j] != 0; ++j)
	  word[j] = lang->to_normalized((i->word)[j]);
	word[j] = 0;
	word_score = typo_edit_distance(&*word.begin(), &*original.begin(),
					parms.typo_edit_distance_weights);
	i->score = weighted_average(i->soundslike_score, word_score);
      }
      near_misses.swap(scored_near_misses);
      scored_near_misses.sort();
      
      i = scored_near_misses.begin();
      
      if (i == scored_near_misses.end()) return;
      
      skip_first_couple(i);
      
      threshold = i->score + parms.span;
      if (threshold < parms.edit_distance_weights.max)
	threshold = parms.edit_distance_weights.max;
      
    } else {
	
      parms.set_original_word_size(original_word.word.size());
      
      NearMisses::iterator i;
      NearMisses::iterator prev;
      int word_score;
      
      near_misses.push_front(ScoreWordSound());
      // the first item will NEVER be looked at.
      scored_near_misses.push_front(ScoreWordSound());
      scored_near_misses.front().score = -1;
      // this item will only be looked at when sorting so 
      // make it a small value to keep it at the front.

      int try_for = (parms.word_weight*parms.edit_distance_weights.max)/100;
      while (true) {
	try_for += (parms.word_weight*parms.edit_distance_weights.max)/100;
	
	// put all pairs whose score <= initial_limit*max_weight
	// into the scored list

	prev = near_misses.begin();
	i = prev;
	++i;
	while (i != near_misses.end()) {

	  int level = needed_level(try_for, i->soundslike_score);
	
	  if (no_soundslike)
	    word_score = i->soundslike_score;
	  else if (level >= int(i->soundslike_score/parms.edit_distance_weights.min))
	    word_score = edit_distance(original_word.word_stripped.c_str(),
				       i->word_stripped,
				       level, level,
				       parms.edit_distance_weights);
	  else
	    word_score = LARGE_NUM;
	  
	  if (word_score < LARGE_NUM) {
	    i->score = weighted_average(i->soundslike_score, word_score);
	    
	    scored_near_misses.splice_into(near_misses,prev,i);
	    
	    i = prev; // Yes this is right due to the slice
	    ++i;
	    
	  } else {
	    
	    prev = i;
	    ++i;
	    
	  }
	}
	
	scored_near_misses.sort();
	
	i = scored_near_misses.begin();
	++i;
	
	if (i == scored_near_misses.end()) continue;
	
	int k = skip_first_couple(i);
	
	if ((k == parms.skip && i->score <= try_for) 
	    || prev == near_misses.begin() ) // or no more left in near_misses
	  break;
      }
      
      threshold = i->score + parms.span;
      if (threshold < parms.edit_distance_weights.max)
	threshold = parms.edit_distance_weights.max;

#  ifdef DEBUG_SUGGEST
      cout << "Threshold is: " << threshold << endl;
      cout << "try_for: " << try_for << endl;
      cout << "Size of scored: " << scored_near_misses.size() << endl;
      cout << "Size of ! scored: " << near_misses.size() << endl;
#  endif
      
      //if (threshold - try_for <=  parms.edit_distance_weights.max/2) return;
      
      prev = near_misses.begin();
      i = prev;
      ++i;
      while (i != near_misses.end()) {
	
	int initial_level = needed_level(try_for, i->soundslike_score);
	int max_level = needed_level(threshold, i->soundslike_score);
	
	if (no_soundslike)
	  word_score = i->soundslike_score;
	else if (initial_level < max_level)
	  word_score = edit_distance(original_word.word_stripped.c_str(),
				     i->word_stripped,
				     initial_level+1,max_level,
				     parms.edit_distance_weights);
	else
	  word_score = LARGE_NUM;
	
	if (word_score < LARGE_NUM) {
	  i->score = weighted_average(i->soundslike_score, word_score);
	  
	  scored_near_misses.splice_into(near_misses,prev,i);
	  
	  i = prev; // Yes this is right due to the slice
	  ++i;
	  
	} else {
	  
	  prev = i;
	  ++i;

	}
      }
      
      scored_near_misses.sort();
      scored_near_misses.pop_front();
    }
  }

  void Working::transfer() {

#  ifdef DEBUG_SUGGEST
    cout << endl << endl 
	 << original_word.word << '\t' 
	 << original_word.soundslike << '\t'
	 << endl;
#  endif
    int c = 1;
    hash_set<String,HashString<String> > duplicates_check;
    String final_word;
    pair<hash_set<String,HashString<String> >::iterator, bool> dup_pair;
    for (NearMisses::const_iterator i = scored_near_misses.begin();
	 i != scored_near_misses.end() && c <= parms.limit
	   && ( i->score <= threshold || c <= 3 );
	 ++i, ++c) {
#    ifdef DEBUG_SUGGEST
      cout << i->word << '\t' << i->score 
           << '\t' << lang->to_soundslike(i->word) << endl;
#    endif
      if (i->repl_list != 0) {
	const char * word;
	string::size_type pos;
	while((word = i->repl_list->next()) != 0) {
	  dup_pair = duplicates_check.insert(fix_case(word));
	  if (dup_pair.second && 
	      ((pos = dup_pair.first->find(' '), pos == String::npos)
	       ? (bool)manager->check(*dup_pair.first)
	       : (manager->check((String)dup_pair.first->substr(0,pos)) 
		  && manager->check((String)dup_pair.first->substr(pos+1))) ))
	    near_misses_final->push_back(*dup_pair.first);
	}
      } else {
	dup_pair = duplicates_check.insert(fix_case(i->word));
	if (dup_pair.second )
	  near_misses_final->push_back(*dup_pair.first);
      }
    }
  }
  
  void Working::get_suggestions(NearMissesFinal & sug) {
    near_misses_final = & sug;
    if (original_word.soundslike.empty()) return;
    try_others();
    score_list();
    transfer();
  }
  
  class SuggestionListImpl : public SuggestionList {
    struct Parms {
      typedef const char *                    Value;
      typedef NearMissesFinal::const_iterator Iterator;
      Iterator end;
      Parms(Iterator e) : end(e) {}
      bool endf(Iterator e) const {return e == end;}
      Value end_state() const {return 0;}
      Value deref(Iterator i) const {return i->c_str();}
    };
  public:
    NearMissesFinal suggestions;

    SuggestionList * clone() const {return new SuggestionListImpl(*this);}
    void assign(const SuggestionList * other) {
      *this = *static_cast<const SuggestionListImpl *>(other);
    }

    bool empty() const { return suggestions.empty(); }
    Size size() const { return suggestions.size(); }
    VirEmul * elements() const {
      return new MakeVirEmulation<Parms, StringEmulation>
	(suggestions.begin(), Parms(suggestions.end()));
    }
  };

  class SuggestImpl : public Suggest {
    ManagerImpl * manager_;
    SuggestionListImpl  suggestion_list;
    SuggestParms parms_;
  public:
    SuggestImpl(ManagerImpl * m) 
      : manager_(m), parms_(m->config()->retrieve("sug-mode")) 
    {parms_.fill_distance_lookup(m->config(), m->lang());}
    SuggestImpl(ManagerImpl * m, const SuggestParms & p) 
      : manager_(m), parms_(p) 
    {parms_.fill_distance_lookup(m->config(), m->lang());}
    void set_mode(ParmString mode) {
      parms_.set(mode);
    }
    double score(const char *base, const char *other) {
      //parms_.set_original_word_size(strlen(base));
      //Score s(&manager_->lang(),base,parms_);
      //string sl = manager_->lang().to_soundslike(other);
      //ScoreWordSound sws(other, sl.c_str());
      //s.score(sws);
      //return sws.score;
      return -1;
    }
    SuggestionList & suggest(const char * word);
  };

  SuggestionList & SuggestImpl::suggest(const char * word) { 
#   ifdef DEBUG_SUGGEST
    cout << "=========== begin suggest " << word << " ===========\n";
#   endif
    parms_.set_original_word_size(strlen(word));
    suggestion_list.suggestions.resize(0);
    Working sug(manager_, &manager_->lang(),word,parms_);
    sug.get_suggestions(suggestion_list.suggestions);
#   ifdef DEBUG_SUGGEST
    cout << "^^^^^^^^^^^  end suggest " << word << "  ^^^^^^^^^^^\n";
#   endif
    return suggestion_list;
  }
  
}

namespace aspell {
  Suggest * new_default_suggest(ManagerImpl * m) {
    return new aspell_default_suggest::SuggestImpl(m);
  }

  Suggest * new_default_suggest(ManagerImpl * m, const SuggestParms & p) {
    return new aspell_default_suggest::SuggestImpl(m,p);
  }

  PosibErr<void> SuggestParms::set(ParmString mode) {

    if (mode != "normal" && mode != "fast" && mode != "ultra" && mode != "bad-spellers")
      return make_err(bad_value, "sug-mode", mode, "one of ultra, fast, normal, or bad-spellers");

    edit_distance_weights.del1 =  95;
    edit_distance_weights.del2 =  95;
    edit_distance_weights.swap =  90;
    edit_distance_weights.sub =  100;
    edit_distance_weights.similar = 10;
    edit_distance_weights.max = 100;
    edit_distance_weights.min =  90;

    normal_soundslike_weight = 50;
    small_word_soundslike_weight = 15;
    small_word_threshold = 4;

    soundslike_weight = normal_soundslike_weight;
    word_weight       = 100 - normal_soundslike_weight;
      
    skip = 2;
    limit = 100;
    if (mode == "normal") {
      use_typo_analysis = true;
      soundslike_level = 2; // either one or two
      span = 50;
    } else if (mode == "fast") {
      use_typo_analysis = true;
      soundslike_level = 1; // either one or two
      span = 50;
    } else if (mode == "ultra") {
      use_typo_analysis = false;
      soundslike_level = 1; // either one or two
      span = 50;
    } else if (mode == "bad-spellers") {
      use_typo_analysis = false;
      normal_soundslike_weight = 55;
      small_word_threshold = 0;
      soundslike_level = 2; // either one or two
      span = 125;
      limit = 1000;
    } else {
      abort(); // this should NEVER happen.
    }

    return no_err;
  }

  PosibErr<void> SuggestParms::fill_distance_lookup(const Config * c, const Language & l) {
    
    TypoEditDistanceWeights & w = typo_edit_distance_weights;

    String keyboard = c->retrieve("keyboard");

    if (keyboard == "none") {
      
      use_typo_analysis = false;
      
    } else {

      FStream in;
      String file, dir1, dir2;
      fill_data_dir(c, dir1, dir2);
      find_file(file, dir1, dir2, keyboard, ".kbd");
      RET_ON_ERR(in.open(file.c_str(), "r"));

      int c = l.max_normalized() + 1;
      w.repl .init(c);
      w.extra.init(c);

      for (int i = 0; i != c; ++i) {
	for (int j = 0; j != c; ++j) {
	  w.repl (i,j) = w.repl_dis2;
	  w.extra(i,j) = w.extra_dis2;
	}
      }

      String key, data;
      while (getdata_pair(in, key, data)) {
	if (key.size() != 2) 
	  return make_err(bad_file_format, file);
	w.repl (l.to_normalized(key[0]),
		l.to_normalized(key[1])) = w.repl_dis1;
	w.repl (l.to_normalized(key[1]),
		l.to_normalized(key[0])) = w.repl_dis1;
	w.extra(l.to_normalized(key[0]),
		l.to_normalized(key[1])) = w.extra_dis1;
	w.extra(l.to_normalized(key[1]),
		l.to_normalized(key[0])) = w.extra_dis1;
      }

      for (int i = 0; i != c; ++i) {
	w.repl(i,i) = 0;
	w.extra(i,i) = w.extra_dis1;
      }
      
    }
    return no_err;
  }
  
    
  SuggestParms * SuggestParms::clone() const {
    return new SuggestParms(*this);
  }

  void SuggestParms::set_original_word_size(int size) {
    if (size <= small_word_threshold) {
      soundslike_weight = small_word_soundslike_weight;
    } else {
      soundslike_weight = normal_soundslike_weight;
    }
    word_weight = 100 - soundslike_weight;
  }
}
