// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_GET_DATA__HPP
#define ASPELL_GET_DATA__HPP

namespace acommon {

  class IStream;
  class String;

  bool getdata_pair(IStream & in, 
		    String & key, 
		    String & data);

  void unescape(String &);
}
#endif
