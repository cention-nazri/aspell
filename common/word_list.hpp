#ifndef PSPELL_WORD_LIST__HPP
#define PSPELL_WORD_LIST__HPP

#include "parm_string.hpp"
#include "vector.hpp"

namespace pcommon {

class StringEmulation;

class WordList {
 public:
  void (* to_encoded_)(ParmString, Vector<char> &);
  WordList() : to_encoded_(0) {}
  virtual bool empty() const = 0;
  virtual unsigned int size() const = 0;
  virtual StringEmulation * elements() const = 0;
  virtual ~WordList() {}
};


}

#endif /* PSPELL_WORD_LIST__HPP */
