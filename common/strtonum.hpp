// This file is part of The New Aspell
// Copyright (C) 2002 by Melvin Hadasht and Kevin Atkinson under the
// GNU LGPL license version 2.0 or 2.1.  You should have received a
// copy of the LGPL license along with this library if you did not you
// can find it at http://www.gnu.org/.

#ifndef ASPELL_STRTONUM__HPP
#define ASPELL_STRTONUM__HPP

namespace acommon {

  double strtod_c(const char * nptr, char ** endptr);
  long strtoi_c(const char * npter,char ** endptr);

}

#endif
