
#include <cstring>

#include "vararray.hpp"
#include "typo_editdist.hpp"
#include "config.hpp"
#include "language.hpp"
#include "file_data_util.hpp"
#include "getdata.hpp"
#include "cache-t.hpp"

// edit_distance is implemented using a straight forward dynamic
// programming algorithm with out any special tricks.  Its space
// usage AND running time is tightly asymptotically bounded by
// strlen(a)*strlen(b)

namespace aspeller {

  using namespace std;

  short typo_edit_distance(ParmString word0, 
			   ParmString target0,
			   const TypoEditDistanceWeights & w) 
  {
    int word_size   = word0.size() + 1;
    int target_size = target0.size() + 1;
    const unsigned char * word 
      = reinterpret_cast<const unsigned char *>(word0.str());
    const unsigned char * target 
      = reinterpret_cast<const unsigned char *>(target0.str());
    VARARRAY(short, e_d, word_size * target_size);
    ShortMatrix e(word_size,target_size, e_d);
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

  static GlobalCache<TypoEditDistanceWeights> typo_edit_dist_weights_cache("keyboard");

  PosibErr<void> setup(CachePtr<const TypoEditDistanceWeights> & res,
                       const Config * c, const Language * l, ParmString kb)
  {
    PosibErr<TypoEditDistanceWeights *> pe = get_cache_data(&typo_edit_dist_weights_cache, c, l, kb);
    if (pe.has_err()) return pe;
    res.reset(pe.data);
    return no_err;
  }

  PosibErr<TypoEditDistanceWeights *> 
  TypoEditDistanceWeights::get_new(const char * kb, const Config * cfg, const Language * l)
  {
    FStream in;
    String file, dir1, dir2;
    fill_data_dir(cfg, dir1, dir2);
    find_file(file, dir1, dir2, kb, ".kbd");
    RET_ON_ERR(in.open(file.c_str(), "r"));

    TypoEditDistanceWeights * w = new TypoEditDistanceWeights();
    w->keyboard = kb;
    
    int c = l->max_normalized() + 1;
    int cc = c * c;
    w->data = (short *)malloc(cc * 2 * sizeof(short));
    w->repl .init(c, c, w->data);
    w->extra.init(c, c, w->data + cc);
    
    for (int i = 0; i != c; ++i) {
      for (int j = 0; j != c; ++j) {
        w->repl (i,j) = w->repl_dis2;
        w->extra(i,j) = w->extra_dis2;
      }
    }
    
    String buf;
    DataPair d;
    while (getdata_pair(in, d, buf)) {
      if (d.key.size != 2)
        return make_err(bad_file_format, file);
      w->repl (l->to_normalized(d.key[0]),
               l->to_normalized(d.key[1])) = w->repl_dis1;
      w->repl (l->to_normalized(d.key[1]),
               l->to_normalized(d.key[0])) = w->repl_dis1;
      w->extra(l->to_normalized(d.key[0]),
               l->to_normalized(d.key[1])) = w->extra_dis1;
      w->extra(l->to_normalized(d.key[1]),
               l->to_normalized(d.key[0])) = w->extra_dis1;
    }
    
    for (int i = 0; i != c; ++i) {
      w->repl(i,i) = 0;
      w->extra(i,i) = w->extra_dis1;
    }

    return w;
  }
  

}
