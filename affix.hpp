// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.
//
// Copyright 2002 Kevin B. Hendricks, Stratford, Ontario, Canada And
// Contributors.  All rights reserved. See the file affix.license for
// details.

#ifndef _AFFIXMGR_HXX_
#define _AFFIXMGR_HXX_

#include "data.hpp"

#define SETSIZE         256
#define MAXAFFIXES      256
#define MAXWORDLEN      100
#define XPRODUCT        (1 << 0)

#define MAXLNLEN        1024

#define TESTAFF( a , b) strchr(a, b)

namespace aspeller {

struct affentry
{
   char * strip;
   char * appnd;
   short  stripl;
   short  appndl;
   short  numconds;
   short  xpflg;
   char   achar;
   char   conds[SETSIZE];
};

struct replentry {
  char * pattern;
  char * replacement;
};

  struct LookupInfo {
    const BasicWordSet * ws;
    const SensitiveCompare & cmp;
    BasicWordInfo lookup (ParmString word) {return ws->lookup(word,cmp);}
    LookupInfo(const BasicWordSet * w,  const SensitiveCompare & c)
      : ws(w), cmp(c) {}
  };

class AffEntry
{
protected:
       char *       appnd;
       char *       strip;
       short        appndl;
       short        stripl;
       short        numconds;
       short        xpflg;
       char         achar;
       char         conds[SETSIZE];
};

class AffixMgr
{

  AffEntry *          pStart[SETSIZE];
  AffEntry *          sStart[SETSIZE];
  BasicWordSet *      pHMgr;
  char *              trystring;
  char *              encoding;
  char *              compound;
  int                 cpdmin;
  int                 numrep;
  replentry *         reptable;

public:
 
  AffixMgr(const char * affpath, BasicWordSet * ptr);
  ~AffixMgr();
  BasicWordInfo       affix_check(LookupInfo, const char * word, int len);
  BasicWordInfo       prefix_check(LookupInfo, const char * word, int len);
  BasicWordInfo       suffix_check(LookupInfo, const char * word, int len, 
				   int sfxopts, AffEntry* ppfx);
  BasicWordInfo       compound_check(const char * word, int len, char compound_flag);
  int                 get_numrep();
  struct replentry *  get_reptable();
  char *              get_encoding();
  char *              get_try_string();
  char *              get_compound();
             
private:
  int  parse_file(const char * affpath);
  int  parse_try(char * line);
  int  parse_set(char * line);
  int  parse_cpdflag(char * line);
  int  parse_cpdmin(char * line);
  int  parse_reptable(char * line, FILE * af);
  int  parse_affix(char * line, const char at, FILE * af);

  void encodeit(struct affentry * ptr, char * cs);
  int build_pfxlist(AffEntry* pfxptr);
  int build_sfxlist(AffEntry* sfxptr);
  int process_pfx_order();
  int process_sfx_order();
};

}

#endif

