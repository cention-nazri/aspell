// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#ifndef PSPELL_GET_DATA__HPP
#define PSPELL_GET_DATA__HPP

namespace pcommon {

  class IStream;
  class String;

  bool getdata_pair(IStream & in, 
		    String & key, 
		    String & data);

  void unescape(String &);
}
#endif
