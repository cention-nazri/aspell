#ifndef PCOMMON_ENCODED__HPP
#define PCOMMON_ENCODED__HPP

#include "parm_string.hpp"
#include "vector.hpp"

namespace pcommon
{
  struct Encoded {
    Vector<char> temp_str_1;
    Vector<char> temp_str_2;
    void (from_encoded_ *)(ParmString, Vector<char> &);
    void (to_encoded_ *)(ParmString, Vector<char> &);
    Encoded() : from_encoded_(0), to_encoded_(0) {}
  };

}
