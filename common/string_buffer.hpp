
#include "basic_list.hpp"

namespace pcommon {

  class StringBuffer {
  public: // but dont use
    static const unsigned int   buf_size = 1024 - 16;
    struct Buf {
      char buf[buf_size];
    };
  private:
    static const Buf sbuf;
    BasicList<Buf> bufs;
    unsigned int fill;
  public:
    StringBuffer();
    char * alloc(unsigned int size);
  };

}
