#ifndef aspell_file_data_util__hh
#define aspell_file_data_util__hh

#include "parm_string.hpp"

using namespace pcommon;

namespace pcommon {class Config;}

namespace aspell {

  void fill_data_dir(const Config *, String & dir1, String & dir2);
  const String & find_file(String & path,
                           const String & dir1, const String & dir2, 
                           const String & name, const char * extension);
}

#endif
