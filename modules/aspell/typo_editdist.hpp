#ifndef __aspell_typo_edit_distance_hh__
#define __aspell_typo_edit_distance_hh__

#include "matrix.hpp"

namespace aspell {

  struct TypoEditDistanceWeights {
    int missing; // the cost of having to insert a character
    int swap;    // the cost of swapping two adjecent letters
    ShortMatrix repl; // the cost of replacing one letter with another
    ShortMatrix extra; // the cost of removing an extra letter

    int repl_dis1; // the cost of replace when the distance is 1
    int repl_dis2; //    "          "     otherwise
    int extra_dis1;// 
    int extra_dis2;//

    // IMPORTANT: It is still necessary to initialize and fill in
    //            repl and extra
    TypoEditDistanceWeights(int m = 85,  int s = 60, 
			    int r1 = 70, int r = 110, 
			    int e1 = 70, int e = 100)
      : missing(m), swap(s)
      , repl_dis1(r1), repl_dis2(r)
      , extra_dis1(e1), extra_dis2(e)
    {}
  };

  // edit_distance finds the shortest edit distance. 
  // Preconditions:
  // max(strlen(word), strlen(target))*max(of the edit weights) <= 2^15
  // word,target are not null pointers
  // w.repl and w.extra are square matrices
  // the maximum character value is less than the size of w.repl and w.extra 
  // Returns:
  //   the edit distance between a and b

  // the running time is tightly asymptotically bounded by strlen(a)*strlen(b)

  short typo_edit_distance(const unsigned char * word, 
			   const unsigned char * target,
			   const TypoEditDistanceWeights & w);
}

#endif
