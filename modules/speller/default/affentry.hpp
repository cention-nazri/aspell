// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.
//
// Copyright 2002 Kevin B. Hendricks, Stratford, Ontario, Canada And
// Contributors.  All rights reserved. See the file affix.license for
// details.

#ifndef _AFFIX_HXX_
#define _AFFIX_HXX_

#include "affix.hpp"
#include "data.hpp"

/* A Prefix Entry  */

namespace aspeller {

struct PfxEntry : public AffEntry
{
  AffixMgr*    pmyMgr;
  
  PfxEntry * next;
  PfxEntry * next_eq;
  PfxEntry * next_ne;
  PfxEntry * flag_next;
  PfxEntry(AffixMgr * pmgr) : pmyMgr(pmgr) {}
  ~PfxEntry();

  bool check(const LookupInfo &, ParmString, CheckInfo &, GuessInfo *) const;

  inline bool          allow_cross() const { return ((xpflg & XPRODUCT) != 0); }
  inline unsigned char flag() const { return achar;  }
  inline const char *  key() const  { return appnd;  }
  char *               add(ParmString) const;
};

/* A Suffix Entry */

struct SfxEntry : public AffEntry
{
  AffixMgr*    pmyMgr;
  char *       rappnd; // this is set in AffixMgr::build_sfxlist
  
  SfxEntry *   next;
  SfxEntry *   next_eq;
  SfxEntry *   next_ne;
  SfxEntry *   flag_next;

  SfxEntry(AffixMgr* pmgr) : pmyMgr(pmgr) {}
  ~SfxEntry();

  bool check(const LookupInfo &, ParmString, CheckInfo &, GuessInfo *,
             int optflags, AffEntry * ppfx);

  inline bool          allow_cross() const { return ((xpflg & XPRODUCT) != 0); }
  inline unsigned char flag() const { return achar;  }
  inline const char *  key() const  { return rappnd; } 
  char * add(ParmString) const;
};

}

#endif


