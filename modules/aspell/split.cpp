#include <strstream>
#include "split.hpp"

using namespace std;
using namespace pcommon;

namespace aspell {

  vector<String> split(const String & str) {
    vector<String> data;
    istrstream s(str.c_str());
    String item;
    while (s >> item) {
      data.push_back(item);
    }
    return data;
  }

}
