
#include "error.hpp"

namespace pcommon {
  bool Error::is_a(ErrorInfo const * to_find) const 
  {
    const ErrorInfo * e = err;
    while (e) {
      if (e == to_find) return true;
      e = e->isa;
    }
    return false;
  }
}
