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

// FIXME: OPTIMIZATIONS
//   store the number of letters that are the same as the previous 
//     soundslike so that it can possible be skipped
//   don't expand every affix immendently as the whole entry may be
//     able to be skipped based on the scan of the root word.
//     this will require storing the maxim number of words stripped
//     with each entry

#include "getdata.hpp"

#include "fstream.hpp"

#include "aspeller.hpp"
#include "asuggest.hpp"
#include "basic_list.hpp"
#include "clone_ptr-t.hpp"
#include "config.hpp"
#include "data.hpp"
#include "editdist.hpp"
#include "editdist2.hpp"
#include "errors.hpp"
#include "file_data_util.hpp"
#include "hash-t.hpp"
#include "language.hpp"
#include "leditdist.hpp"
#include "speller_impl.hpp"
#include "stack_ptr.hpp"
#include "suggest.hpp"

#include "iostream.hpp"
//#define DEBUG_SUGGEST

using namespace aspeller;
using namespace acommon;
using namespace std;

namespace {

  typedef vector<String> NearMissesFinal;

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
    String   stripped;
    String   soundslike;
    CasePattern  case_pattern;
    OriginalWord() {}
    OriginalWord (const String &w, const String &sl)
      : word(w), soundslike(sl) {}
    OriginalWord (const String &w, const String &sl,
		 const String &l, CasePattern cp)
      : word(w), stripped(l), soundslike(sl), case_pattern(cp) {}
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
    WordEntry * repl_list;
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
    OriginalWord     original_word;
    SuggestParms     parms;

