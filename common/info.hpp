// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_INFO__HPP
#define ASPELL_INFO__HPP

#include "posib_err.hpp"
#include "type_id.hpp"

namespace acommon {

  class Config;
  struct DictInfo;
  class DictInfoEnumeration;
  class DictInfoList;
  struct ModuleInfo;
  class ModuleInfoEnumeration;
  class ModuleInfoList;
  class StringList;
  struct StringListImpl;
  class FStream;

  struct ModuleInfo {
    const char * name;
    double order_num;
    const char * lib_dir;
    StringList * pwli_dirs;
  };

  struct DictInfo {
    const char * file;
    const char * code;
    const char * jargon;
    ModuleInfo * module;
    int size;
    const char * size_str;
  };

  struct MDInfoListAll;
  struct MDInfoNode;

  struct MDInfoList 
  {
    unsigned int size_;
    MDInfoNode * head_;
    MDInfoList() : size_(0), head_(0) {}
    virtual ~MDInfoList() {}
    void clear();
    PosibErr<void> fill(MDInfoListAll &,
			Config *,
			const StringListImpl & dirs,
			const char * suffix);
    virtual PosibErr<void> proc_file(MDInfoListAll &,
				     Config *,
				     const char * dir,
				     const char * name,
				     unsigned int name_size,
				     IStream &) = 0;
  };

  struct ModuleInfoNode;

  class ModuleInfoList : public MDInfoList {
  public:
    bool empty() const;
    unsigned int size() const;
    ModuleInfoEnumeration * elements() const;
    virtual ~ModuleInfoList() {}
    PosibErr<void> proc_file(MDInfoListAll &,
			     Config *,
			     const char * dir,
			     const char * name,
			     unsigned int name_size,
			     IStream &);
    ModuleInfoNode * find(const char * to_find, 
			  unsigned int to_find_len);
  };

  ModuleInfoList * get_module_info_list(Config *);

  class DictInfoList : public MDInfoList {
  public:
    bool empty() const;
    unsigned int size() const;
    DictInfoEnumeration * elements() const;
    virtual ~DictInfoList() {}
    PosibErr<void> proc_file(MDInfoListAll &,
			     Config *,
			     const char * dir,
			     const char * name,
			     unsigned int name_size,
			     IStream &);
  };

  DictInfoList * get_dict_info_list(Config *);

  class ModuleInfoEnumeration {
  public:
    const ModuleInfoNode * node_;
    ModuleInfoEnumeration(const ModuleInfoNode * n) : node_(n) {}

    bool at_end() const;
    const ModuleInfo * next();
    int ref_count_;
    TypeId type_id_;
    unsigned int type_id() { return type_id_.num; }
    int copyable_;
    int copyable() { return copyable_; }
    ModuleInfoEnumeration * clone() const;
    void assign(const ModuleInfoEnumeration * other);
    ModuleInfoEnumeration() : ref_count_(0), copyable_(2) {}
    virtual ~ModuleInfoEnumeration() {}
  };

  struct DictInfoNode;

  class DictInfoEnumeration {
  public:
    const DictInfoNode * node_;
    DictInfoEnumeration(const DictInfoNode * n) : node_(n) {}

    bool at_end() const;
    const DictInfo * next();
    int ref_count_;
    TypeId type_id_;
    unsigned int type_id() { return type_id_.num; }
    int copyable_;
    int copyable() { return copyable_; }
    DictInfoEnumeration * clone() const;
    void assign(const DictInfoEnumeration * other);
    DictInfoEnumeration() : ref_count_(0), copyable_(2) {}
    virtual ~DictInfoEnumeration() {}
  };


}

#endif /* ASPELL_INFO__HPP */
