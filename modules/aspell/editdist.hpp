#ifndef __aspell_edit_distance_hh__
#define __aspell_edit_distance_hh__

#include "weights.hpp"

namespace aspell {

  // edit_distance finds the shortest edit distance.  The edit distance is 
  // (cost of swap)(# of swaps) + (cost of deletion)(# of deletions) 
  //   + (cost of insertion)(# of insertions) 
  //   + (cost of substitutions)(# of substitutions)

  // Preconditions:
  // max(strlen(a), strlen(b))*max(of the edit weights) <= 2^15
  // a,b are not null pointers
  // Returns:
  //   the edit distance between a and b

  // the running time is tightly asymptotically bounded by strlen(a)*strlen(b)

  short edit_distance(const char * a, const char * b,
		      const EditDistanceWeights & w = EditDistanceWeights());
}

#endif
