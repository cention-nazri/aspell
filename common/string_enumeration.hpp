
#ifndef PSPELL_STRING_EMULATION__HPP
#define PSPELL_STRING_EMULATION__HPP

#include "parm_string.hpp"
#include "type_id.hpp"
#include "vector.hpp"

namespace pcommon {

  class StringEnumeration;

  class StringEnumeration {
  public:
    typedef const char * Value;
    virtual bool at_end() const = 0;
    virtual const char * next() = 0;
    int ref_count_;
    TypeId type_id_;
    unsigned int type_id() { return type_id_.num; }
    int copyable_;
    int copyable() { return copyable_; }
    virtual StringEnumeration * clone() const = 0;
    virtual void assign(const StringEnumeration * other) = 0;
    Vector<char> temp_str;
    void (* to_encoded_)(ParmString, Vector<char> &);
    StringEnumeration() : ref_count_(0), copyable_(2), to_encoded_(0) {}
    virtual ~StringEnumeration() {}
  };
  
}

#endif /* PSPELL_STRING_EMULATION__HPP */
