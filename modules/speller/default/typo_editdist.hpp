#ifndef __aspeller_typo_edit_distance_hh__
#define __aspeller_typo_edit_distance_hh__

#include "cache.hpp"
#include "matrix.hpp"

namespace acommon {
  class Config;
}

namespace aspeller {

  class Language;

  using namespace acommon;

  struct TypoEditDistanceWeights : public Cacheable {
    int missing; // the cost of having to insert a character
    int swap;    // the cost of swapping two adjecent letters
    short * data; // memory for repl and extra
    ShortMatrix repl; // the cost of replacing one letter with another
    ShortMatrix extra; // the cost of removing an extra letter

    int repl_dis1; // the cost of replace when the distance is 1
    int repl_dis2; //    "          "     otherwise
    int extra_dis1;// 
    int extra_dis2;//

    // IMPORTANT: It is still necessary to initialize and fill in
    //            repl and extra
  private:
    TypoEditDistanceWeights(int m = 85,  int s = 60, 
			    int r1 = 70, int r = 110, 
			    int e1 = 70, int e = 100)
      : missing(m), swap(s), data(0) 
      , repl_dis1(r1), repl_dis2(r)
      , extra_dis1(e1), extra_dis2(e)
    {}
  public:
    ~TypoEditDistanceWeights() {if (data) free(data);}

    String keyboard;
    typedef const Config CacheConfig;
    typedef const Language CacheConfig2;
    typedef const char * CacheKey;
    bool cache_key_eq(const char * kb) const {return keyboard == kb;}
    static PosibErr<TypoEditDistanceWeights *> get_new(const char *, const Config *, const Language *);
  private:
    TypoEditDistanceWeights(const TypoEditDistanceWeights &);
    void operator=(const TypoEditDistanceWeights &);
  };

  PosibErr<void> setup(CachePtr<const TypoEditDistanceWeights> & res,
                       const Config * c, const Language * l, ParmString kb);

  // edit_distance finds the shortest edit distance. 
  // Preconditions:
  // max(strlen(word), strlen(target))*max(of the edit weights) <= 2^15
  // word,target are not null pointers
  // w.repl and w.extra are square matrices
  // the maximum character value is less than the size of w.repl and w.extra 
  // Returns:
  //   the edit distance between a and b

  // the running time is tightly asymptotically bounded by strlen(a)*strlen(b)

  short typo_edit_distance(ParmString word, 
			   ParmString target,
			   const TypoEditDistanceWeights & w);
}

#endif
