// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.

#include "config.hpp"

namespace acommon {
  
  extern const ConfigModule * filter_modules_begin;
  extern const ConfigModule * filter_modules_end;
  
  Config * new_config() {
    Config * config = new_basic_config();
    config->set_modules(filter_modules_begin, filter_modules_end);
    return config;
  }

}
