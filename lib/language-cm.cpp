/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#include "convert.hpp"
#include "error.hpp"
#include "language.hpp"
#include "language-c.hpp"
#include "language_types.hpp"
#include "mutable_string.hpp"
#include "posib_err.hpp"
#include "string_enumeration.hpp"
#include "word_list.hpp"
#include "lang_impl.hpp"

namespace acommon {

class CanHaveError;
class Config;
struct Error;
class Language;
struct MunchListParms;

extern "C" CanHaveError * new_aspell_language(Config * config)
{
  PosibErr<const LangBase *> ret = new_language(config);
  if (ret.has_err()) return new CanHaveError(ret.release_err());

  StackPtr<Language> lang(new Language(ret));

  const char * sys_enc = lang->real->charmap();
  String user_enc = config->retrieve("encoding");
  if (user_enc == "none") user_enc = sys_enc;

  PosibErr<FullConvert *> conv;
  conv = new_full_convert(*config, user_enc, sys_enc, NormFrom);
  if (conv.has_err()) return new CanHaveError(conv.release_err());
  lang->to_internal_.reset(conv);
  conv = new_full_convert(*config, sys_enc, user_enc, NormTo);
  if (conv.has_err()) return new CanHaveError(conv.release_err());
  lang->from_internal_.reset(conv);

  return lang.release();
}

class MunchWordList : public WordList {
public:
  GuessInfo gi;
  bool empty() const {return !gi.head;}
  unsigned int size() const;
  StringEnumeration * elements() const;
};

class MunchStringEnumeration : public StringEnumeration {
public:
  IntrCheckInfo * ci;
  String buf;
  MunchStringEnumeration(IntrCheckInfo * ci0) : ci(ci0) {}
  bool at_end() const {return ci == 0;}
  const char * next();
  StringEnumeration * clone() const 
    {return new MunchStringEnumeration(*this);}
  void assign(const StringEnumeration * other) 
    {*this = *static_cast<const MunchStringEnumeration *>(other);}
};
  
unsigned int MunchWordList::size() const
{
  unsigned s = 0;
  for (const IntrCheckInfo * ci = gi.head; ci; ci = ci->next) ++s;
  return s;
}

StringEnumeration * MunchWordList::elements() const
{
  return new MunchStringEnumeration(gi.head);
}

const char * MunchStringEnumeration::next()
{
  buf.clear();
  buf << ci->word;
  if (ci->pre_flag || ci->suf_flag) {
    buf << '/';
    if (ci->pre_flag) buf << ci->pre_flag;
    if (ci->suf_flag) buf << ci->suf_flag;
  }
  return buf.str();
}

extern "C" const WordList * aspell_language_munch(Language * ths, const char * str, int str_size)
{
  if (!ths->munch_wl) ths->munch_wl = new MunchWordList;
  ths->munch_wl->from_internal_ = ths->from_internal_;
  ths->temp_str_0.clear();
  ths->to_internal_->convert(str, str_size, ths->temp_str_0);
  ths->real->munch(ths->temp_str_0, &ths->munch_wl->gi);
  return ths->munch_wl;
}

class ExpandWordList : public WordList {
public:
  ObjStack exp_buf;
  WordAff * exp_list;
  int limit;
  bool empty() const {return !exp_list;}
  unsigned int size() const;
  StringEnumeration * elements() const;
};

class ExpandStringEnumeration : public StringEnumeration {
public:
  WordAff * p;
  String buf;
  int limit;
  ExpandStringEnumeration(WordAff * p0, int l0) 
    :  p(p0), limit(l0) {}
  bool at_end() const {return p == 0;}
  const char * next();
  StringEnumeration * clone() const 
    {return new ExpandStringEnumeration(*this);}
  void assign(const StringEnumeration * other) 
    {*this = *static_cast<const ExpandStringEnumeration *>(other);}
};

unsigned int ExpandWordList::size() const
{
  unsigned s = 0;
  for (WordAff * p = exp_list; p; p = p->next) ++s;
  return s;
}

StringEnumeration * ExpandWordList::elements() const
{
  return new ExpandStringEnumeration(exp_list, limit);
}

const char * ExpandStringEnumeration::next()
{
  buf.clear();
  buf << p->word;
  if (limit < INT_MAX && p->aff[0]) buf << '/' << (const char *)p->aff;
  return buf.str();
}

extern "C" const WordList * aspell_language_expand(Language * ths, const char * str, int str_size, int limit)
{
  if (!ths->expand_wl) ths->expand_wl = new ExpandWordList;
  ths->expand_wl->from_internal_ = ths->from_internal_;
  ths->temp_str_0.clear();
  ths->to_internal_->convert(str, str_size, ths->temp_str_0);
  char * w = ths->temp_str_0.mstr();
  char * af = strchr(w, '/');
  size_t s;
  if (af != 0) {
    s = af - w;
    *af++ = '\0';
  } else {
    s = ths->temp_str_0.size();
    af = w + s;
  }
  ths->expand_wl->exp_buf.reset();
  ths->expand_wl->exp_list = ths->real->expand(w, af, ths->expand_wl->exp_buf, limit);
  ths->expand_wl->limit = limit;
  return ths->expand_wl;
}


Language::~Language() 
{
  delete munch_wl;
  delete expand_wl;
}

}

