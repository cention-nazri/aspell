
#include <cstring>

#include "typo_editdist.hpp"

// edit_distance is implemented using a straight forward dynamic
// programming algorithm with out any special tricks.  Its space
// usage AND running time is tightly asymptotically bounded by
// strlen(a)*strlen(b)

namespace aspeller {

  using namespace std;

  short typo_edit_distance(const char * word0, 
			   const char * target0,
			   const TypoEditDistanceWeights & w) 
  {
    int word_size   = strlen(word0  ) + 1;
    int target_size = strlen(target0) + 1;
    const unsigned char * word 
      = reinterpret_cast<const unsigned char *>(word0);
    const unsigned char * target 
      = reinterpret_cast<const unsigned char *>(target0);
    ShortMatrix e;
    e.init(word_size,target_size);
    e(0,0) = 0;
    for (int j = 1; j != target_size; ++j)
      e(0,j) = e(0,j-1) + w.missing;
    --word;
    --target;
    short te;
    for (int i = 1; i != word_size; ++i) {
      e(i,0) = e(i-1,0) + w.extra_dis2;
      for (int j = 1; j != target_size; ++j) {

	if (word[i] == target[j]) {

	  e(i,j) = e(i-1,j-1);

	} else {
	  
	  te = e(i,j) = e(i-1,j-1) + w.repl(word[i],target[j]);
	  
	  if (i != 1) {
	    te =  e(i-1,j ) + w.extra(word[i-1], target[j]);
	    if (te < e(i,j)) e(i,j) = te;
	    te = e(i-2,j-1) + w.extra(word[i-1], target[j]) 
 	                     + w.repl(word[i]  , target[j]);
	    if (te < e(i,j)) e(i,j) = te;
	  } else {
	    te =  e(i-1,j) + w.extra_dis2;
	    if (te < e(i,j)) e(i,j) = te;
	  }

	  te = e(i,j-1) + w.missing;
	  if (te < e(i,j)) e(i,j) = te;

	  //swap
	  if (i != 1 && j != 1) {
	      te = e(i-2,j-2) + w.swap
		+ w.repl(word[i], target[j-1])
		+ w.repl(word[i-1], target[j]);
	      if (te < e(i,j)) e(i,j) = te;
	    }
	}
      } 
    }
    return e(word_size-1,target_size-1);
  }
}
