// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#ifndef _asuggest_hh_
#define _asuggest_hh_

#include "editdist.hpp"
#include "typo_editdist.hpp"

namespace aspeller {
  class Speller;
  class Suggest;

  struct SuggestParms {
    // implementation at the end of suggest.cc

    EditDistanceWeights     edit_distance_weights;
    TypoEditDistanceWeights typo_edit_distance_weights;

    bool use_typo_analysis;

    int normal_soundslike_weight; // percentage

    int small_word_soundslike_weight; 
    int small_word_threshold;
    
    int soundslike_weight;
    int word_weight;

    int soundslike_level; // either 1 or 2
    
    int skip;
    int span;
    int limit;

    SuggestParms(ParmString mode = "normal") {
      set(mode);
    }
    
    PosibErr<void> set(ParmString mode = "normal");
    PosibErr<void> fill_distance_lookup(const Config * c, const Language & l);
    
    virtual ~SuggestParms() {}
    virtual SuggestParms * clone() const;
    virtual void set_original_word_size(int size);
  };
  
  Suggest * new_default_suggest(const Speller *, const SuggestParms &);
}

#endif
