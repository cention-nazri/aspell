#ifndef PSPELL_PARM_STRING__HPP
#define PSPELL_PARM_STRING__HPP

#include <string.h>
#include <limits.h>

namespace pcommon {

  template<typename Ret> class PosibErr;

  class String;

  class ParmString {
  public:
    ParmString() : str_(0) {}
    ParmString(const char * str, unsigned int sz = UINT_MAX) 
      : str_(str), size_(sz) {}
    inline ParmString(const String &);
    inline ParmString(const PosibErr<const char *> &);
    inline ParmString(const PosibErr<String> &);

    bool empty() const {
      return str_ == 0 || str_[0] == '\0';
    }
    unsigned int size() const {
      if (size_ != UINT_MAX) return size_;
      else return size_ = strlen(str_);
    }
    operator const char * () const {
      return str_;
    }
    const char * str () const {
      return str_;
    }
  private:
    const char * str_;
    mutable unsigned int size_;
  };

  inline bool operator== (ParmString s1, ParmString s2)
  {
    if (s1.str() == 0 || s2.str() == 0)
      return s1.str() == s2.str();
    return strcmp(s1,s2) == 0;
  }
  inline bool operator== (const char * s1, ParmString s2)
  {
    if (s1 == 0 || s2.str() == 0)
      return s1 == s2.str();
    return strcmp(s1,s2) == 0;
  }
  inline bool operator== (ParmString s1, const char * s2)
  {
    if (s1.str() == 0 || s2 == 0)
      return s1.str() == s2;
    return strcmp(s1,s2) == 0;
  }
  inline bool operator!= (ParmString s1, ParmString s2)
  {
    if (s1.str() == 0 || s2.str() == 0)
      return s1.str() != s2.str();
    return strcmp(s1,s2) != 0;
  }
  inline bool operator!= (const char * s1, ParmString s2)
  {
    if (s1 == 0 || s2.str() == 0)
      return s1 != s2.str();
    return strcmp(s1,s2) != 0;
  }
  inline bool operator!= (ParmString s1, const char * s2)
  {
    if (s1.str() == 0 || s2 == 0)
      return s1.str() != s2;
    return strcmp(s1,s2) != 0;
  }

}

#endif
