#ifndef PSPELL_CAN_HAVE_ERROR__HPP
#define PSPELL_CAN_HAVE_ERROR__HPP

#include "copy_ptr.hpp"

namespace pcommon {

struct Error;

class CanHaveError {
 public:
  CopyPtr<Error> err_;
  CanHaveError(Error * e = 0) : err_(e) {}
  virtual ~CanHaveError() {}
};


}

#endif /* PSPELL_CAN_HAVE_ERROR__HPP */
