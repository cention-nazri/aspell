// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef PCOMMON_POSIB_ERR__HPP
#define PCOMMON_POSIB_ERR__HPP

#include "string.hpp"
#include "error.hpp"

namespace acommon {

  // PosibErr<type> is a special Error handling device that will make
  // sure that an error is properly handled.  It is expected to be
  // used as the return type of the function It will automatitcly
  // convert to the "normal" return type however if the normal
  // returned type is accessed and there is an "unhandled" error
  // condition it will abort It will also abort if the object is
  // destroyed with an "unhandled" error condition.  This includes
  // ignoring the return type of a function returing an error
  // condition.  An error condition is handled by simply checking for
  // the precence of an error, calling ignore, or taking owenership of
  // the error.

  enum WhichErr { PrimErr, SecErr };

  extern "C" const ErrorInfo * const perror_bad_file_format;

  // FIXME: Avoid memory leak with ErrPtr and Error, 
  //        and in particular err->mesg
  
  struct ErrPtr {
    const Error * err;
    bool handled;
    int refcount;
    ErrPtr(const Error * e) : err(e), handled(false), refcount(1) {}
  };
  
  class PosibErrBase {
  public:
    PosibErrBase() 
      : err_(0), has_data_(false) {}
    // If the derived type has the potential for data (has_data_ is true)
    // then its copy constructor and assigment operator calls
    // copy and destroy directly overriding the comptaibly check
    PosibErrBase(const PosibErrBase & other) 
      : has_data_(false)
    {
      copy(other);
    }
    PosibErrBase& operator= (const PosibErrBase & other) {
      posib_handle_incompat_assign(other);
      copy(other);
      return *this;
    }
    Error * release_err() {
      if (err_ == 0)
	return 0;
      else
	return release();
    }
    void ignore_err() {
      if (err_ != 0)
	err_->handled = true;
    }
    const Error * get_err() const {
      if (err_ == 0) {
	return 0;
      } else {
	err_->handled = true;
	return err_->err;
      }
    }
    const Error * prvw_err() const {
      if (err_ == 0)
	return 0;
      else
	return err_->err;
    }
    bool has_err() const {
      return err_ != 0;
    }
    bool has_err(const ErrorInfo * e) const {
      if (err_ == 0) {
	return false;
      } else {
	if (err_->err->is_a(e)) {
	  err_->handled = true;
	  return true;
	} else {
	  return false;
	}
      }
    }
    PosibErrBase & prim_err(const ErrorInfo * inf, ParmString p1 = 0,
			    ParmString p2 = 0, ParmString p3 = 0, 
			    ParmString p4 = 0)
    {
      return set(inf, p1, p2, p3, p4);
    }

    // This should only be called _after_ set is called
    PosibErrBase & with_file(ParmString fn);
    
    PosibErrBase & set(const ErrorInfo *, 
		       ParmString, ParmString, ParmString, ParmString);
    
    ~PosibErrBase() {
      destroy();
    }

  protected:

    PosibErrBase(bool hd) 
      : err_(0), has_data_(hd) {}

    void posib_handle_err() const {
      if (err_ && !err_->handled)
	handle_err();
    }

    void copy(const PosibErrBase & other) {
      err_ = other.err_;
      if (err_) {
	++ err_->refcount;
      }
    }
    void destroy() {
      if (err_ == 0) return;
      -- err_->refcount;
      if (err_->refcount == 0) {
	if (!err_->handled)
	  handle_err();
	del();
      }
    }
    void posib_handle_incompat_assign(const PosibErrBase & other) {
      if (has_data_ && other.has_data_ && !other.err_)
	handle_incompat_assign();
    }
    
  private:

    void handle_err() const;
    void handle_incompat_assign() const;
    Error * release();
    void del();
    ErrPtr * err_;
    bool has_data_; // has the *potential* of having data

  };

  template <typename Ret>
  class PosibErr : public PosibErrBase
  {
  public:
    PosibErr()
      : PosibErrBase(true), data() {}
    PosibErr(const PosibErrBase & other) 
      : PosibErrBase(true), data()
    {
      posib_handle_incompat_assign(other);
      PosibErrBase::copy(other);
    }
    PosibErr(const PosibErr & other)
      : PosibErrBase(true), data(other.data) 
    {
      PosibErrBase::copy(other);
    }
    PosibErr& operator= (const PosibErr & other) {
      data = other.data;
      PosibErrBase::destroy();
      PosibErrBase::copy(other);
      return *this;
    }
    PosibErr(const Ret & d) : data(d) {}
    operator const Ret & () const {posib_handle_err(); return data;}
    
    Ret data;
  };

  template <>
  class PosibErr<void> : public PosibErrBase
  {
  public:
    PosibErr(const PosibErrBase & other) 
      : PosibErrBase(other) {}
    PosibErr() {}
  };

//
//
//
#define RET_ON_ERR_SET(command, type, var) \
  type var;do{PosibErr<type> pe=command;if(pe.has_err())return pe;var=pe.data;} while(false)
#define RET_ON_ERR(command) \
  do{PosibErrBase pe = command;if(pe.has_err())return pe;}while(false)

  
  //
  //
  //

  static inline PosibErrBase make_err(const ErrorInfo * inf, 
				      ParmString p1 = 0, ParmString p2 = 0,
				      ParmString p3 = 0, ParmString p4 = 0)
  {
    return PosibErrBase().prim_err(inf, p1, p2, p3, p4);
  }

  static const PosibErrBase no_err;

  //
  //
  //

  inline String & String::operator= (const PosibErr<const char *> & s)
  {
    std::string::operator=(s.data);
    return *this;
  }

  //inline String & String::operator= (const PosibErr<String> & s)
  //{
  //  std::string::operator=(s.data);
  //  return *this;
  //}

  inline ParmString::ParmString(const PosibErr<const char *> & s)
    : str_(s.data) {}

  inline ParmString::ParmString(const PosibErr<String> & s)
    : str_(s.data.c_str()), size_(s.data.size()) {}

}

#endif
