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

#include "posib_err.hpp"
#include "wordinfo.hpp"
#include "fstream.hpp"
#include "parm_string.hpp"
#include "char_vector.hpp"

#define SETSIZE         256
#define MAXAFFIXES      256
#define MAXWORDLEN      100
#define XPRODUCT        (1 << 0)

#define MAXLNLEN        1024

#define TESTAFF( a , b) strchr(a, b)

namespace acommon {
  class Config;
  struct CheckInfo;
}

namespace aspeller {

  using namespace acommon;

  class Language;

  class SpellerImpl;
  using acommon::CheckInfo;
  struct GuessInfo;

  struct LookupInfo {
    SpellerImpl * sp;
    inline BasicWordInfo lookup (ParmString word);
    LookupInfo(SpellerImpl * s) : sp(s) {}
  };

  struct CheckList;
  CheckList * new_check_list();
  void delete_check_list(CheckList *);
  CheckInfo * check_list_data(CheckList *);

  struct AffEntry
  {
    char *       appnd;
    char *       strip;
    short        appndl;
    short        stripl;
    short        numconds;
    short        xpflg;
    char         achar;
    char         conds[SETSIZE];
  };
  struct PfxEntry;
  struct SfxEntry;

  struct WordAff
  {
    String word;
    String af;
  };

  class AffixMgr
  {

    const Language * lang;

    PfxEntry *          pStart[SETSIZE];
    SfxEntry *          sStart[SETSIZE];
    PfxEntry *          pFlag[SETSIZE];
    SfxEntry *          sFlag[SETSIZE];

    String              encoding;
    String              compound;
    int                 cpdmin;

    String affix_file;

  public:
 
    AffixMgr(const Language * l) : lang(l) {}
    ~AffixMgr();

    PosibErr<void> setup(ParmString affpath);

    BasicWordInfo       affix_check(LookupInfo, ParmString, CheckInfo &, GuessInfo *) const;
    BasicWordInfo       prefix_check(LookupInfo, ParmString, CheckInfo &, GuessInfo *) const;
    BasicWordInfo       suffix_check(LookupInfo, ParmString, CheckInfo &, GuessInfo *,
				     int sfxopts, AffEntry* ppfx) const;


    void                munch(ParmString word, CheckList *) const;
    void                expand(ParmString word, ParmString affixes,
                               CheckList *) const;
    // expand enough so the affixes does not effect the first limit
    // characters
    int                 expand(ParmString word, ParmString af, 
                               int limit, WordAff * l) const;

    char *              get_encoding();
             
  private:
    PosibErr<void> parse_file(const char * affpath);
    PosibErr<void> parse_affix(ParmString line, const char at, FStream & af);

    void encodeit(AffEntry * ptr, char * cs);
    PosibErr<void> build_pfxlist(PfxEntry* pfxptr);
    PosibErr<void> build_sfxlist(SfxEntry* sfxptr);
    PosibErr<void> process_pfx_order();
    PosibErr<void> process_sfx_order();
  };

  PosibErr<AffixMgr *> new_affix_mgr(ParmString name, 
                                     const Language * lang);

}

#endif

