// This file is part of The New Aspell
// Copyright (C) 2004 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.
//
// Copyright 2002 Kevin B. Hendricks, Stratford, Ontario, Canada And
// Contributors.  All rights reserved. See the file affix.license for
// details.

#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "iostream.hpp"

#include "affix.hpp"
#include "errors.hpp"
#include "getdata.hpp"
#include "parm_string.hpp"
#include "check_list.hpp"
#include "speller_impl.hpp"

using namespace std;

namespace aspeller {

typedef unsigned char byte;
static char EMPTY[1] = {0};

//////////////////////////////////////////////////////////////////////
//
// *Entry struct definations
//

struct AffEntry
{
  char *         appnd;
  char *         strip;
  unsigned short appndl;
  unsigned short stripl;
  unsigned short numconds;
  short        xpflg;
  char         achar;
  char         conds[SETSIZE];
};

// A Prefix Entry
  
struct PfxEntry : public AffEntry
{
  AffixMgr * pmyMgr;
  
  PfxEntry * next;
  PfxEntry * next_eq;
  PfxEntry * next_ne;
  PfxEntry * flag_next;
  PfxEntry(AffixMgr * pmgr) : pmyMgr(pmgr) {}

  bool check(const LookupInfo &, ParmString, CheckInfo &, GuessInfo *) const;

  inline bool          allow_cross() const { return ((xpflg & XPRODUCT) != 0); }
  inline byte flag() const { return achar;  }
  inline const char *  key() const  { return appnd;  }
  SimpleString add(SimpleString, ObjStack & buf) const;
};

// A Suffix Entry

struct SfxEntry : public AffEntry
{
  AffixMgr*    pmyMgr;
  char *       rappnd; // this is set in AffixMgr::build_sfxlist
  
  SfxEntry *   next;
  SfxEntry *   next_eq;
  SfxEntry *   next_ne;
  SfxEntry *   flag_next;

  SfxEntry(AffixMgr* pmgr) : pmyMgr(pmgr) {}

  bool check(const LookupInfo &, ParmString, CheckInfo &, GuessInfo *,
             int optflags, AffEntry * ppfx);

