#ifndef __aspeller_matrix_hh__
#define __aspeller_matrix_hh__

#include <vector>

namespace aspeller {

  class ShortMatrix {
    int x_size;
    int y_size;
    std::vector<short> data;
  public:
    ShortMatrix() {}
    void init(int s) {x_size = y_size = s; data.resize(s*s);}
    void init(int sx, int sy) {x_size = sx; y_size = sy; data.resize(sx*sy);}
    short operator() (int x, int y) const {return data[x + y*x_size];}
    short & operator() (int x, int y) {return data[x + y*x_size];}
  };

}

#endif
