#include <sstream>
#include "split.hpp"

using namespace std;
using namespace acommon;

namespace aspeller {

  vector<String> split(const String & str) {
    vector<String> data;
    istringstream s(str.c_str());
    String item;
    while (s >> item) {
      data.push_back(item);
    }
    return data;
  }

}