  inline bool          allow_cross() const { return ((xpflg & XPRODUCT) != 0); }
  inline byte flag() const { return achar;  }
  inline const char *  key() const  { return rappnd; } 
  SimpleString add(SimpleString, ObjStack & buf, int limit = INT_MAX) const;
};

//////////////////////////////////////////////////////////////////////
//
// Utility functions declarations
//

/* return 1 if s1 is subset of s2 */
static bool isSubset(const char * s1, const char * s2)
{
  while( *s1 && (*s1 == *s2) ) {
    s1++;
    s2++;
  }
  return (*s1 == '\0');
}

// return 1 if s1 (reversed) is a leading subset of end of s2
static bool isRevSubset(const char * s1, const char * end_of_s2, int len)
{
  while( (len > 0) && *s1 && (*s1 == *end_of_s2) ) {
    s1++;
    end_of_s2--;
    len --;
  }
  return (*s1 == '\0');
}

//////////////////////////////////////////////////////////////////////
//
// CheckList
//

CheckList * new_check_list()
{
  return new CheckList;
}

void delete_check_list(CheckList * cl)
{
  delete cl;
}

CheckInfo * check_list_data(CheckList * cl)
{
  if (cl->gi.num > 0)
    return cl->data + 1;
  else
    return 0;
}

CheckList::CheckList()
 : gi(63)
{
  memset(data, 0, sizeof(data));
  gi.reset(data);
}

void CheckList::reset()
{
  for (CheckInfo * p = data + 1; p != data + 1 + gi.num; ++p) {
    free(const_cast<char *>(p->word));
    p->word = 0;
  }
  gi.reset(data);
}

//////////////////////////////////////////////////////////////////////
//
// Affix Manager
//

PosibErr<void> AffixMgr::setup(ParmString affpath, Conv & iconv)
{
  // register hash manager and load affix data from aff file
  //cpdmin = 3;  // default value
  max_strip_ = 0;
  for (int i=0; i < SETSIZE; i++) {
    pStart[i] = NULL;
    sStart[i] = NULL;
    pFlag[i] = NULL;
    sFlag[i] = NULL;
    max_strip_f[i] = 0;
  }
  return parse_file(affpath, iconv);
}

AffixMgr::~AffixMgr()
{
  // pass through linked prefix entries and clean up
  for (int i=0; i < SETSIZE ;i++) {
    pFlag[i] = NULL;
    PfxEntry * ptr = (PfxEntry *)pStart[i];
    PfxEntry * nptr = NULL;
    while (ptr) {
      nptr = ptr->next;
      delete ptr;
      ptr = nptr;
      nptr = NULL;
    }
  }

  // pass through linked suffix entries and clean up
  for (int j=0; j < SETSIZE ; j++) {
    sFlag[j] = NULL;
    SfxEntry * ptr = (SfxEntry *)sStart[j];
    SfxEntry * nptr = NULL;
    while (ptr) {
      nptr = ptr->next;
      delete ptr;
      ptr = nptr;
      nptr = NULL;
    }
  }
}

static inline void MAX(int & lhs, int rhs) 
{
  if (lhs < rhs) lhs = rhs;
}

// read in aff file and build up prefix and suffix entry objects 
PosibErr<void> AffixMgr::parse_file(const char * affpath, Conv & iconv)
{
  // io buffers
  String buf; DataPair dp;
 
  // open the affix file
  affix_file = strings.dup(affpath);
  FStream afflst;
  RET_ON_ERR(afflst.open(affpath,"r"));

  // step one is to parse the affix file building up the internal
  // affix data structures

  // FIXME: Make sure that the encoding used for the affix file
  //   is the same as the internal encoding used for the language

  // read in each line ignoring any that do not
  // start with a known line type indicator

  while (getdata_pair(afflst,dp,buf)) {
    char affix_type = ' ';

    /* parse in the name of the character set used by the .dict and .aff */

    if (dp.key == "SET") {
      String buf;
      encoding = strings.dup(fix_encoding_str(dp.value, buf));
      char msg[96];
      snprintf(msg, 96, _("Expected the file to be in \"%s\" not \"%s\"."),
               lang->data_encoding(), encoding);
      if (strcmp(encoding, lang->data_encoding()) != 0)
        return make_err(bad_file_format, affix_file, msg);
    }

    /* parse in the flag used by the controlled compound words */
    //else if (d.key == "COMPOUNDFLAG")
    //  compound = strings.dup(d.value);

    /* parse in the flag used by the controlled compound words */
    //else if (d.key == "COMPOUNDMIN")
    //  cpdmin = atoi(d.value); // FiXME

    //else if (dp.key == "TRY" || dp.key == "REP");

    else if (dp.key == "PFX" || dp.key == "SFX")
      affix_type = dp.key[0];

    if (affix_type == ' ') continue;

    //
    // parse this affix: P - prefix, S - suffix
    //

    int numents = 0;      // number of affentry structures to parse
    char achar='\0';      // affix char identifier
    short xpflg=0;
    StackPtr<AffEntry> nptr;
    {
      // split affix header line into pieces
      split(dp);
      if (dp.key.empty()) goto error;
      // key is affix char
      achar = iconv(dp.key)[0];

      split(dp);
      if (dp.key.empty()) goto error;
      // key is cross product indicator 
      if (dp.key[0] == 'Y') xpflg = XPRODUCT;
    
      split(dp);
      if (dp.key.empty()) goto error;
      // key is number of affentries
      numents = atoi(dp.key); 
  
      for (int j = 0; j < numents; j++) {
        getdata_pair(afflst, dp, buf);

        if (affix_type == 'P')
          nptr.reset(new PfxEntry(this));
        else
          nptr.reset(new SfxEntry(this));

        nptr->xpflg = xpflg;

        split(dp);
        if (dp.key.empty()) goto error;
        // key is affix charter
        if (iconv(dp.key)[0] != achar) {
          char msg[64];
          snprintf(msg, 64, _("affix '%s' is corrupt, possible incorrect count"), 
                   MsgConv(lang)(achar));
          return make_err(bad_file_format, affix_file, msg);
        }
        nptr->achar = achar;
 
        split(dp);
        if (dp.key.empty()) goto error;
        // key is strip 
        if (dp.key != "0") {
          ParmString s0(iconv(dp.key));
          MAX(max_strip_, s0.size());
          MAX(max_strip_f[(byte)achar], s0.size());
          nptr->strip = strings.dup(s0);
          nptr->stripl = s0.size();
        } else {
          nptr->strip= strings.dup("");
          nptr->stripl = 0;
        }
    
        split(dp);
        if (dp.key.empty()) goto error;
        // key is affix string or 0 for null
        if (dp.key != "0") {
          nptr->appnd = strings.dup(iconv(dp.key));
          nptr->appndl = strlen(nptr->appnd);
        } else {
          nptr->appnd  = strings.dup("");
          nptr->appndl = 0;
        }
    
        split(dp);
        if (dp.key.empty()) goto error;
        // key is the conditions descriptions
        encodeit(nptr,iconv(dp.key));
    
        // now create SfxEntry or PfxEntry objects and use links to
        // build an ordered (sorted by affix string) list
        if (affix_type == 'P')
          build_pfxlist(static_cast<PfxEntry *>(nptr.release()));
        else
          build_sfxlist(static_cast<SfxEntry *>(nptr.release())); 
      }
    }
    continue;
  error:
    char msg[32];
    snprintf(msg, 32, _("Affix '%s' is corrupt"), MsgConv(lang)(achar));
    return make_err(other_error, msg).with_file(affix_file, dp.line_num);
  }
  afflst.close();

  // now we can speed up performance greatly taking advantage of the 
  // relationship between the affixes and the idea of "subsets".

  // View each prefix as a potential leading subset of another and view
  // each suffix (reversed) as a potential trailing subset of another.

  // To illustrate this relationship if we know the prefix "ab" is
  // found in the word to examine, only prefixes that "ab" is a
  // leading subset of need be examined.  Furthermore is "ab" is not
  // present then none of the prefixes that "ab" is is a subset need
  // be examined.

  // The same argument goes for suffix string that are reversed.

  // Then to top this off why not examine the first char of the word
  // to quickly limit the set of prefixes to examine (i.e. the
  // prefixes to examine must be leading supersets of the first
  // character of the word (if they exist)
 
  // To take advantage of this "subset" relationship, we need to add
  // two links from entry.  One to take next if the current prefix
  // is found (call it nexteq) and one to take next if the current
  // prefix is not found (call it nextne).

  // Since we have built ordered lists, all that remains is to
  // properly intialize the nextne and nexteq pointers that relate
  // them

  process_pfx_order();
  process_sfx_order();

  return no_err;

}


// we want to be able to quickly access prefix information
// both by prefix flag, and sorted by prefix string itself
// so we need to set up two indexes

PosibErr<void> AffixMgr::build_pfxlist(PfxEntry* pfxptr)
{
  PfxEntry * ptr;
  PfxEntry * pptr;
  PfxEntry * ep = pfxptr;

  // get the right starting point 
  const char * key = ep->key();
  const byte flg = ep->flag();

  // first index by flag which must exist
  ptr = pFlag[flg];
  ep->flag_next = ptr;
  pFlag[flg] = ep;

  // next index by affix string

  // handle the special case of null affix string
  if (strlen(key) == 0) {
    // always inset them at head of list at element 0
    ptr = pStart[0];
    ep->next = ptr;
    pStart[0] = ep;
    return no_err;
  }

  // now handle the general case
  byte sp = *((const byte *)key);
  ptr = (PfxEntry*)pStart[sp];
  
  /* handle the insert at top of list case */
  if ((!ptr) || ( strcmp( ep->key() , ptr->key() ) <= 0)) {
    ep->next = ptr;
    pStart[sp] = ep;
    return no_err;
  }

  /* otherwise find where it fits in order and insert it */
  pptr = NULL;
  for (; ptr != NULL; ptr = ptr->next) {
    if (strcmp( ep->key() , ptr->key() ) <= 0) break;
    pptr = ptr;
  }
  pptr->next = ep;
  ep->next = ptr;
  return no_err;
}


// we want to be able to quickly access suffix information
// both by suffix flag, and sorted by the reverse of the
// suffix string itself; so we need to set up two indexes

PosibErr<void> AffixMgr::build_sfxlist(SfxEntry* sfxptr)
{
  SfxEntry * ptr;
  SfxEntry * pptr;
  SfxEntry * ep = sfxptr;
  sfxptr->rappnd = (char *)strings.alloc(sfxptr->appndl + 1);
  // reverse the string
  sfxptr->rappnd[sfxptr->appndl] = 0;
  for (char * dest = sfxptr->rappnd + sfxptr->appndl - 1, * src = sfxptr->appnd;
       dest >= sfxptr->rappnd;
       --dest, ++src)
    *dest = *src;

  /* get the right starting point */
  const char * key = ep->key();
  const byte flg = ep->flag();


  // first index by flag which must exist
  ptr = sFlag[flg];
  ep->flag_next = ptr;
  sFlag[flg] = ep;


  // next index by affix string
    
  // handle the special case of null affix string
  if (strlen(key) == 0) {
    // always inset them at head of list at element 0
    ptr = sStart[0];
    ep->next = ptr;
    sStart[0] = ep;
    return no_err;
  }

  // now handle the normal case
  byte sp = *((const byte *)key);
  ptr = sStart[sp];
  
  /* handle the insert at top of list case */
  if ((!ptr) || ( strcmp( ep->key() , ptr->key() ) <= 0)) {
    ep->next = ptr;
    sStart[sp] = ep;
    return no_err;
  }

  /* otherwise find where it fits in order and insert it */
  pptr = NULL;
  for (; ptr != NULL; ptr = ptr->next) {
    if (strcmp( ep->key(), ptr->key() ) <= 0) break;
    pptr = ptr;
  }
  pptr->next = ep;
  ep->next = ptr;
  return no_err;
}



// initialize the PfxEntry links NextEQ and NextNE to speed searching
PosibErr<void> AffixMgr::process_pfx_order()
{
  PfxEntry* ptr;

  // loop through each prefix list starting point
  for (int i=1; i < SETSIZE; i++) {

    ptr = pStart[i];

    // look through the remainder of the list
    //  and find next entry with affix that 
    // the current one is not a subset of
    // mark that as destination for NextNE
    // use next in list that you are a subset
    // of as NextEQ

    for (; ptr != NULL; ptr = ptr->next) {

      PfxEntry * nptr = ptr->next;
      for (; nptr != NULL; nptr = nptr->next) {
        if (! isSubset( ptr->key() , nptr->key() )) break;
      }
      ptr->next_ne = nptr;
      ptr->next_eq = NULL;
      if ((ptr->next) && isSubset(ptr->key() , 
                                  (ptr->next)->key())) 
        ptr->next_eq = ptr->next;
    }

    // now clean up by adding smart search termination strings:
    // if you are already a superset of the previous prefix
    // but not a subset of the next, search can end here
    // so set NextNE properly

    ptr = (PfxEntry *) pStart[i];
    for (; ptr != NULL; ptr = ptr->next) {
      PfxEntry * nptr = ptr->next;
      PfxEntry * mptr = NULL;
      for (; nptr != NULL; nptr = nptr->next) {
        if (! isSubset(ptr->key(),nptr->key())) break;
        mptr = nptr;
      }
      if (mptr) mptr->next_ne = NULL;
    }
  }
  return no_err;
}



// initialize the SfxEntry links NextEQ and NextNE to speed searching
PosibErr<void> AffixMgr::process_sfx_order()
{
  SfxEntry* ptr;

  // loop through each prefix list starting point
  for (int i=1; i < SETSIZE; i++) {

    ptr = sStart[i];

    // look through the remainder of the list
    //  and find next entry with affix that 
    // the current one is not a subset of
    // mark that as destination for NextNE
    // use next in list that you are a subset
    // of as NextEQ

    for (; ptr != NULL; ptr = ptr->next) {
      SfxEntry * nptr = ptr->next;
      for (; nptr != NULL; nptr = nptr->next) {
        if (! isSubset(ptr->key(),nptr->key())) break;
      }
      ptr->next_ne = nptr;
      ptr->next_eq = NULL;
      if ((ptr->next) && isSubset(ptr->key(),(ptr->next)->key())) 
        ptr->next_eq = ptr->next;
    }


    // now clean up by adding smart search termination strings:
    // if you are already a superset of the previous suffix
    // but not a subset of the next, search can end here
    // so set NextNE properly

    ptr = (SfxEntry *) sStart[i];
    for (; ptr != NULL; ptr = ptr->next) {
      SfxEntry * nptr = ptr->next;
      SfxEntry * mptr = NULL;
      for (; nptr != NULL; nptr = nptr->next) {
        if (! isSubset(ptr->key(),nptr->key())) break;
        mptr = nptr;
      }
      if (mptr) mptr->next_ne = NULL;
    }
  }
  return no_err;
}



// takes aff file condition string and creates the
// conds array - please see the appendix at the end of the
// file affentry.cxx which describes what is going on here
// in much more detail

void AffixMgr::encodeit(AffEntry * ptr, const char * cs)
{
  byte c;
  int i, j, k;
  byte mbr[MAXLNLEN];

  // now clear the conditions array */
  for (i=0;i<SETSIZE;i++) ptr->conds[i] = (byte) 0;

  // now parse the string to create the conds array */
  int nc = strlen(cs);
  int neg = 0;   // complement indicator
  int grp = 0;   // group indicator
  int n = 0;     // number of conditions
  int ec = 0;    // end condition indicator
  int nm = 0;    // number of member in group

  // if no condition just return
  if (strcmp(cs,".")==0) {
    ptr->numconds = 0;
    return;
  }

  i = 0;
  while (i < nc) {
    c = *((byte *)(cs + i));

    // start group indicator
    if (c == '[') {
      grp = 1;
      c = 0;
    }

    // complement flag
    if ((grp == 1) && (c == '^')) {
      neg = 1;
      c = 0;
    }

    // end goup indicator
    if (c == ']') {
      ec = 1;
      c = 0;
    }

    // add character of group to list
    if ((grp == 1) && (c != 0)) {
      *(mbr + nm) = c;
      nm++;
      c = 0;
    }

    // end of condition 
    if (c != 0) {
      ec = 1;
    }

    
    if (ec) {
      if (grp == 1) {
        if (neg == 0) {
          // set the proper bits in the condition array vals for those chars
          for (j=0;j<nm;j++) {
            k = (unsigned int) mbr[j];
            ptr->conds[k] = ptr->conds[k] | (1 << n);
          }
        } else {
          // complement so set all of them and then unset indicated ones
          for (j=0;j<SETSIZE;j++) ptr->conds[j] = ptr->conds[j] | (1 << n);
          for (j=0;j<nm;j++) {
            k = (unsigned int) mbr[j];
            ptr->conds[k] = ptr->conds[k] & ~(1 << n);
          }
        }
        neg = 0;
        grp = 0;   
        nm = 0;
      } else {
        // not a group so just set the proper bit for this char
        // but first handle special case of . inside condition
        if (c == '.') {
          // wild card character so set them all
          for (j=0;j<SETSIZE;j++) ptr->conds[j] = ptr->conds[j] | (1 << n);
        } else {  
          ptr->conds[(unsigned int) c] = ptr->conds[(unsigned int)c] | (1 << n);
        }
      }
      n++;
      ec = 0;
    }


    i++;
  }
  ptr->numconds = n;
  return;
}


// check word for prefixes
bool AffixMgr::prefix_check (const LookupInfo & linf, ParmString word, 
                             CheckInfo & ci, GuessInfo * gi) const
{
 
  // first handle the special case of 0 length prefixes
  PfxEntry * pe = (PfxEntry *) pStart[0];
  while (pe) {
    if (pe->check(linf,word,ci,gi)) return true;
    pe = pe->next;
  }
  
  // now handle the general case
  byte sp = *reinterpret_cast<const byte *>(word.str());
  PfxEntry * pptr = (PfxEntry *)pStart[sp];

  while (pptr) {
    if (isSubset(pptr->key(),word)) {
      if (pptr->check(linf,word,ci,gi)) return true;
      pptr = pptr->next_eq;
    } else {
      pptr = pptr->next_ne;
    }
  }
    
  return false;
}


// check word for suffixes
bool AffixMgr::suffix_check (const LookupInfo & linf, ParmString word, 
                             CheckInfo & ci, GuessInfo * gi,
                             int sfxopts, AffEntry * ppfx) const
{

  // first handle the special case of 0 length suffixes
  SfxEntry * se = (SfxEntry *) sStart[0];
  while (se) {
    if (se->check(linf, word, ci, gi, sfxopts, ppfx)) return true;
    se = se->next;
  }
  
  // now handle the general case
  byte sp = *((const byte *)(word + word.size() - 1));
  SfxEntry * sptr = (SfxEntry *) sStart[sp];

  while (sptr) {
    if (isRevSubset(sptr->key(), word + word.size() - 1, word.size())) {
      if (sptr->check(linf, word, ci, gi, sfxopts, ppfx))
        return true;
      sptr = sptr->next_eq;
    } else {
      sptr = sptr->next_ne;
    }
  }
    
  return false;
}

// check if word with affixes is correctly spelled
bool AffixMgr::affix_check(const LookupInfo & linf, ParmString word, 
                           CheckInfo & ci, GuessInfo * gi) const
{
  // Deal With Case in a semi-intelligent manner
  CasePattern cp = case_pattern(*lang,word);
  ParmString pword = word;
  ParmString sword = word;
  CharVector lower;
  if (cp == FirstUpper) {
    lower.append(word, word.size() + 1);
    lower[0] = lang->to_lower(word[0]);
    pword = ParmString(lower.data(), lower.size() - 1);
  } else if (cp == AllUpper) {
    lower.resize(word.size() + 1);
    unsigned int i = 0;
    for (; i != word.size(); ++i)
      lower[i] = lang->to_lower(word[i]);
    lower[i] = '\0';
    pword = ParmString(lower.data(), lower.size() - 1);
    sword = pword;
  }

  // check all prefixes (also crossed with suffixes if allowed) 
  if (prefix_check(linf, pword, ci, gi)) return true;

  // if still not found check all suffixes
  return suffix_check(linf, sword, ci, gi, 0, NULL);
}

void AffixMgr::get_word(String & word, const CheckInfo & ci) const
{
  CasePattern cp = case_pattern(*lang,word);
  if (ci.pre_add) {
    if (cp == FirstUpper) word[0] = lang->to_lower(word[0]);
    size_t s = strlen(ci.pre_add);
    word.replace(0, strlen(ci.pre_strip), ci.pre_add, s);
    if (cp == FirstUpper) word[0] = lang->to_title(word[0]);
    else if (cp == AllUpper)
      for (size_t i = 0; i != s; ++i) word[i] = lang->to_upper(word[i]);
  }
  if (ci.suf_add) {
    size_t strip = strlen(ci.suf_strip);
    size_t s     = strlen(ci.suf_add);
    size_t start = word.size() - strip;
    word.replace(start, strip, ci.suf_add, s);
    if (cp == AllUpper)
      for (size_t i = start; i != start + s; ++i) 
	word[i] = lang->to_upper(word[i]);
  }
}


void AffixMgr::munch(ParmString word, CheckList * cl) const
{
  LookupInfo li(0, LookupInfo::AlwaysTrue);
  CheckInfo ci;
  cl->reset();
  CasePattern cp = case_pattern(*lang,word);
  if (cp == AllUpper) return;
  if (cp != FirstUpper)
    prefix_check(li, word, ci, &cl->gi);
  suffix_check(li, word, ci, &cl->gi, 0, NULL);
}

WordAff * AffixMgr::expand(ParmString word, ParmString aff, 
                           ObjStack & buf, int limit) const
{
  byte * empty = (byte *)buf.alloc(1);
  *empty = 0;

  byte * suf  = (byte *)buf.alloc(aff.size() + 1); 
  byte * suf_e = suf;
  byte * csuf = (byte *)buf.alloc(aff.size() + 1); 
  byte * csuf_e = csuf;

  WordAff * head = (WordAff *)buf.alloc_bottom(sizeof(WordAff));
  WordAff * cur = head;
  cur->word = buf.dup(word);
  cur->aff  = suf;

  for (const byte * c = (const byte *)aff.str(), * end = c + aff.size();
       c != end; 
       ++c) 
  {
    if (sFlag[*c]) *suf_e++ = *c; 
    if (sFlag[*c] && sFlag[*c]->allow_cross()) *csuf_e++ = *c;
    
    for (PfxEntry * p = pFlag[*c]; p; p = p->flag_next) {
      SimpleString newword = p->add(word, buf);
      if (!newword) continue;
      cur->next = (WordAff *)buf.alloc_bottom(sizeof(WordAff));
      cur = cur->next;
      cur->word = newword;
      cur->aff = p->allow_cross() ? csuf : empty;
      break;
    }
  }

  *suf_e = 0;
  *csuf_e = 0;
  cur->next = 0;

  if (limit == 0) return head;

  WordAff * * end = &cur->next;
  WordAff * * very_end = end;
  size_t nsuf_s = suf_e - suf + 1;

  for (WordAff * * cur = &head; cur != end; cur = &(*cur)->next) {
    if ((int)(*cur)->word.size - max_strip_ >= limit) continue;
    byte * nsuf = (byte *)buf.alloc(nsuf_s);
    expand_suffix((*cur)->word, (*cur)->aff, buf, limit, nsuf, &very_end);
    (*cur)->aff = nsuf;
  }

  return head;
}

WordAff * AffixMgr::expand_suffix(ParmString word, const byte * aff, 
                                  ObjStack & buf, int limit,
                                  byte * new_aff, WordAff * * * l) const
{
  WordAff * head = 0;
  if (l) head = **l;
  WordAff * * cur = l ? *l : &head;

  while (*aff) {
    if ((int)word.size() - max_strip_f[*aff] >= limit) goto not_expanded;
    for (SfxEntry * p = sFlag[*aff]; p; p = p->flag_next) {
      SimpleString newword = p->add(word, buf, limit);
      if (!newword) continue;
      if (newword == EMPTY) goto not_expanded;
      *cur = (WordAff *)buf.alloc_bottom(sizeof(WordAff));
      (*cur)->word = newword;
      (*cur)->aff  = (const byte *)EMPTY;
      cur = &(*cur)->next;
      goto expanded;
    }
  not_expanded:
    if (new_aff) *new_aff++ = *aff;
  expanded:
    ++aff;
  }
  *cur = 0;
  if (new_aff) *new_aff = 0;
  if (l) *l = cur;
  return head;
}


//////////////////////////////////////////////////////////////////////
//
// LookupInfo
//

static void free_word(WordEntry * w)
{
  free((void *)w->word);
}

static void free_aff(WordEntry * w)
{
  free((void *)w->aff);
}

struct LookupBookkepping
{
  const char * aff;
  bool alloc;
  LookupBookkepping() : aff(0), alloc(false) {}
};

static void append_aff(LookupBookkepping & s, WordEntry & o)
{
  size_t s0 = strlen(s.aff);
  size_t s1 = strlen(o.aff);
  if (s0 == s1 && memcmp(s.aff, o.aff, s0) == 0) return;
  char * tmp = (char *)malloc(s0 + s1 + 1);
  // FIXME: avoid adding duplicate flags
  memcpy(tmp, o.aff, s1);
  memcpy(tmp + s1, s.aff, s0);
  tmp[s0 + s1] = '\0';
  if (s.alloc) free((void *)s.aff);
  s.aff = tmp;
  s.alloc = true;
}

// FIXME: There are some problems with stripped lookup when accents
//   are in the word AND the affix conditions depends on those accents
//   being there as they WONT BE in the stripped word.

inline bool LookupInfo::lookup (ParmString word, WordEntry & o) const
{
  SpellerImpl::WS::const_iterator i = begin;
  const char * w = 0;
  LookupBookkepping s;
  if (mode == Word) {
    do {
      if (i->ws->lookup(word, o, i->cmp)) {
        w = o.word;
        if (s.aff == 0) s.aff = o.aff;
        else append_aff(s, o); // this should not be a very common case
      }
      ++i;
    } while (i != end);
  } else if (mode == Soundslike) {
    do {
      if (i->ws->soundslike_lookup(word, o)) {
        w = o.word;
        if (s.aff == 0) s.aff = o.aff;
        else append_aff(s, o); // this should not be a very common case
        while (o.adv()) append_aff(s, o); // neither should this
      }
      ++i;
    } while (i != end);
  } else {
    o.word = strdup(word);
    o.aff  = word + strlen(word);
    //o.free_ = free_word; //FiXME this isn't right....
    return true;
  }
  if (!w) return false;
  o.word = w;
  o.aff = s.aff;
  if (s.alloc) o.free_ = free_aff;
  return true;
}

//////////////////////////////////////////////////////////////////////
//
// Affix Entry
//

// add prefix to this word assuming conditions hold
SimpleString PfxEntry::add(SimpleString word, ObjStack & buf) const
{
  int cond;
  /* make sure all conditions match */
  if ((word.size > stripl) && (word.size >= numconds)) {
    const byte * cp = (const byte *) word.str;
    for (cond = 0;  cond < numconds;  cond++) {
      if ((conds[*cp++] & (1 << cond)) == 0)
        break;
    }
    if (cond >= numconds) {
      /* */
      int alen = word.size - stripl;
      char * newword = (char *)buf.alloc(alen + appndl + 1);
      if (appndl) memcpy(newword, appnd, appndl);
      memcpy(newword + appndl, word + stripl, alen + 1);
      return SimpleString(newword, alen + appndl);
    }
  }
  return SimpleString();
}

// check if this prefix entry matches 
bool PfxEntry::check(const LookupInfo & linf, ParmString word,
                     CheckInfo & ci, GuessInfo * gi) const
{
  int			cond;	// condition number being examined
  int	                tmpl;   // length of tmpword
  WordEntry             wordinfo;     // hash entry of root word or NULL
  byte *	cp;		
  char	        tmpword[MAXWORDLEN+1];


  // on entry prefix is 0 length or already matches the beginning of the word.
  // So if the remaining root word has positive length
  // and if there are enough chars in root word and added back strip chars
  // to meet the number of characters conditions, then test it

  tmpl = word.size() - appndl;

  if ((tmpl > 0) &&  (tmpl + stripl >= numconds)) {

    // generate new root word by removing prefix and adding
    // back any characters that would have been stripped

    if (stripl) strcpy (tmpword, strip);
    strcpy ((tmpword + stripl), (word + appndl));

    // now make sure all of the conditions on characters
    // are met.  Please see the appendix at the end of
    // this file for more info on exactly what is being
    // tested

    cp = (byte *)tmpword;
    for (cond = 0;  cond < numconds;  cond++) {
      if ((conds[*cp++] & (1 << cond)) == 0) break;
    }

    // if all conditions are met then check if resulting
    // root word in the dictionary

    if (cond >= numconds) {
      CheckInfo * lci = 0;
      tmpl += stripl;
      if (linf.lookup(tmpword, wordinfo)) {

        if (TESTAFF(wordinfo.aff, achar))
          lci = &ci;
        else if (gi)
          lci = gi->add();

        if (lci)
          lci->word = wordinfo.word;

      } else {
              
        // prefix matched but no root word was found 
        // if XPRODUCT is allowed, try again but now 
        // cross checked combined with a suffix
                
        if (gi)
          lci = gi->last;
                
        if (xpflg & XPRODUCT) {
          if (pmyMgr->suffix_check(linf, ParmString(tmpword, tmpl), 
                                   ci, gi,
                                   XPRODUCT, (AffEntry *)this))
            lci = &ci;
          else if (gi && gi->last != lci) {
            while (lci = const_cast<CheckInfo *>(lci->next), lci) {
              lci->pre_flag = achar;
              lci->pre_add = appnd;
              lci->pre_strip = strip;
            }
          }
        }
      }
              
      if (lci) {
        lci->pre_flag = achar;
        lci->pre_add = appnd;
        lci->pre_strip = strip;
      }
      if (lci ==&ci) return true;
    }
  }
  return false;
}

// add suffix to this word assuming conditions hold
SimpleString SfxEntry::add(SimpleString word, ObjStack & buf, int limit) const
{
  int cond;
  /* make sure all conditions match */
  if ((word.size > stripl) && (word.size >= numconds)) {
    const byte * cp = (const byte *) (word + word.size);
    for (cond = numconds; --cond >=0; ) {
      if ((conds[*--cp] & (1 << cond)) == 0)
        break;
    }
    if (cond < 0) {
      int alen = word.size - stripl;
      if (alen >= limit) return EMPTY;
      /* we have a match so add suffix */
      char * newword = (char *)buf.alloc(alen + appndl + 1);
      memcpy(newword, word, alen);
      memcpy(newword + alen, appnd, appndl + 1);
      return SimpleString(newword, alen + appndl);
    }
  }
  return SimpleString();
}

// see if this suffix is present in the word 
bool SfxEntry::check(const LookupInfo & linf, ParmString word,
                     CheckInfo & ci, GuessInfo * gi,
                     int optflags, AffEntry* ppfx)
{
  int	                tmpl;		 // length of tmpword 
  int			cond;		 // condition beng examined
  WordEntry             wordinfo;        // hash entry pointer
  byte *	cp;
  char	        tmpword[MAXWORDLEN+1];
  PfxEntry* ep = (PfxEntry *) ppfx;


  // if this suffix is being cross checked with a prefix
  // but it does not support cross products skip it

  if ((optflags & XPRODUCT) != 0 &&  (xpflg & XPRODUCT) == 0)
    return false;

  // upon entry suffix is 0 length or already matches the end of the word.
  // So if the remaining root word has positive length
  // and if there are enough chars in root word and added back strip chars
  // to meet the number of characters conditions, then test it

  tmpl = word.size() - appndl;

  if ((tmpl > 0)  &&  (tmpl + stripl >= numconds)) {

    // generate new root word by removing suffix and adding
    // back any characters that would have been stripped or
    // or null terminating the shorter string

    strcpy (tmpword, word);
    cp = (byte *)(tmpword + tmpl);
    if (stripl) {
      strcpy ((char *)cp, strip);
      tmpl += stripl;
      cp = (byte *)(tmpword + tmpl);
    } else *cp = '\0';

    // now make sure all of the conditions on characters
    // are met.  Please see the appendix at the end of
    // this file for more info on exactly what is being
    // tested

    for (cond = numconds;  --cond >= 0; ) {
      if ((conds[*--cp] & (1 << cond)) == 0) break;
    }

    // if all conditions are met then check if resulting
    // root word in the dictionary

    if (cond < 0) {
      CheckInfo * lci = 0;
      tmpl += stripl;
      if (linf.lookup(tmpword, wordinfo)) {
        if (TESTAFF(wordinfo.aff, achar) && 
            ((optflags & XPRODUCT) == 0 || 
             TESTAFF(wordinfo.aff, ep->achar)))
          lci = &ci;
        else if (gi)
          lci = gi->add();

        if (lci) {
          lci->word = wordinfo.word;
          lci->suf_flag = achar;
          lci->suf_add = appnd;
          lci->suf_strip = strip;
        }
        
        if (lci == &ci) return true;
      }
    }
  }
  return false;
}

//////////////////////////////////////////////////////////////////////
//
// new_affix_mgr
//


PosibErr<AffixMgr *> new_affix_mgr(ParmString name, 
                                   Conv & iconv,
                                   const Language * lang)
{
  if (name == "none")
    return 0;
  //CERR << "NEW AFFIX MGR\n";
  String file;
  file += lang->data_dir();
  file += '/';
  file += lang->name();
  file += "_affix.dat";
  AffixMgr * affix;
  affix = new AffixMgr(lang);
  PosibErrBase pe = affix->setup(file, iconv);
  if (pe.has_err()) {
    delete affix;
    return pe;
  } else {
    return affix;
  }
}
}

