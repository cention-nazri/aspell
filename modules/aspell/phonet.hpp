/*  phonetic.c - generic replacement aglogithms for phonetic transformation
    Copyright (C) 2000 Björn Jacke

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License version 2.1 as published by the Free Software Foundation;
 
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.
 
    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Björn Jacke may be reached by email at bjoern.jacke@gmx.de
*/

#ifndef ASPELL_PHONET__HPP
#define ASPELL_PHONET__HPP

#include "string.hpp"
#include "posib_err.hpp"

using namespace pcommon;

namespace aspell {

  struct PhonetParms {
    String version;
    
    bool followup;
    bool collapse_result;

    static const char * const rules_end;
    const char * * rules;

    char to_upper[256];
    bool is_alpha[256];

    static const int hash_size = 256;
    int hash[hash_size];

    virtual PhonetParms * clone() const = 0;
    virtual void assign(const PhonetParms *) = 0;
    virtual ~PhonetParms() {}
  };

  void init_phonet_charinfo(PhonetParms & parms);
  void init_phonet_hash(PhonetParms & parms);
  int phonet (const char * inword, char * target, 
	      const PhonetParms & parms);

#if 0
  void dump_phonet_rules(std::ostream & out, const PhonetParms & parms);
  // the istream must be seekable
#endif

  PosibErr<PhonetParms *> load_phonet_rules(const String & file);

}

#endif
