#ifndef PSPELL_INFO__HPP
#define PSPELL_INFO__HPP

#include <stdio.h> //FIXME: Convert to use FStream

#include "posib_err.hpp"
#include "type_id.hpp"

namespace pcommon {

  class Config;
  struct DictInfo;
  class DictInfoEmulation;
  class DictInfoList;
  struct ModuleInfo;
  class ModuleInfoEmulation;
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
				     FStream &) = 0;
  };

  struct ModuleInfoNode;

  class ModuleInfoList : public MDInfoList {
  public:
    bool empty() const;
    unsigned int size() const;
    ModuleInfoEmulation * elements() const;
    virtual ~ModuleInfoList() {}
    PosibErr<void> proc_file(MDInfoListAll &,
			     Config *,
			     const char * dir,
			     const char * name,
			     unsigned int name_size,
			     FStream &);
    ModuleInfoNode * find(const char * to_find, 
			  unsigned int to_find_len);
  };

  ModuleInfoList * get_module_info_list(Config *);

  class DictInfoList : public MDInfoList {
  public:
    bool empty() const;
    unsigned int size() const;
    DictInfoEmulation * elements() const;
    virtual ~DictInfoList() {}
    PosibErr<void> proc_file(MDInfoListAll &,
			     Config *,
			     const char * dir,
			     const char * name,
			     unsigned int name_size,
			     FStream &);
  };

  DictInfoList * get_dict_info_list(Config *);

  class ModuleInfoEmulation {
  public:
    const ModuleInfoNode * node_;
    ModuleInfoEmulation(const ModuleInfoNode * n) : node_(n) {}

    bool at_end() const;
    const ModuleInfo * next();
    int ref_count_;
    TypeId type_id_;
    unsigned int type_id() { return type_id_.num; }
    int copyable_;
    int copyable() { return copyable_; }
    ModuleInfoEmulation * clone() const;
    void assign(const ModuleInfoEmulation * other);
    ModuleInfoEmulation() : ref_count_(0), copyable_(2) {}
    virtual ~ModuleInfoEmulation() {}
  };

  struct DictInfoNode;

  class DictInfoEmulation {
  public:
    const DictInfoNode * node_;
    DictInfoEmulation(const DictInfoNode * n) : node_(n) {}

    bool at_end() const;
    const DictInfo * next();
    int ref_count_;
    TypeId type_id_;
    unsigned int type_id() { return type_id_.num; }
    int copyable_;
    int copyable() { return copyable_; }
    DictInfoEmulation * clone() const;
    void assign(const DictInfoEmulation * other);
    DictInfoEmulation() : ref_count_(0), copyable_(2) {}
    virtual ~DictInfoEmulation() {}
  };


}

#endif /* PSPELL_INFO__HPP */