/**************************************************************************

Appendix:  Understanding Affix Code


An affix is either a  prefix or a suffix attached to root words to make 
other words.

Basically a Prefix or a Suffix is set of AffEntry objects
which store information about the prefix or suffix along 
with supporting routines to check if a word has a particular 
prefix or suffix or a combination.

The structure affentry is defined as follows:

struct AffEntry
{
   unsigned char achar;   // char used to represent the affix
   char * strip;          // string to strip before adding affix
   char * appnd;          // the affix string to add
   short  stripl;         // length of the strip string
   short  appndl;         // length of the affix string
   short  numconds;       // the number of conditions that must be met
   short  xpflg;          // flag: XPRODUCT- combine both prefix and suffix 
   char   conds[SETSIZE]; // array which encodes the conditions to be met
};


Here is a suffix borrowed from the en_US.aff file.  This file 
is whitespace delimited.

SFX D Y 4 
SFX D   0     e          d
SFX D   y     ied        [^aeiou]y
SFX D   0     ed         [^ey]
SFX D   0     ed         [aeiou]y

This information can be interpreted as follows:

In the first line has 4 fields

Field
-----
1     SFX - indicates this is a suffix
2     D   - is the name of the character flag which represents this suffix
3     Y   - indicates it can be combined with prefixes (cross product)
4     4   - indicates that sequence of 4 affentry structures are needed to
               properly store the affix information

The remaining lines describe the unique information for the 4 SfxEntry 
objects that make up this affix.  Each line can be interpreted
as follows: (note fields 1 and 2 are as a check against line 1 info)

Field
-----
1     SFX         - indicates this is a suffix
2     D           - is the name of the character flag for this affix
3     y           - the string of chars to strip off before adding affix
                         (a 0 here indicates the NULL string)
4     ied         - the string of affix characters to add
5     [^aeiou]y   - the conditions which must be met before the affix
                    can be applied

Field 5 is interesting.  Since this is a suffix, field 5 tells us that
there are 2 conditions that must be met.  The first condition is that 
the next to the last character in the word must *NOT* be any of the 
following "a", "e", "i", "o" or "u".  The second condition is that
the last character of the word must end in "y".

So how can we encode this information concisely and be able to 
test for both conditions in a fast manner?  The answer is found
but studying the wonderful ispell code of Geoff Kuenning, et.al. 
(now available under a normal BSD license).

If we set up a conds array of 256 bytes indexed (0 to 255) and access it
using a character (cast to an unsigned char) of a string, we have 8 bits
of information we can store about that character.  Specifically we
could use each bit to say if that character is allowed in any of the 
last (or first for prefixes) 8 characters of the word.

Basically, each character at one end of the word (up to the number 
of conditions) is used to index into the conds array and the resulting 
value found there says whether the that character is valid for a 
specific character position in the word.  

For prefixes, it does this by setting bit 0 if that char is valid 
in the first position, bit 1 if valid in the second position, and so on. 

If a bit is not set, then that char is not valid for that postion in the
word.

If working with suffixes bit 0 is used for the character closest 
to the front, bit 1 for the next character towards the end, ..., 
with bit numconds-1 representing the last char at the end of the string. 

Note: since entries in the conds[] are 8 bits, only 8 conditions 
(read that only 8 character positions) can be examined at one
end of a word (the beginning for prefixes and the end for suffixes.

So to make this clearer, lets encode the conds array values for the 
first two affentries for the suffix D described earlier.


  For the first affentry:    
     numconds = 1             (only examine the last character)

     conds['e'] =  (1 << 0)   (the word must end in an E)
     all others are all 0

  For the second affentry:
     numconds = 2             (only examine the last two characters)     

     conds[X] = conds[X] | (1 << 0)     (aeiou are not allowed)
         where X is all characters *but* a, e, i, o, or u
         

     conds['y'] = (1 << 1)     (the last char must be a y)
     all other bits for all other entries in the conds array are zero


**************************************************************************/
