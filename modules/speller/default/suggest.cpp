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
    char * word;
    char * word_clean;
    const char * soundslike;
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
    OriginalWord     original;
    const SuggestParms * parms;

  public:
    Score(const Language *l, const String &w, const SuggestParms * p)
      : lang(l), original(), parms(p)
    {
      original.word = w;
      l->to_lower(original.lower, w.str());
      l->to_clean(original.clean, w.str());
      l->to_soundslike(original.soundslike, w.str());
      original.case_pattern = l->case_pattern(w);
    }
    void fix_case(char * str) {
      lang->LangImpl::fix_case(original.case_pattern, str, str);
    }
    const char * fix_case(const char * str, String & buf) {
      return lang->LangImpl::fix_case(original.case_pattern, str, buf);
    }
  };

  class Working : public Score {
   
    int threshold;
    bool try_harder;

    unsigned int max_word_length;

    SpellerImpl  *     sp;
    NearMisses         scored_near_misses;
    NearMisses         near_misses;
    NearMissesFinal  * near_misses_final;

    char * temp_end;

    String             tmpbuf;
    ObjStack           buffer;

    static const bool do_count = true;
    static const bool dont_count = false;

    void commit_temp(const char * b) {
      if (temp_end) {
        buffer.resize_temp(temp_end - b + 1);
        buffer.commit_temp();
        temp_end = 0; }}
    void abort_temp() {
      buffer.abort_temp();
      temp_end = 0;}
    const char * to_soundslike_temp(const char * w, unsigned s, unsigned * len = 0) {
      char * sl = (char *)buffer.alloc_temp(s + 1);
      temp_end = lang->LangImpl::to_soundslike(sl, w, s);
      if (len) *len = temp_end - sl;
      return sl;}
    const char * to_soundslike_temp(const WordEntry & sw) {
      char * sl = (char *)buffer.alloc_temp(sw.word_size + 1);
      temp_end = lang->LangImpl::to_soundslike(sl, sw.word, sw.word_size, sw.word_info);
      if (temp_end == 0) return sw.word;
      else return sl;}
    const char * to_soundslike(const char * w, unsigned s) {
      char * sl = (char *)buffer.alloc_temp(s + 1);
      temp_end = lang->LangImpl::to_soundslike(sl, w, s);
      commit_temp(sl);
      return sl;}

    char * convert(SpellerImpl::WS::const_iterator i, const WordEntry & sw) {
      char * w = (char *)buffer.alloc(sw.word_size + 1);
      i->convert.convert(sw.word, w);
      return w;
    }
    char * convert(SpellerImpl::WS::const_iterator i, ParmString w) {
      char * t = (char *)buffer.alloc(w.size() + 1);
      i->convert.convert(w, t);
      return t;
    }

    void try_word(ParmString str, int score);
    void add_sound(SpellerImpl::WS::const_iterator i,
                   WordEntry * sw, const char * sl, int score = -1);
    void add_nearmiss(char * word, unsigned word_size, WordInfo word_info,
                      const char * sl,
                      int w_score, int sl_score,
                      bool count = do_count, WordEntry * rl = 0);
    void add_nearmiss(SpellerImpl::WS::const_iterator, const WordEntry & w, 
                      const char * sl,
                      int w_score, int sl_score, bool count = do_count);
    void add_nearmiss(SpellerImpl::WS::const_iterator, const WordAff * w,
                      const char * sl, 
                      int w_score, int sl_score, bool count = do_count);
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

    void try_split();
    void try_one_edit_word();
    void try_scan();
    void try_scan_root();
    void try_repl();
    void try_ngram();

    void score_list();
    void fine_tune_score();
    void transfer();
  public:
    Working(SpellerImpl * m, const Language *l,
	    const String & w, const SuggestParms *  p)
      : Score(l,w,p), threshold(1), max_word_length(0), sp(m) {}
    void get_suggestions(NearMissesFinal &sug);
  };

  void Working::get_suggestions(NearMissesFinal & sug) {

    near_misses_final = & sug;

    try_split();

    if (parms->try_one_edit_word)
      try_one_edit_word();

    if (parms->try_scan) {
      if (sp->soundslike_root_only)
        try_scan_root();
      else
        try_scan();
    }
    
    if (parms->use_repl_table)
      try_repl();

    score_list();

    if (try_harder) {

      try_ngram();

      score_list();
      
    }

    fine_tune_score();

    transfer();
  }

  void Working::try_word(ParmString str, int score)  
  {
    String word;
    String buf;
    WordEntry sw;
    for (SpellerImpl::WS::const_iterator i = sp->suggest_ws.begin();
         i != sp->suggest_ws.end();
         ++i)
    {
      i->dict->clean_lookup(str, sw);
      for (;!sw.at_end(); sw.adv())
        add_nearmiss(i, sw, 0, score, -1, do_count);
    }
    if (sp->affix_compress) {
      // FIXME: Double check, optimize
      CheckInfo ci; memset(&ci, 0, sizeof(ci));
      bool res = lang->affix()->affix_check(LookupInfo(sp, LookupInfo::Clean), str, ci, 0);
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
      add_nearmiss(tmp, wlen, 0, 0, score, -1, do_count);
    }
  }

  void Working::add_nearmiss(char * word, unsigned word_size,
                             WordInfo word_info,
                             const char * sl,
                             int w_score, int sl_score, 
                             bool count, WordEntry * rl)
  {
    if (sl == 0) sl = to_soundslike(word, word_size);
    if (sl_score < 0) sl_score = edit_distance(original.soundslike, sl, 
                                               parms->edit_distance_weights);
    //CERR.printf("adding nearmiss %s (%d) %s (%d)\n", 
    //            word, w_score, sl, sl_score);

    near_misses.push_front(ScoreWordSound());
    ScoreWordSound & d = near_misses.front();
    d.word = word;
    d.soundslike = sl;
    
    if (parms->use_typo_analysis) {
      unsigned int l = word_size;
      if (l > max_word_length) max_word_length = l;
    }
    
    if (word_info & ALL_CLEAN) {
      d.word_clean = (char *)buffer.alloc(word_size + 1);
      lang->LangImpl::to_clean((char *)d.word_clean, word);
    } else {
      d.word_clean = d.word;
    }
    
    d.word_score       = w_score;
    d.soundslike_score = sl_score;
    d.count = count;
    d.repl_list = rl;
  }

  void Working::add_nearmiss(SpellerImpl::WS::const_iterator i,
                             const WordEntry & w, const char * sl,
                             int w_score, int sl_score, bool count)
  {
    assert(w.word_size == strlen(w.word));
    WordEntry * repl = 0;
    if (w.what == WordEntry::Misspelled) {
      repl = new WordEntry;
      const ReplacementDict * repl_dict
        = static_cast<const ReplacementDict *>(i->dict);
      repl_dict->repl_lookup(w, *repl);
    }
    add_nearmiss(convert(i, w.word), w.word_size, w.word_info, 
                 sl,
                 w_score, sl_score, count);
  }

  void Working::add_nearmiss(SpellerImpl::WS::const_iterator i,
                             const WordAff * w, const char * sl,
                             int w_score, int sl_score, bool count)
  {
    add_nearmiss(convert(i, w->word.str), w->word.size, 0, 
                 sl,
                 w_score, sl_score, count);
  }

  void Working::try_split() {
    const String & word       = original.word;
    
    if (word.size() < 4 || parms->split_chars.empty()) return;
    size_t i = 0;
    
    String new_word_str;
    String buf;
    new_word_str.resize(word.size() + 1);
    char * new_word = new_word_str.data();
    memcpy(new_word, word.data(), word.size());
    new_word[word.size() + 1] = '\0';
    new_word[word.size() + 0] = new_word[word.size() - 1];
    
    for (i = word.size() - 2; i >= 2; --i) {
      new_word[i+1] = new_word[i];
      new_word[i] = '\0';
      
      if (sp->check(new_word) && sp->check(new_word + i + 1)) {
        for (size_t j = 0; j != parms->split_chars.size(); ++j)
        {
          new_word[i] = parms->split_chars[j];
          add_nearmiss(buffer.dup(new_word), word.size() + 1, 0, 0,
                       parms->edit_distance_weights.del2*3/2, -1,
                       dont_count);
        }
      }
    }
  }

  void Working::try_one_edit_word() 
  {
    const String & orig = original.clean;
    const char * replace_list = lang->clean_chars();
    char a,b;
    const char * c;
    String new_word;
    size_t i;

    // Try word as is (in case of case difference etc)

    try_word(orig, 0);

    // Change one letter
    
    new_word = orig;
    
    for (i = 0; i != orig.size(); ++i) {
      for (c = replace_list; *c; ++c) {
        if (*c == orig[i]) continue;
        new_word[i] = *c;
        try_word(new_word, parms->edit_distance_weights.sub);
      }
      new_word[i] = orig[i];
    }
    
    // Interchange two adjacent letters.
    
    for (i = 0; i+1 != orig.size(); ++i) {
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

    if (orig.size() > 1) {
      new_word = orig;
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

  void Working::add_sound(SpellerImpl::WS::const_iterator i,
                          WordEntry * sw, const char * sl, int score)
  {
    WordEntry w;
    i->dict->soundslike_lookup(*sw, w);

    for (; !w.at_end(); w.adv()) {
      
      add_nearmiss(i, w, sl, -1, score);

      if (w.aff[0]) {
        String sl_buf;
        ObjStack exp_buf;
        WordAff * exp_list;
        exp_list = lang->affix()->expand(w.word, w.aff, exp_buf);
        for (WordAff * p = exp_list->next; p; p = p->next)
          add_nearmiss(i, p, 0, -1, -1);
      }

    }
  }

  void Working::try_scan() 
  {
    const char * original_soundslike = original.soundslike.str();
    
    EditDist (* edit_dist_fun)(const char *, const char *, 
                               const EditDistanceWeights &);
    
    if (parms->soundslike_level == 1)
      edit_dist_fun = limit1_edit_distance;
    else
      edit_dist_fun = limit2_edit_distance;

    WordEntry * sw;
    WordEntry w;
    const char * sl = 0;
    EditDist score;
    unsigned int stopped_at = LARGE_NUM;
    ObjStack exp_buf;
    WordAff * exp_list;
    WordAff single;
    single.next = 0;

    for (SpellerImpl::WS::const_iterator i = sp->suggest_ws.begin();
         i != sp->suggest_ws.end();
         ++i) 
    {
      StackPtr<SoundslikeEnumeration> els(i->dict->soundslike_elements());

      while ( (sw = els->next(stopped_at)) ) {

        //CERR.printf("[%s (%d) %d]\n", sw->word, sw->word_size, sw->what);
        assert(strlen(sw->word) == sw->word_size);
          
        if (sw->what != WordEntry::Word) {
          sl = sw->word;
          abort_temp();
        } else if (!*sw->aff) {
          sl = to_soundslike_temp(*sw);
        } else {
          goto affix_case;
        }

        //CERR.printf("SL = %s\n", sl);
        
        score = edit_dist_fun(sl, original_soundslike, parms->edit_distance_weights);
        stopped_at = score.stopped_at - sl;
        if (score >= LARGE_NUM) continue;
        stopped_at = LARGE_NUM;
        commit_temp(sl);
        add_sound(i, sw, sl, score);
        continue;
        
      affix_case:
        
        exp_buf.reset();
        
        // first expand any prefixes
        if (sp->fast_scan) { // if fast_scan than no prefixes
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
          unsigned sl_len;
          sl = to_soundslike_temp(p->word.str, p->word.size, &sl_len);
          score = edit_dist_fun(sl, original_soundslike, parms->edit_distance_weights);
          stopped_at = score.stopped_at - sl;
          stopped_at += p->word.size - sl_len;
          
          if (score < LARGE_NUM) {
            commit_temp(sl);
            add_nearmiss(i, p, sl, score, -1, do_count);
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
            sl = to_soundslike_temp(q->word.str, q->word.size);
            score = edit_dist_fun(sl, original_soundslike, parms->edit_distance_weights);
            if (score >= LARGE_NUM) continue;
            commit_temp(sl);
            add_nearmiss(i, q, sl, score, -1, do_count);
          }
        }
      }
    }
  }

  void Working::try_scan_root() 
  {
    EditDist (* edit_dist_fun)(const char *, const char *, 
                               const EditDistanceWeights &);
    
    if (parms->soundslike_level == 1)
      edit_dist_fun = limit1_edit_distance;
    else
      edit_dist_fun = limit2_edit_distance;

    WordEntry * sw;
    WordEntry w;
    const char * sl = 0;
    EditDist score;
    int stopped_at = LARGE_NUM;
    CheckList * cl = new_check_list();
    lang->munch(original.word, cl);
    Vector<const char *> sls;
    sls.push_back(original.soundslike.str());
#ifdef DEBUG_SUGGEST
    COUT.printf("will try soundslike: %s\n", sls.back());
#endif
    for (const aspeller::CheckInfo * ci = check_list_data(cl); 
         ci; 
         ci = ci->next) 
    {
      sl = to_soundslike(ci->word.str(), ci->word.size());
      Vector<const char *>::iterator i = sls.begin();
      while (i != sls.end() && strcmp(*i, sl) != 0) ++i;
      if (i == sls.end()) {
        sls.push_back(to_soundslike(ci->word.str(), ci->word.size()));
#ifdef DEBUG_SUGGEST
        COUT.printf("will try root soundslike: %s\n", sls.back());
#endif
      }
    }
    delete_check_list(cl);
    cl = 0;
    const char * * begin = sls.pbegin();
    const char * * end   = sls.pend();
    for (SpellerImpl::WS::const_iterator i = sp->suggest_ws.begin();
         i != sp->suggest_ws.end();
         ++i) 
    {
      StackPtr<SoundslikeEnumeration> els(i->dict->soundslike_elements());

      while ( (sw = els->next(stopped_at)) ) {
          
        if (sw->what != WordEntry::Word) {
          sl = sw->word;
          abort_temp();
        } else {
          sl = to_soundslike_temp(*sw);
        } 

        stopped_at = LARGE_NUM;
        for (const char * * s = begin; s != end; ++s) {
          score = edit_dist_fun(sl, *s, 
                                parms->edit_distance_weights);
          if (score.stopped_at - sl < stopped_at)
            stopped_at = score.stopped_at - sl;
          if (score >= LARGE_NUM) continue;
          stopped_at = LARGE_NUM;
          commit_temp(sl);
          add_sound(i, sw, sl, score);
          //CERR.printf("using %s: will add %s with score %d\n", *s, sl, (int)score);
          break;
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
    String buf;
    Vector<ReplTry> repl_try;
    StackPtr<SuggestReplEnumeration> els(lang->repl());
    const SuggestRepl * r = 0;
    const char * word = original.lower.str();
    const char * wend = word + original.lower.size();
    while (r = els->next(), r) 
    {
      const char * p = word;
      while ((p = strstr(p, r->substr))) {
        buf.clear();
        buf.append(word, p);
        buf.append(r->repl, strlen(r->repl));
        p += strlen(r->substr);
        buf.append(p, wend + 1);
        try_word(buf.data(), parms->edit_distance_weights.sub*3/2);
      }
    }
  }

  // generate an n-gram score comparing s1 and s2
  static int ngram(int n, char * s1, const char * s2)
  {
    int nscore = 0;
    int l1 = strlen(s1);
    int l2 = strlen(s2);
    int ns;
    for (int j=1;j<=n;j++) {
      ns = 0;
      for (int i=0;i<=(l1-j);i++) {
        char c = *(s1 + i + j);
        *(s1 + i + j) = '\0';
        if (strstr(s2,(s1+i))) ns++;
        *(s1 + i + j ) = c;
      }
      nscore = nscore + ns;
      if (ns < 2) break;
    }
    ns = 0;
    ns = (l2-l1)-2;
    return (nscore - ((ns > 0) ? ns : 0));
  }

  struct NGramScore {
    SpellerImpl::WS::const_iterator i;
    WordEntry info;
    const char * soundslike;
    int score;
    NGramScore() {}
    NGramScore(SpellerImpl::WS::const_iterator i0,
               WordEntry info0, const char * sl, int score0) 
      : i(i0), info(info0), soundslike(sl), score(score0) {}
  };


  void Working::try_ngram()
  {
    String original_soundslike = original.soundslike;
    original_soundslike.ensure_null_end();
    WordEntry * sw = 0;
    const char * sl = 0;
    typedef Vector<NGramScore> Candidates;
    hash_set<const char *> already_have;
    Candidates candidates;
    int min_score = 0;
    int count = 0;

    for (NearMisses::iterator i = scored_near_misses.begin();
         i != scored_near_misses.end();
         ++i)
      already_have.insert(i->soundslike);

    for (SpellerImpl::WS::const_iterator i = sp->suggest_ws.begin();
         i != sp->suggest_ws.end();
         ++i) 
    {
      StackPtr<SoundslikeEnumeration> els(i->dict->soundslike_elements());
      
      while ( (sw = els->next(LARGE_NUM)) ) {

        if (sw->what != WordEntry::Word) {
          abort_temp();
          sl = sw->word;
        } else {
          sl = to_soundslike_temp(sw->word, sw->word_size);
        }
        
        if (already_have.have(sl)) continue;

        int ng = ngram(3, original_soundslike.data(), sl);

        if (ng >= min_score) {
          commit_temp(sl);
          candidates.push_back(NGramScore(i, *sw, sl, ng));
          if (ng > min_score) count++;
          if (count >= 10) {
            int orig_min = min_score;
            min_score = LARGE_NUM;
            Candidates::iterator i = candidates.begin();
            Candidates::iterator j = candidates.begin();
            for (; i != candidates.end(); ++i) {
              assert(i->info.free_ == 0);
              if (i->score == orig_min) continue;
              if (min_score > i->score) min_score = i->score;
              *j = *i;
              ++j;
            }
            count = 0;
            candidates.resize(j-candidates.begin());
            for (i = candidates.begin(); i != candidates.end(); ++i) {
              if (i->score != min_score) count++;
            }
          }
        }
      }
    }
    
    for (Candidates::iterator i = candidates.begin();
         i != candidates.end();
         ++i)
    {
#ifdef DEBUG_SUGGEST
      COUT.printf("ngram: %s %d\n", i->soundslike, i->score);
      add_sound(i->i, &i->info, i->soundslike);
#endif
    }
  }
  
  void Working::score_list() {
    try_harder = true;
    if (near_misses.empty()) return;

    NearMisses::iterator i;
    NearMisses::iterator prev;
    int word_score;

    bool no_soundslike = !lang->have_soundslike();
      
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
	
	if (no_soundslike)
	  word_score = i->soundslike_score;
	else if (level >= int(i->soundslike_score/parms->edit_distance_weights.min))
	  word_score = edit_distance(original.clean,
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
	
      if (no_soundslike)
	word_score = i->soundslike_score;
      else if (initial_level < max_level)
	word_score = edit_distance(original.clean.c_str(),
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

    near_misses.pop_front();

    scored_near_misses.sort();
    scored_near_misses.pop_front();

    try_harder = near_misses.empty();

#  ifdef DEBUG_SUGGEST
    COUT << "Size of scored: " << scored_near_misses.size() << "\n";
    COUT << "Size of ! scored: " << near_misses.size() << "\n";
#  endif
  }

  void Working::fine_tune_score() {

    NearMisses::iterator i;

    if (parms->use_typo_analysis) {
      int max = 0;
      unsigned int j;

      CharVector orig_norm, word;
      orig_norm.resize(original.word.size() + 1);
      for (j = 0; j != original.word.size(); ++j)
          orig_norm[j] = lang->to_normalized(original.word[j]);
      orig_norm[j] = 0;
      ParmString orig(orig_norm.data(), j);
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
	 << original.word << '\t' 
	 << original.soundslike << '\t'
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
      //COUT.printf("%p %p: ",  i->word, i->soundslike);
      COUT << i->word << '\t' << i->score 
           << '\t' << i->soundslike << "\n";
#    endif
      if (i->repl_list != 0) {
 	String::size_type pos;
	do {
          abort();
 	  dup_pair = duplicates_check.insert(fix_case(i->repl_list->word, buf));
 	  if (dup_pair.second && 
 	      ((pos = dup_pair.first->find(' '), pos == String::npos)
 	       ? (bool)sp->check(*dup_pair.first)
 	       : (sp->check((String)dup_pair.first->substr(0,pos)) 
 		  && sp->check((String)dup_pair.first->substr(pos+1))) ))
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
      //parms_.set_original_size(strlen(base));
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
    try_one_edit_word = false; // FIXME
    try_scan = true;           // FIXME
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
