// Copyright (c) 2000
// Kevin Atkinson
//
// Permission to use, copy, modify, distribute and sell this software
// and its documentation for any purpose is hereby granted without
// fee, provided that the above copyright notice appear in all copies
// and that both that copyright notice and this permission notice
// appear in supporting documentation. Kevin Atkinson makes no
// representations about the suitability of this software for any
// purpose.  It is provided "as is" without express or implied
// warranty.

#ifndef __autil_emulation__
#define __autil_emulation__

#include "clone_ptr-t.hpp"

// An emulation is an efficient way to iterate through elements much
// like a forward iterator.  The at_end method is a convince method
// as emulations will return a null pointer or some other sort of
// special end state when they are at the end.
// Unlike an iterator iterating through x elements on a list can be 
// done in x function calls while an iterator will require 3*x.
// function calls.
// Example of emulator usage
//   const char * word;
//   while ( (word = elements->next()) != 0) { // one function call
//     cout << word << endl;
//   }
// And an iterator
//   iterator i = container->begin();
//   iterator end = container->end();
//   while (i != end) { // comparison, one function call
//     cout << *i << endl; // deref, total of two function calls
//     ++i;                // increment, total of three function calls.
//   }
// Normally all three calls are inline so it doesn't really matter
// but when the container type is not visible there are not inline
// and probably even virtual.
// If you really love iterators you can very easily wrap an emulation 
// in a forward iterator.  

namespace pcommon {

  template <typename Val>
  class VirEmulation {
  public:
    typedef Val Value;
    virtual VirEmulation * clone() const = 0;
    virtual void assign(const VirEmulation *) = 0;
    virtual Value next() = 0;
    virtual bool at_end() const = 0;
    virtual ~VirEmulation() {}
  };

  template <typename Base>
  class Emulation {
  public:
    typedef typename Base::Value Value;
    typedef Base                 VirEmul;

  private:
    ClonePtr<VirEmul> p_;

  public:
    Emulation() : p_(0) {}
    Emulation(VirEmul * p) : p_(p) {}
    Emulation(const VirEmul & p) : p_(p.clone()) {}
    Emulation & operator=(VirEmul * p) {
      p_.reset(p);
      return *this;
    }
    
    Value next() {return p_->next();}
    bool at_end() const {return p_->at_end();}
  };

  template <class Parms, class VirEmul = VirEmulation<typename Parms::Value> > 
  // Parms is expected to have the following members:
  //   typename Value
  //   typename Iterator;
  //   bool endf(Iterator)  
  //   Value end_state()
  //   Value deref(Iterator)
  class MakeVirEmulation : public VirEmul {
  public:
    typedef typename Parms::Iterator Iterator;
    typedef typename Parms::Value    Value;
  private:
    Iterator  i_;
    Parms     parms_;
  public:

    MakeVirEmulation(const Iterator i, const Parms & p = Parms()) 
      : i_(i), parms_(p) {}

    VirEmul * clone() const {
      return new MakeVirEmulation(*this);
    }

    void assign (const VirEmul * other) {
      *this = *static_cast<const MakeVirEmulation *>(other);
    }

    Value next() {
      if (parms_.endf(i_))
	return parms_.end_state();
      Value temp = parms_.deref(i_);
      ++i_;
      return temp;
    }

    bool at_end() const {
      return parms_.endf(i_);
    }
  };

  template <class Value>
  struct MakeAlwaysEndEmulationParms {
    Value end_state() const {return Value();}
  };

  template <class Value>
  struct MakeAlwaysEndEmulationParms<Value *> {
    Value * end_state() const {return 0;}
  };
  
  template <class Value> 
  class MakeAlwaysEndEmulation : public VirEmulation<Value> {
    MakeAlwaysEndEmulationParms<Value> parms_;
  public:
    MakeAlwaysEndEmulation * clone() const {
      return new MakeAlwaysEndEmulation(*this);
    }
    void assign(const VirEmulation<Value> * that) {
      *this = *static_cast<const MakeAlwaysEndEmulation *>(that);
    }
    Value next() {return parms_.end_state();}
    bool at_end() const {return true;}
  };
}

#endif

