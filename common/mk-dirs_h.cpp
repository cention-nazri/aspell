// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include <cstring>
#include <iostream>

int main(int argc, const char *argv[])
{
  using namespace std;
  const char * prefix = argv[1];
  const char * key    = argv[2];
  const char * value  = argv[3];
  unsigned int prefix_len = strlen(prefix);
  while (prefix[prefix_len-1] == '/') -- prefix_len;
  if (strncmp(prefix,value,prefix_len) == 0) {
    value += prefix_len;
    while (*value == '/') ++ value;
    cout << "#define " << key << " \"<prefix:" << value << ">\"\n";
  } else {
    cout << "#define " << key << " \"" << value << "\"\n";
  }
  return 0;
}
