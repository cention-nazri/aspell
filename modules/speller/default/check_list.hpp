// This file is part of The New Aspell
// Copyright (C) 2004 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.

#ifndef __aspeller_check_list__
#define __aspeller_check_list__

#include "speller.hpp"

namespace aspeller {

  using acommon::CheckInfo;

  static inline void clear_check_info(CheckInfo & ci)
  {
    memset(&ci, 0, sizeof(ci));
  }

  struct GuessInfo
  {
    int num;
    int max;
    CheckInfo * last;
    GuessInfo(int m) : max(m) {}
    void reset(CheckInfo * ci) { num = 0; last = ci; }
    CheckInfo * add() {
      if (num >= max) return 0;
      num++;
      last->next = last + 1;
      last = const_cast<CheckInfo *>(last->next);
      clear_check_info(*last);
      last->guess = true;
      return last;
    }
  };

  struct CheckList
  {
    GuessInfo gi;
    CheckInfo data[64];
    void reset();
    CheckList();
    ~CheckList() {reset();}
  };


}

#endif
