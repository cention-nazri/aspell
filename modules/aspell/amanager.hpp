// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#ifndef __aspell_amanager_hh_
#define __aspell_amanager_hh_

#include "manager_impl.hpp"
#include "config.hpp"

#include <list> //FIXME: convert to BasicList

namespace aspell {

  class ManagerImpl::DataSetCollection {
    friend class ManagerImpl;
  public:
    struct Item {
      //Item() {}
      DataSet          *data_set;
      bool              use_to_check;
      bool              use_to_suggest;
      bool              save_on_saveall;
      bool              own;
      SpecialId         special_id;
      LocalWordSetInfo  local_info;
      Item(DataSet * w) : data_set(w), 
	use_to_check(false), use_to_suggest(false), 
	save_on_saveall(false), own(false), special_id(none_id)
      {}
      void set_sensible_defaults();
    };

  private:
    typedef std::list<Item> WordLists; 
  public:
    typedef WordLists::const_iterator const_iterator;
    typedef const_iterator                  iterator;
    
    const_iterator begin() const {return wordlists_.begin();}
    const_iterator end()   const {return wordlists_.end();}

    typedef const_iterator            ConstIterator;
    typedef WordLists::iterator       Iterator;

  private:


    WordLists wordlists_;

    Iterator begin() {return wordlists_.begin();}
    Iterator end()   {return wordlists_.end();}

    struct Parms {
      typedef ConstIterator      Iterator;
      typedef DataSet *    Value;
      Iterator end_;
      Parms(Iterator e) : end_(e) {}
      Value deref(Iterator i) const {return i->data_set;}
      bool endf(Iterator i) const {return i == end_;}
      Value end_state() const {return 0;}
    };

    Iterator locate(const DataSet::Id & to_find)
    {
      Iterator   i = wordlists_.begin();
      Iterator end = wordlists_.end();
      while (i != end && i->data_set->id() != to_find) ++i;
      return i;
    }

    ConstIterator locate(const DataSet::Id & to_find) const
    {
      ConstIterator   i = wordlists_.begin();
      ConstIterator end = wordlists_.end();
      while (i != end && i->data_set->id() != to_find) ++i;
      return i;
    }

    Iterator locate(SpecialId to_find)
    {
      Iterator   i = wordlists_.begin();
      Iterator end = wordlists_.end();
      while (i != end && i->special_id != to_find) ++i;
      return i;
    }

    ConstIterator locate(SpecialId to_find) const
    {
      ConstIterator   i = wordlists_.begin();
      ConstIterator end = wordlists_.end();
      while (i != end && i->special_id != to_find) ++i;
      return i;
    }
  };
}

#endif
