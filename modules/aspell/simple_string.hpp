
#ifndef simplestring_hh
#define simplestring_hh

#include <string.h>

namespace aspell {

  class SimpleString {
  private:
    const char * str_;
    bool         delete_;
    SimpleString & operator= (const SimpleString &);
  public:
    SimpleString() : delete_(false) {}
    SimpleString(const char * other) {
      size_t size = strlen(other)+1;
      str_ = new char[size];
      strncpy(const_cast<char *>(str_), other, size);
      delete_ = true;
    }
    SimpleString(const SimpleString & other) {
      if (other.delete_) {
	size_t size = strlen(other.str_)+1;
	str_ = new char[size];
	strncpy(const_cast<char *>(str_), other.str_, size);
	delete_ = true;
      } else {
	str_ = other.str_;
	delete_ = false;
      }
    }

    SimpleString(const char * other, int) 
      : str_(other), delete_(false) {}
    ~SimpleString() {if (delete_) delete[] str_;}
    const char * c_str() const {return str_;}
  };

  inline bool operator== (SimpleString rhs, SimpleString lhs) {
    return strcmp(lhs.c_str(), rhs.c_str()) == 0;
  }

  inline bool operator!= (SimpleString rhs, SimpleString lhs) {
    return strcmp(lhs.c_str(), rhs.c_str()) != 0;
  }

}
 
#endif
