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
// FIXME: Improve Suggest
//   after scoring and ranking the suggestions there are not enough
//     words than resort to MySpell ngram
//   also considering first aiming for one edit distance then if that
//     is not enough aim for two.  This strategy should only be used
//     when true soundslike are not used.

#include "getdata.hpp"

#include "fstream.hpp"

#include "speller_impl.hpp"
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

//#include "iostream.hpp"
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
    String   lower;
    String   clean;
    String   soundslike;
    CasePattern  case_pattern;
    OriginalWord() {}
  };

  //
  // struct ScoreWordSound - used for storing the possible words while
  //   they are being processed.
  //

  struct ScoreWordSound {
    char *  word;
    char *  word_clean;
    int           score;
    int           word_score;
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
    const SuggestParms * parms;

  public:
    Score(const Language *l, const String &w, const SuggestParms * p)
      : lang(l), original_word(), parms(p)
    {
      original_word.word = w;
      l->to_lower(original_word.lower, w.str());
      l->to_clean(original_word.clean, w.str());
      l->to_soundslike(original_word.soundslike, w.str());
      original_word.case_pattern = l->case_pattern(w);
    }
    void fix_case(char * str) {
      lang->LangImpl::fix_case(original_word.case_pattern, str, str);
    }
    const char * fix_case(const char * str, String & buf) {
      return lang->LangImpl::fix_case(original_word.case_pattern, str, buf);
    }
  };

  class Working : public Score {
   
    int threshold;

    unsigned int max_word_length;

    SpellerImpl  *     speller;
    NearMisses         scored_near_misses;
    NearMisses         near_misses;
    NearMissesFinal  * near_misses_final;

    String             tmpbuf;
    ObjStack           buffer;

    bool use_soundslike, fast_scan, fast_lookup, affix_compress, affix_compress_soundslike;

    static const bool do_count = true;
    static const bool dont_count = false;

    const String & active_soundslike() {
      return use_soundslike ? original_word.soundslike : original_word.clean;
    }

    void try_word(ParmString str, int score);
    void try_sound(ParmString, int score);
    void add_nearmiss(MutableString word, int w_score, int sl_score, bool count, 
                      WordEntry * rl = 0);
    void add_nearmiss_word(MutableString word, int word_score, bool count, 
                           WordEntry * rl = 0);
    void add_nearmiss(MutableString word, int sl_score, bool count, 
                      WordEntry * rl = 0)
    {
      add_nearmiss(word, -1, sl_score, count, rl);
    }
    int needed_level(int want, int soundslike_score) {
      int n = (100*want - parms->soundslike_weight*soundslike_score)
	/(parms->word_weight*parms->edit_distance_weights.min);
      return n > 0 ? n : 0;
    }
    int weighted_average(int soundslike_score, int word_score) {
      return (parms->word_weight*word_score 
	      + parms->soundslike_weight*soundslike_score)/100;
    }
    int skip_first_couple(NearMisses::iterator & i) {
      int k = 0;
      while (preview_next(i) != scored_near_misses.end()) 
	// skip over the first couple of items as they should
	// not be counted in the threshold score.
      {
	if (!i->count) {
	  ++i;
	} else if (k == parms->skip) {
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
    void try_one_edit_word();
    void try_one_edit_sl();
    void try_scan();
    void try_repl();
    void try_ngram();

    void score_list();
    void transfer();
  public:
    Working(SpellerImpl * m, const Language *l,
	    const String & w, const SuggestParms *  p)
      : Score(l,w,p), threshold(1), max_word_length(0), speller(m) ,
        use_soundslike(m->use_soundslike),
        fast_scan(m->fast_scan), fast_lookup(m->fast_lookup),
        affix_compress(!m->affix_ws.empty()),
	affix_compress_soundslike(!m->suggest_affix_ws.empty()) {}
    void get_suggestions(NearMissesFinal &sug);
  };

  //
  // try_sound - tries the soundslike string if there is a match add 
  //    the possable words to near_misses
  //

  void Working::get_suggestions(NearMissesFinal & sug) {
    near_misses_final = & sug;
    try_others();
    score_list();
    transfer();
  }

  void Working::try_word(ParmString str, int score)  
  {
    String word;
    WordEntry sw;
    for (SpellerImpl::WS::const_iterator i = speller->suggest_ws.begin();
         i != speller->suggest_ws.end();
         ++i)
    {
      i->dict->clean_lookup(str, sw);
      for (;!sw.at_end(); sw.adv()) {
        ParmString sw_word(sw.word);
        char * w = (char *)buffer.alloc(sw_word.size() + 1);
        i->convert.convert(sw_word, w);
        WordEntry * repl = 0;
        if (sw.what == WordEntry::Misspelled) {
          repl = new WordEntry;
          const ReplacementDict * repl_dict
            = static_cast<const ReplacementDict *>(i->dict);
          repl_dict->repl_lookup(sw, *repl);
        }
        add_nearmiss(MutableString(w, sw_word.size()), score, do_count, repl);
      }
    }
    if (affix_compress) {
      CheckInfo ci; memset(&ci, 0, sizeof(ci));
      bool res = lang->affix()->affix_check(LookupInfo(speller, LookupInfo::Clean), str, ci, 0);
      if (!res) return;
      size_t slen = ci.word.size() - ci.pre_strip_len - ci.suf_strip_len;
      size_t wlen = slen + ci.pre_add_len + ci.suf_add_len;
      char * tmp = (char *)buffer.alloc(wlen + 1);
      if (ci.pre_add_len) 
        memcpy(tmp, ci.pre_add, ci.pre_add_len);
      memcpy(tmp + ci.pre_add_len, ci.word.str() + ci.pre_strip_len, slen);
      if (ci.suf_add_len) 
        memcpy(tmp + ci.pre_add_len + slen, ci.suf_add, ci.suf_add_len);
      tmp[wlen] = '\0';
      // no need to convert word as clean lookup is not supported
      // when affixes are involved
      add_nearmiss(MutableString(tmp, wlen), score, do_count);
    }
  }
  
  void Working::try_sound(ParmString str, int score)  
  {
    String word;
    WordEntry sw;
    for (SpellerImpl::WS::const_iterator i = speller->suggest_ws.begin();
         i != speller->suggest_ws.end();
         ++i)
    {
      i->dict->soundslike_lookup(str, sw);
      for (;!sw.at_end(); sw.adv()) {
        ParmString sw_word(sw.word);
        char * w = (char *)buffer.alloc(sw_word.size() + 1);
        i->convert.convert(sw_word, w);
        WordEntry * repl = 0;
        if (sw.what == WordEntry::Misspelled) {
          repl = new WordEntry;
          const ReplacementDict * repl_dict
            = static_cast<const ReplacementDict *>(i->dict);
          repl_dict->repl_lookup(sw, *repl);
        }
        add_nearmiss(MutableString(w, sw_word.size()), score, do_count, repl);
      }
    }
  }
  
  void Working::add_nearmiss(MutableString word, int w_score, int sl_score, 
                             bool count, WordEntry * rl)
  {
    near_misses.push_front(ScoreWordSound());
    ScoreWordSound & d = near_misses.front();
    d.word = word;
    
    if (parms->use_typo_analysis) {
      unsigned int l = word.size;
      if (l > max_word_length) max_word_length = l;
    }
    
    if (!lang->is_clean(word)) { // FIXME: avoid the need for this test
      d.word_clean = (char *)buffer.alloc(word.size + 1);
      lang->LangImpl::to_clean((char *)d.word_clean, word);
    } else {
      d.word_clean = d.word;
    }
    
    d.word_score       = w_score;
    d.soundslike_score = sl_score;
    d.count = count;
    d.repl_list = rl;
  }

  void Working::add_nearmiss_word(MutableString word, int w_score, 
                                  bool count, WordEntry * rl)
  {
    if (use_soundslike) {
      tmpbuf.clear();
      lang->to_soundslike(tmpbuf, word);
      int sl_score = edit_distance(original_word.soundslike, tmpbuf, 
                                   parms->edit_distance_weights);
      add_nearmiss(word, w_score, sl_score, count, rl);
    } else {
      add_nearmiss(word, w_score, w_score, count, rl);
    }
  }

  //
  // try_others - tries to come up with possible suggestions
  //

  void Working::try_others () {

    try_split();

    if (parms->soundslike_level == 1 && (!fast_scan || !use_soundslike))
      try_one_edit_word();
    else
      try_scan();
    
    if (!use_soundslike && parms->use_repl_table)
      try_repl();

    //try_ngram();

  }

  void Working::try_split() {
    const String & word       = original_word.word;
    
    if (word.size() < 4 || parms->split_chars.empty()) return;
    size_t i = 0;
    
    String new_word_str;
    new_word_str.resize(word.size() + 1);
    char * new_word = new_word_str.data();
    memcpy(new_word, word.data(), word.size());
    new_word[word.size() + 1] = '\0';
    new_word[word.size() + 0] = new_word[word.size() - 1];
    
    for (i = word.size() - 2; i >= 2; --i) {
      new_word[i+1] = new_word[i];
      new_word[i] = '\0';
      
      if (speller->check(new_word) && speller->check(new_word + i + 1)) {
        for (size_t j = 0; j != parms->split_chars.size(); ++j)
        {
          new_word[i] = parms->split_chars[j];
          add_nearmiss(buffer.dup(new_word), 
                       parms->edit_distance_weights.del2*3/2,
                       dont_count);
        }
      }
    }
  }

  void Working::try_one_edit_word() 
  {
    const String & original = original_word.clean;
    const char * replace_list = lang->clean_chars();
    char a,b;
    const char * c;
    String new_word;
    size_t i;

    // Try word as is (in case of case difference etc)

    try_word(original, 0);

    // Change one letter
    
    new_word = original;
    
    for (i = 0; i != original.size(); ++i) {
      for (c = replace_list; *c; ++c) {
        if (*c == original[i]) continue;
        new_word[i] = *c;
        try_word(new_word, parms->edit_distance_weights.sub);
      }
      new_word[i] = original[i];
    }
    
    // Interchange two adjacent letters.
    
    for (i = 0; i+1 != original.size(); ++i) {
      a = new_word[i];
      b = new_word[i+1];
      new_word[i] = b;
      new_word[i+1] = a;
      try_word(new_word,parms->edit_distance_weights.swap);
      new_word[i] = a;
      new_word[i+1] = b;
    }

    // Add one letter

    new_word += ' ';
    i = new_word.size()-1;
    while(true) {
      for (c=replace_list; *c; ++c) {
        new_word[i] = *c;
        try_word(new_word,parms->edit_distance_weights.del1);
      }
      if (i == 0) break;
      new_word[i] = new_word[i-1];
      --i;
    }
    
    // Delete one letter

    if (original.size() > 1) {
      new_word = original;
      a = new_word[new_word.size() - 1];
      new_word.resize(new_word.size() - 1);
      i = new_word.size();
      while (true) {
        try_word(new_word,parms->edit_distance_weights.del2);
        if (i == 0) break;
        b = a;
        a = new_word[i-1];
        new_word[i-1] = b;
        --i;
      }
    }
  }

  void Working::try_one_edit_sl() 
  {
    const String & soundslike = original_word.soundslike;
    const char * replace_list = lang->soundslike_chars();
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
        try_sound(new_soundslike, parms->edit_distance_weights.sub);
      }
      new_soundslike[i] = soundslike[i];
    }
    
    // Interchange two adjacent letters.
    
    for (i = 0; i+1 != soundslike.size(); ++i) {
      a = new_soundslike[i];
      b = new_soundslike[i+1];
      new_soundslike[i] = b;
      new_soundslike[i+1] = a;
      try_sound(new_soundslike,parms->edit_distance_weights.swap);
      new_soundslike[i] = a;
      new_soundslike[i+1] = b;
    }

    // Add one letter

    new_soundslike += ' ';
    i = new_soundslike.size()-1;
    while(true) {
      for (c=replace_list; *c; ++c) {
        new_soundslike[i] = *c;
        try_sound(new_soundslike,parms->edit_distance_weights.del1);
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
        try_sound(new_soundslike,parms->edit_distance_weights.del2);
        if (i == 0) break;
        b = a;
        a = new_soundslike[i-1];
        new_soundslike[i-1] = b;
        --i;
      }
    }
  }

  // TODO: Consider adding support for converting the word to the "simple"
  //   soundslike on the fly when "real" soundslike data is not available.
  // This will even work with affix compressed words and the optimization
  //   to avoid expanding unless needed.  Care needs to be taken to account
  //   for the fact that the simpile sl could be smaller than the word.
  //   Using "(strlen(word) - strlen(sl)) + stopped_at + 1" as the limit
  //   should do the trick.
  // When using jump-tables the words will need to grouped based on
  //   the simple soundslike and not the actual word.

  void Working::try_scan() 
  {
    const char * original_soundslike = active_soundslike().str();
    //unsigned int original_soundslike_len = strlen(original_soundslike);
    
    EditDist (* edit_dist_fun)(const char *, const char *, 
                               const EditDistanceWeights &);
    
    if (parms->soundslike_level == 1)
      edit_dist_fun = limit1_edit_distance;
    else
      edit_dist_fun = limit2_edit_distance;

    WordEntry * sw;
    WordEntry w;
    const char * sl = 0;
    String sl_buf; // FIXME: Make constant for max word len
    EditDist score;
    unsigned int stopped_at = LARGE_NUM;
    ObjStack exp_buf;
    WordAff * exp_list;
    WordAff single;
    single.next = 0;

    for (SpellerImpl::WS::const_iterator i = speller->suggest_ws.begin();
         i != speller->suggest_ws.end();
         ++i) 
    {
      StackPtr<SoundslikeEnumeration> els(i->dict->soundslike_elements());
      
      while ( (sw = els->next(stopped_at)) ) {

        if (sw->what != WordEntry::Word) {
          sl = sw->word;
        } else if (!*sw->aff) {
          sl_buf.clear();
          sl = lang->LangImpl::to_clean(sl_buf, sw->word);
        } else {
          goto affix_case;
        }

        score = edit_dist_fun(sl, original_soundslike, parms->edit_distance_weights);
        stopped_at = score.stopped_at - sl;
        if (score >= LARGE_NUM) continue;
        stopped_at = LARGE_NUM;
        i->dict->soundslike_lookup(*sw, w);
	//CERR << sw->word << "\n";
        for (; !w.at_end(); w.adv()) {
	  //CERR << "  " << w.word << "\n";
          ParmString w_word(w.word);
          char * wf = (char *)buffer.alloc(w_word.size() + 1);
          i->convert.convert(w_word, wf);
          WordEntry * repl = 0;
          if (w.what == WordEntry::Misspelled) {
            repl = new WordEntry;
            const ReplacementDict * repl_dict
              = static_cast<const ReplacementDict *>(i->dict);
            repl_dict->repl_lookup(w, *repl);
          }
          add_nearmiss(MutableString(wf, w_word.size()), score, do_count, repl);
        }
        continue;

      affix_case:

        exp_buf.reset();

        // first expand any prefixes
        if (fast_scan) { // if fast_scan than no prefixes
          single.word.str = sw->word;
          single.word.size = strlen(sw->word);
          single.aff = (const unsigned char *)sw->aff;
          exp_list = &single;
        } else {
          exp_list = lang->affix()->expand_prefix(sw->word, sw->aff, exp_buf);
        }

        // iterate through each semi-expanded word, any affix flags
        // are now guaranteed to be suffixes
        for (WordAff * p = exp_list; p; p = p->next)
        {
          // try the root word
          sl_buf.clear();
          lang->LangImpl::to_clean(sl_buf, p->word);
          score = edit_dist_fun(sl_buf.c_str(), original_soundslike, parms->edit_distance_weights);
          stopped_at = score.stopped_at - sl_buf.c_str();

          if (score < LARGE_NUM) {
            char * wf = (char *)buffer.alloc(p->word.size + 1);
            i->convert.convert(p->word, wf);
            add_nearmiss(MutableString(wf, p->word.size), score, do_count);
          }

          // expand any suffixes, using stopped_at as a hint to avoid
          // unneeded expansions.  Note stopped_at is the last character
          // looked at by limit_edit_dist.  Thus if the character
          // at stopped_at is changed it might effect the result
          // hence the "limit" is stopped_at + 1
          if (p->word.size - lang->affix()->max_strip() > stopped_at)
            exp_list = 0;
          else
            exp_list = lang->affix()->expand_suffix(p->word, p->aff, 
                                                    exp_buf, stopped_at + 1);

          // reset stopped_at if necessary
          if (score < LARGE_NUM) stopped_at = LARGE_NUM;

          // iterate through fully expanded words, if any
          for (WordAff * q = exp_list; q; q = q->next) {
            sl_buf.clear();
            lang->to_clean(sl_buf, q->word);
            score = edit_dist_fun(sl_buf.c_str(), original_soundslike, parms->edit_distance_weights);
            if (score >= LARGE_NUM) continue;
            char * wf = (char *)buffer.alloc(q->word.size + 1);
            i->convert.convert(q->word, wf);
            add_nearmiss(MutableString(wf, q->word.size), score, do_count);
          }
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

  void Working::try_repl() 
  {
    CharVector buf;
    Vector<ReplTry> repl_try;
    StackPtr<SuggestReplEnumeration> els(lang->repl());
    const SuggestRepl * r = 0;
    const char * word = original_word.lower.c_str();
    const char * wend = word + original_word.lower.size();
    while (r = els->next(), r) 
    {
      const char * p = word;
      while ((p = strstr(p, r->substr))) {
        buf.clear();
        buf.append(word, p);
        buf.append(r->repl, strlen(r->repl));
        p += strlen(r->substr);
        buf.append(p, wend + 1);
        try_sound(buf.data(), parms->edit_distance_weights.sub*3/2);
      }
    }
  }

  
  void Working::score_list() {
    if (near_misses.empty()) return;

    NearMisses::iterator i;
    NearMisses::iterator prev;
    int word_score;
      
    near_misses.push_front(ScoreWordSound());
    // the first item will NEVER be looked at.
    scored_near_misses.push_front(ScoreWordSound());
    scored_near_misses.front().score = -1;
    // this item will only be looked at when sorting so 
    // make it a small value to keep it at the front.

    int try_for = (parms->word_weight*parms->edit_distance_weights.max)/100;
    while (true) {
      try_for += (parms->word_weight*parms->edit_distance_weights.max)/100;
	
      // put all pairs whose score <= initial_limit*max_weight
      // into the scored list

      prev = near_misses.begin();
      i = prev;
      ++i;
      while (i != near_misses.end()) {

	int level = needed_level(try_for, i->soundslike_score);
	
	if (!use_soundslike)
	  word_score = i->soundslike_score;
	else if (level >= int(i->soundslike_score/parms->edit_distance_weights.min))
	  word_score = edit_distance(original_word.clean,
				     i->word_clean,
				     level, level,
				     parms->edit_distance_weights);
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
	
      if ((k == parms->skip && i->score <= try_for) 
	  || prev == near_misses.begin() ) // or no more left in near_misses
	break;
    }
      
    threshold = i->score + parms->span;
    if (threshold < parms->edit_distance_weights.max)
      threshold = parms->edit_distance_weights.max;

#  ifdef DEBUG_SUGGEST
    COUT << "Threshold is: " << threshold << "\n";
    COUT << "try_for: " << try_for << "\n";
    COUT << "Size of scored: " << scored_near_misses.size() << "\n";
    COUT << "Size of ! scored: " << near_misses.size() << "\n";
#  endif
      
    //if (threshold - try_for <=  parms->edit_distance_weights.max/2) return;
      
    prev = near_misses.begin();
    i = prev;
    ++i;
    while (i != near_misses.end()) {
	
      int initial_level = needed_level(try_for, i->soundslike_score);
      int max_level = needed_level(threshold, i->soundslike_score);
	
      if (!use_soundslike)
	word_score = i->soundslike_score;
      else if (initial_level < max_level)
	word_score = edit_distance(original_word.clean.c_str(),
				   i->word_clean,
				   initial_level+1,max_level,
				   parms->edit_distance_weights);
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
    
    if (parms->use_typo_analysis) {
      int max = 0;
      unsigned int j;

      CharVector original, word;
      original.resize(original_word.word.size() + 1);
      for (j = 0; j != original_word.word.size(); ++j)
          original[j] = lang->to_normalized(original_word.word[j]);
      original[j] = 0;
      ParmString orig(original.data(), j);
      word.resize(max_word_length + 1);
      
      for (i = scored_near_misses.begin();
	   i != scored_near_misses.end() && i->score <= threshold;
	   ++i)
      {
	for (j = 0; (i->word)[j] != 0; ++j)
	  word[j] = lang->to_normalized((i->word)[j]);
	word[j] = 0;
	int word_score 
	  = typo_edit_distance(ParmString(word.data(), j), orig,
			       *parms->typo_edit_distance_weights);
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
    String sl;
#  endif
    int c = 1;
    hash_set<String,HashString<String> > duplicates_check;
    String buf;
    String final_word;
    pair<hash_set<String,HashString<String> >::iterator, bool> dup_pair;
    for (NearMisses::const_iterator i = scored_near_misses.begin();
	 i != scored_near_misses.end() && c <= parms->limit
	   && ( i->score <= threshold || c <= 3 );
	 ++i, ++c) {
#    ifdef DEBUG_SUGGEST
      COUT << i->word << '\t' << i->score 
           << '\t' << lang->to_soundslike(sl, i->word) << "\n";
#    endif
      if (i->repl_list != 0) {
 	String::size_type pos;
	do {
 	  dup_pair = duplicates_check.insert(fix_case(i->repl_list->word, buf));
 	  if (dup_pair.second && 
 	      ((pos = dup_pair.first->find(' '), pos == String::npos)
 	       ? (bool)speller->check(*dup_pair.first)
 	       : (speller->check((String)dup_pair.first->substr(0,pos)) 
 		  && speller->check((String)dup_pair.first->substr(pos+1))) ))
 	    near_misses_final->push_back(*dup_pair.first);
 	} while (i->repl_list->adv());
      } else {
        fix_case(i->word);
	dup_pair = duplicates_check.insert(i->word);
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
    PosibErr<void> setup(SpellerImpl * m);
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
  
  PosibErr<void> SuggestImpl::setup(SpellerImpl * m)
  {
    speller_ = m;
    RET_ON_ERR(parms_.set(m->config()->retrieve("sug-mode")));
    if (m->config()->retrieve("sug-mode") == "normal" 
        && !m->fast_scan) parms_.soundslike_level = 1;

    if (m->config()->have("sug-edit-dist"))
      parms_.soundslike_level = m->config()->retrieve_int("sug-edit-dist"); // FIXME: Make sure 1 or 2
    if (m->config()->have("sug-typo-analysis"))
      parms_.use_typo_analysis = m->config()->retrieve_bool("sug-typo-analysis");
    if (m->config()->have("sug-repl-table"))
      parms_.use_repl_table = m->config()->retrieve_bool("sug-repl-table");
    
    parms_.split_chars = m->config()->retrieve("sug-split-chars");

    String keyboard = m->config()->retrieve("keyboard");
    if (keyboard == "none")
      parms_.use_typo_analysis = false;
    else
      RET_ON_ERR(aspeller::setup(parms_.typo_edit_distance_weights, m->config(), &m->lang(), keyboard));

    return no_err;
  }

  SuggestionList & SuggestImpl::suggest(const char * word) { 
#   ifdef DEBUG_SUGGEST
    COUT << "=========== begin suggest " << word << " ===========\n";
#   endif
    parms_.set_original_word_size(strlen(word));
    suggestion_list.suggestions.resize(0);
    Working sug(speller_, &speller_->lang(),word,&parms_);
    sug.get_suggestions(suggestion_list.suggestions);
#   ifdef DEBUG_SUGGEST
    COUT << "^^^^^^^^^^^  end suggest " << word << "  ^^^^^^^^^^^\n";
#   endif
    return suggestion_list;
  }
  
}

namespace aspeller {
  PosibErr<Suggest *> new_default_suggest(SpellerImpl * m) {
    StackPtr<SuggestImpl> s(new SuggestImpl);
    RET_ON_ERR(s->setup(m));
    return s.release();
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
