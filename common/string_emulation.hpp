
#ifndef PSPELL_STRING_EMULATION__HPP
#define PSPELL_STRING_EMULATION__HPP

#include "parm_string.hpp"
#include "type_id.hpp"
#include "vector.hpp"

namespace pcommon {

  class StringEmulation;

  class StringEmulation {
  public:
    typedef const char * Value;
    virtual bool at_end() const = 0;
    virtual const char * next() = 0;
    int ref_count_;
    TypeId type_id_;
    unsigned int type_id() { return type_id_.num; }
    int copyable_;
    int copyable() { return copyable_; }
    virtual StringEmulation * clone() const = 0;
    virtual void assign(const StringEmulation * other) = 0;
    Vector<char> temp_str;
    void (* to_encoded_)(ParmString, Vector<char> &);
    StringEmulation() : ref_count_(0), copyable_(2), to_encoded_(0) {}
    virtual ~StringEmulation() {}
  };
  
}

#endif /* PSPELL_STRING_EMULATION__HPP */
