#ifndef ASPELLER_DATA_ID__HPP
#define ASPELLER_DATA_ID__HPP

#include "settings.h"

#include "copy_ptr-t.hpp"

#include <sys/stat.h>

namespace aspeller {
  
  class DataSet::Id {
  public: // but don't use
    const DataSet * ptr;
    const char    * file_name;
#ifdef USE_FILE_INO
    dev_t           ino;
    ino_t           dev;
#endif
  public:
    Id(DataSet * p, const FileName & fn = FileName());
  };

}

#endif