  public:
    Score(const Language *l, const String &w, const SuggestParms & p)
      : lang(l), original_word(w, l->to_soundslike(w.c_str()), 
			       to_stripped(*l, w),
			       case_pattern(*l, w)),
      parms(p)
    {}
    String fix_case(const String & word) {
      return aspeller::fix_case(*lang,original_word.case_pattern,word);
    }
  };

  class Working : public Score {
   
    int threshold;

    unsigned int max_word_length;

    SpellerImpl  *     speller;
    NearMisses         scored_near_misses;
    NearMisses         near_misses;
    NearMissesFinal  * near_misses_final;

    ObjStack           buffer;

    bool use_soundslike, fast_scan, fast_lookup, affix_compress_soundslike;

    static const bool do_count = true;
    static const bool dont_count = false;

    const String & active_soundslike() {
      return use_soundslike ? original_word.soundslike : original_word.stripped;
    };
    const char * soundslike_chars() {
      return use_soundslike ? lang->soundslike_chars() : lang->stripped_chars();
    }

    void try_sound(ParmString, int score);
    void add_nearmiss(ParmString word, int score, bool count, WordEntry * rl = 0)
    {
      near_misses.push_front(ScoreWordSound());
      ScoreWordSound & d = near_misses.front();
      d.word = word;

      if (parms.use_typo_analysis) {
	unsigned int l = word.size();
	if (l > max_word_length) max_word_length = l;
      }

      if (!is_stripped(*lang,word)) { // FIXME: avoid the need for this test
        d.word_stripped = (char *)buffer.alloc(word.size() + 1);
        to_stripped(*lang, word, (char *)d.word_stripped);
      } else {
	d.word_stripped = d.word;
      }

      d.soundslike_score = score;
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
    void try_split();
    void try_one_edit();
    void try_scan();
    void try_repl();

    void score_list();
    void transfer();
  public:
    Working(SpellerImpl * m, const Language *l,
	    const String & w, const SuggestParms & p)
      : Score(l,w,p), threshold(1), max_word_length(0), speller(m) ,
        use_soundslike(m->use_soundslike),
        fast_scan(m->fast_scan), fast_lookup(m->fast_lookup),
	affix_compress_soundslike(!m->suggest_affix_ws.empty()) {}
    void get_suggestions(NearMissesFinal &sug);
  };

  //
  // try_sound - tries the soundslike string if there is a match add 
  //    the possable words to near_misses
  //

  void Working::get_suggestions(NearMissesFinal & sug) {
    near_misses_final = & sug;
    if (active_soundslike().empty()) return;
    try_others();
    score_list();
    transfer();
  }
  
  void Working::try_sound(ParmString str, int score)  
  {
    String word;
    WordEntry sw;
    for (SpellerImpl::WS::const_iterator i = speller->suggest_ws.begin();
         i != speller->suggest_ws.end();
         ++i)
    {
      i->ws->soundslike_lookup(str, sw);
      for (;!sw.at_end(); sw.adv()) {
        ParmString sw_word(sw.word);
        char * w = (char *)buffer.alloc(sw_word.size() + 1);
        i->convert.convert(sw_word, w);
        WordEntry * repl = 0;
        if (sw.what == WordEntry::Misspelled) {
          repl = new WordEntry;
          const BasicReplacementSet * repl_set
            = static_cast<const BasicReplacementSet *>(i->ws);
          repl_set->repl_lookup(sw, *repl);
        }
        add_nearmiss(ParmString(w, sw_word.size()), score, do_count, repl);
      }
    }
    if (affix_compress_soundslike) {
      CheckInfo ci; memset(&ci, 0, sizeof(ci));
      bool res = lang->affix()->affix_check(LookupInfo(speller, LookupInfo::Soundslike), str, ci, 0);
      if (!res) return;
      
      // FIXME: This is not completely correct when there are multiple
      //   words for a single stripped word.
      for (SpellerImpl::WS::const_iterator i = speller->suggest_affix_ws.begin();
	   i != speller->suggest_affix_ws.end();
	   ++i) 
      {
	i->ws->soundslike_lookup(ci.word, sw);
	for (;!sw.at_end(); sw.adv()) { // FIXME: Ineffecent
	  word.clear();
	  i->convert.convert(sw.word, word);
	  lang->affix()->get_word(word, ci); 
	  add_nearmiss(buffer.dup(word), score, do_count);
	}
      }
    }
  }

  //
  // try_others - tries to come up with possible suggestions
  //

  void Working::try_others () {

    try_split();

    if (parms.soundslike_level == 1 && (!fast_scan || !use_soundslike))
      try_one_edit();
    else
      try_scan();
    
    if (!use_soundslike && parms.use_repl_table)
      try_repl();

    //try_ngram();

  }

  void Working::try_split() {
    const String & word       = original_word.word;
    
    if (word.size() < 4 || parms.split_chars.empty()) return;
    size_t i = 0;
    
    char * new_word = new char[word.size() + 2];
    strncpy(new_word, word.data(), word.size());
    new_word[word.size() + 1] = '\0';
    new_word[word.size() + 0] = new_word[word.size() - 1];
    
    for (i = word.size() - 2; i >= 2; --i) {
      new_word[i+1] = new_word[i];
      new_word[i] = '\0';
      
      if (speller->check(new_word) && speller->check(new_word + i + 1)) {
        for (size_t j = 0; j != parms.split_chars.size(); ++j)
        {
          new_word[i] = parms.split_chars[j];
          add_nearmiss(buffer.dup(new_word), 
                       parms.edit_distance_weights.del2*3/2,
                       dont_count);
        }
      }
    }
    
    delete[] new_word;
  }

  void Working::try_one_edit() 
  {
    const String & soundslike = active_soundslike();
    const char * replace_list = soundslike_chars();
    char a,b;
    const char * c;
    String new_soundslike;
    size_t i;

    // First try the soundslike as is

    try_sound(soundslike, 0);

    // Change one letter
    
    new_soundslike = soundslike;
    
    for (i = 0; i != soundslike.size(); ++i) {
      for (c = replace_list; *c; ++c) {
        if (*c == soundslike[i]) continue;
        new_soundslike[i] = *c;
        try_sound(new_soundslike, parms.edit_distance_weights.sub);
      }
      new_soundslike[i] = soundslike[i];
    }
    
    // Interchange two adjacent letters.
    
    for (i = 0; i+1 != soundslike.size(); ++i) {
      a = new_soundslike[i];
      b = new_soundslike[i+1];
      new_soundslike[i] = b;
      new_soundslike[i+1] = a;
      try_sound(new_soundslike,parms.edit_distance_weights.swap);
      new_soundslike[i] = a;
      new_soundslike[i+1] = b;
    }

    // Add one letter

    new_soundslike += ' ';
    i = new_soundslike.size()-1;
    while(true) {
      for (c=replace_list; *c; ++c) {
        new_soundslike[i] = *c;
        try_sound(new_soundslike,parms.edit_distance_weights.del1);
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
        try_sound(new_soundslike,parms.edit_distance_weights.del2);
        if (i == 0) break;
        b = a;
        a = new_soundslike[i-1];
        new_soundslike[i-1] = b;
        --i;
      }
    }
  }

  void Working::try_scan() 
  {
    const char * original_soundslike = active_soundslike().c_str();
    //unsigned int original_soundslike_len = strlen(original_soundslike);
    
    EditDist (* edit_dist_fun)(const char *, const char *, 
                               const EditDistanceWeights &);
    
    if (parms.soundslike_level == 1)
      edit_dist_fun = limit1_edit_distance;
    else
      edit_dist_fun = limit2_edit_distance;

    WordEntry * sw;
    WordEntry w;
    const char * sl = 0;
    String sl_buf;
    EditDist score;
    unsigned int stopped_at = LARGE_NUM;
    CheckList cl;
    const CheckInfo * ci_cur;
    
    for (SpellerImpl::WS::const_iterator i = speller->suggest_ws.begin();
         i != speller->suggest_ws.end();
         ++i) 
    {
      StackPtr<SoundslikeEnumeration> els(i->ws->soundslike_elements());
      
      while ( (sw = els->next(stopped_at)) ) {

        if (sw->what != WordEntry::Word) {
          sl = sw->word;
        } else if (!*sw->aff) {
          sl_buf.clear();
          to_stripped(*lang, sw->word, sl_buf);
          sl = sl_buf.c_str();
        } else {
          goto affix_case;
        }

        score = edit_dist_fun(sl, original_soundslike, parms.edit_distance_weights);
        stopped_at = score.stopped_at - sl;
        if (score >= LARGE_NUM) continue;
        stopped_at = LARGE_NUM;
        i->ws->soundslike_lookup(*sw, w);
	//CERR << sw->word << "\n";
        for (; !w.at_end(); w.adv()) {
	  //CERR << "  " << w.word << "\n";
          ParmString w_word(w.word);
          char * wf = (char *)buffer.alloc(w_word.size() + 1);
          i->convert.convert(w_word, wf);
          WordEntry * repl = 0;
          if (w.what == WordEntry::Misspelled) {
            repl = new WordEntry;
            const BasicReplacementSet * repl_set
              = static_cast<const BasicReplacementSet *>(i->ws);
            repl_set->repl_lookup(w, *repl);
          }
          add_nearmiss(ParmString(wf, w_word.size()), score, do_count, repl);
        }
        continue;

      affix_case:

        lang->affix()->expand(sw->word, sw->aff, &cl);
        ci_cur = cl.data + 1;
        for (;ci_cur; ci_cur = ci_cur->next) {
          sl_buf.clear();
          to_stripped(*lang, ci_cur->word, sl_buf);
          score = edit_dist_fun(sl_buf.c_str(), original_soundslike, parms.edit_distance_weights);
          stopped_at = score.stopped_at - sl;
          if (score >= LARGE_NUM) continue;
          stopped_at = LARGE_NUM;
          ParmString w_word(ci_cur->word);
          char * wf = (char *)buffer.alloc(w_word.size() + 1);
          i->convert.convert(w_word, wf);
          add_nearmiss(ParmString(wf, w_word.size()), score, do_count);
        }
      }
    }
  }

  struct ReplTry 
  {
    const char * begin;
    const char * end;
    const char * repl;
    size_t repl_len;
    ReplTry(const char * b, const char * e, const char * r)
      : begin(b), end(e), repl(r), repl_len(strlen(r)) {}
  };

  // only use when soundslike == stripped
  void Working::try_repl() 
  {
    CharVector buf;
    Vector<ReplTry> repl_try;
    StackPtr<SuggestReplEnumeration> els(lang->repl());
    const SuggestRepl * r = 0;
    const char * word = original_word.stripped.c_str();
    const char * wend = word + original_word.stripped.size();
    while (r = els->next(), r) 
    {
      const char * p = word;
      while ((p = strstr(p, r->substr))) {
        buf.clear();
        buf.append(word, p);
        buf.append(r->repl, strlen(r->repl));
        p += strlen(r->substr);
        buf.append(p, wend + 1);
        try_sound(buf.data(), parms.edit_distance_weights.sub*3/2);
      }
    }
  }

  
  void Working::score_list() {
    if (near_misses.empty()) return;

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
	
	if (!use_soundslike)
	  word_score = i->soundslike_score;
	else if (level >= int(i->soundslike_score/parms.edit_distance_weights.min))
	  word_score = edit_distance(original_word.stripped.c_str(),
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
    COUT << "Threshold is: " << threshold << "\n";
    COUT << "try_for: " << try_for << "\n";
    COUT << "Size of scored: " << scored_near_misses.size() << "\n";
    COUT << "Size of ! scored: " << near_misses.size() << "\n";
#  endif
      
    //if (threshold - try_for <=  parms.edit_distance_weights.max/2) return;
      
    prev = near_misses.begin();
    i = prev;
    ++i;
    while (i != near_misses.end()) {
	
      int initial_level = needed_level(try_for, i->soundslike_score);
      int max_level = needed_level(threshold, i->soundslike_score);
	
      if (!use_soundslike)
	word_score = i->soundslike_score;
      else if (initial_level < max_level)
	word_score = edit_distance(original_word.stripped.c_str(),
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
    
    if (parms.use_typo_analysis) {
      int max = 0;
      unsigned int j;

      CharVector original, word;
      original.resize(original_word.word.size() + 1);
      for (j = 0; j != original_word.word.size(); ++j)
          original[j] = lang->to_normalized(original_word.word[j]);
      original[j] = 0;
      word.resize(max_word_length + 1);
      
      for (i = scored_near_misses.begin();
	   i != scored_near_misses.end() && i->score <= threshold;
	   ++i)
      {
	for (j = 0; (i->word)[j] != 0; ++j)
	  word[j] = lang->to_normalized((i->word)[j]);
	word[j] = 0;
	int word_score 
	  = typo_edit_distance(word.data(), original.data(),
			       parms.typo_edit_distance_weights);
	i->score = weighted_average(i->soundslike_score, word_score);
	if (max < i->score) max = i->score;
      }
      threshold = max;
      for (;i != scored_near_misses.end() && i->score <= threshold; ++i)
	i->score = threshold + 1;

      scored_near_misses.sort();
    }
  }

  void Working::transfer() {

#  ifdef DEBUG_SUGGEST
    COUT << "\n" << "\n" 
	 << original_word.word << '\t' 
	 << original_word.soundslike << '\t'
	 << "\n";
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
      COUT << i->word << '\t' << i->score 
           << '\t' << lang->to_soundslike(i->word) << "\n";
#    endif
      if (i->repl_list != 0) {
 	string::size_type pos;
	do {
 	  dup_pair = duplicates_check.insert(fix_case(i->repl_list->word));
 	  if (dup_pair.second && 
 	      ((pos = dup_pair.first->find(' '), pos == String::npos)
 	       ? (bool)speller->check(*dup_pair.first)
 	       : (speller->check((String)dup_pair.first->substr(0,pos)) 
 		  && speller->check((String)dup_pair.first->substr(pos+1))) ))
 	    near_misses_final->push_back(*dup_pair.first);
 	} while (i->repl_list->adv());
      } else {
	dup_pair = duplicates_check.insert(fix_case(i->word));
	if (dup_pair.second )
	  near_misses_final->push_back(*dup_pair.first);
      }
    }
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
      return new MakeEnumeration<Parms, StringEnumeration>
	(suggestions.begin(), Parms(suggestions.end()));
    }
  };

  class SuggestImpl : public Suggest {
    SpellerImpl * speller_;
    SuggestionListImpl  suggestion_list;
    SuggestParms parms_;
  public:
    SuggestImpl(SpellerImpl * m);
    //SuggestImpl(SpellerImpl * m, const SuggestParms & p)
    //  : speller_(m), parms_(p) 
    //{parms_.fill_distance_lookup(m->config(), m->lang());}
    PosibErr<void> set_mode(ParmString mode) {
      return parms_.set(mode);
    }
    double score(const char *base, const char *other) {
      //parms_.set_original_word_size(strlen(base));
      //Score s(&speller_->lang(),base,parms_);
      //string sl = speller_->lang().to_soundslike(other);
      //ScoreWordSound sws(other, sl.c_str());
      //s.score(sws);
      //return sws.score;
      return -1;
    }
    SuggestionList & suggest(const char * word);
  };
  
  SuggestImpl::SuggestImpl(SpellerImpl * m)
    : speller_(m), parms_(m->config()->retrieve("sug-mode")) 
  {
    if (m->config()->retrieve("sug-mode") == "normal" 
        && !m->fast_scan) parms_.soundslike_level = 1;

    if (m->config()->have("sug-edit-dist"))
      parms_.soundslike_level = m->config()->retrieve_int("sug-edit-dist"); // FIXME: Make sure 1 or 2
    if (m->config()->have("sug-typo-analysis"))
      parms_.use_typo_analysis = m->config()->retrieve_bool("sug-typo-analysis");
    if (m->config()->have("sug-repl-table"))
      parms_.use_repl_table = m->config()->retrieve_bool("sug-repl-table");
    
    parms_.split_chars = m->config()->retrieve("sug-split-chars");

    parms_.fill_distance_lookup(m->config(), m->lang());
  }

  SuggestionList & SuggestImpl::suggest(const char * word) { 
#   ifdef DEBUG_SUGGEST
    COUT << "=========== begin suggest " << word << " ===========\n";
#   endif
    parms_.set_original_word_size(strlen(word));
    suggestion_list.suggestions.resize(0);
    Working sug(speller_, &speller_->lang(),word,parms_);
    sug.get_suggestions(suggestion_list.suggestions);
#   ifdef DEBUG_SUGGEST
    COUT << "^^^^^^^^^^^  end suggest " << word << "  ^^^^^^^^^^^\n";
#   endif
    return suggestion_list;
  }
  
}

namespace aspeller {
  Suggest * new_default_suggest(SpellerImpl * m) {
    return new SuggestImpl(m);
  }

  //Suggest * new_default_suggest(SpellerImpl * m, const SuggestParms & p) {
  //  return new aspeller_default_suggest::SuggestImpl(m,p);
  //}

  PosibErr<void> SuggestParms::set(ParmString mode) {

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

    split_chars = " -";

    skip = 2;
    limit = 100;
    if (mode == "normal") {
      use_typo_analysis = true;
      use_repl_table = true;
      soundslike_level = 2; // either one or two
      span = 50;
    } else if (mode == "slow") {
      use_typo_analysis = true;
      use_repl_table = true;
      soundslike_level = 2; // either one or two
      span = 50;
    } else if (mode == "fast") {
      use_typo_analysis = true;
      use_repl_table = true;
      soundslike_level = 1; // either one or two
      span = 50;
    } else if (mode == "ultra") {
      use_typo_analysis = false;
      use_repl_table = false;
      soundslike_level = 1; // either one or two
      span = 50;
    } else if (mode == "bad-spellers") {
      use_typo_analysis = false;
      use_repl_table = true;
      normal_soundslike_weight = 55;
      small_word_threshold = 0;
      soundslike_level = 2; // either one or two
      span = 125;
      limit = 1000;
    } else {
      return make_err(bad_value, "sug-mode", mode, "one of ultra, fast, normal, slow, or bad-spellers");
    }

    return no_err;
  }

  PosibErr<void> SuggestParms::fill_distance_lookup(const Config * c, const Language & l) {

    // FIXME: avoid having to recreate the tables each time
    //        instead, cache the table and use the cached copy
    
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

      FixedBuffer<64> buf;
      DataPair d;
      while (getdata_pair(in, d, buf)) {
	if (d.key.size() != 2)
	  return make_err(bad_file_format, file);
	w.repl (l.to_normalized(d.key[0]),
		l.to_normalized(d.key[1])) = w.repl_dis1;
	w.repl (l.to_normalized(d.key[1]),
		l.to_normalized(d.key[0])) = w.repl_dis1;
	w.extra(l.to_normalized(d.key[0]),
		l.to_normalized(d.key[1])) = w.extra_dis1;
	w.extra(l.to_normalized(d.key[1]),
		l.to_normalized(d.key[0])) = w.extra_dis1;
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
