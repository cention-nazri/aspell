
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
