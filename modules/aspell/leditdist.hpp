
#ifndef __aspell_leditdist_hh__
#define __aspell_leditdist_hh__

#include "weights.hpp"

namespace aspell {

  // limit_edit_distance finds the shortest edit distance but will
  // stop and return a number at least as large as LARGE_NUM if it has
  // to do more edits than a set limit.
  // Note that this does NOT bean than the score returned <= limit*w.max
  // as sub vs submarine will return 6*(cost of insertion) no matter what
  // the limit is.
  // The edit distance is 
  // (cost of swap)(# of swaps) + (cost of deletion)(# of deletions) 
  //   + (cost of insertion)(# of insertions) 
  //   + (cost of substitutions)(# of substitutions)

  // Preconditions:
  //  (limit+1)*w.min < limit*w.max
  //  limit <= 5 (use edit_distance if n > 5)

  // The running time is asymptotically bounded above by 
  // (3^l)*n where l is the limit and n is the maxium of strlen(a),strlen(b)
  // Based ob my informal tests however the n does not really matter
  // and the running time is more like (3^l).

  // limit_edit_distance, based on my informal tests, turns out to be
  // faster than edit_dist for l < 5.  For l == 5 it is about the 
  // smaller for short strings (<= 5) and less than for longer strings

  // limit2_edit_distance(a,b,w) = limit_edit_distance(a,b,2,w)
  //  but is roughly 2/3's faster

  static const int LARGE_NUM = 0xFFFFF;

  int limit_edit_distance(const char * a, const char * b, int limit, 
			  const EditDistanceWeights & w 
			  = EditDistanceWeights());
  
  int limit2_edit_distance(const char * a, const char * b,
			   const EditDistanceWeights & w 
			   = EditDistanceWeights());
  
}

#endif
