// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

// POSIX includes
#include <dirent.h>

#include "config.hpp"
#include "errors.hpp"
#include "fstream.hpp"
#include "getdata.hpp"
#include "info.hpp"
#include "string.hpp"
#include "string_list_impl.hpp"

namespace acommon {

  /////////////////////////////////////////////////////////////////
  //
  // Info Nodes
  //

  struct MDInfoNode
  {
    MDInfoNode * next;
    MDInfoNode(MDInfoNode * n = 0) : next(n) {}
    virtual ~MDInfoNode() {}
  };
  
  struct ModuleInfoNode : public MDInfoNode
  {
    ModuleInfo c_struct;
    String name;
    String lib_dir;
    StringListImpl pwli_dirs;
  };

  struct DictInfoNode : public MDInfoNode
  {
    DictInfo c_struct;
    String file;
    String code;
    String jargon;
    String size_str;
  };

  bool operator< (const DictInfoNode &, const DictInfoNode &);

  /////////////////////////////////////////////////////////////////
  //
  // Lists of Info Lists
  //

  static void get_data_dirs (Config *,
			     StringListImpl &);

  struct MDInfoListAll
  // this is in an invalid state if some of the lists
  // has data but others don't
  {
    StringListImpl for_dirs;
    ModuleInfoList module_info_list;
    StringListImpl pwli_dir_list;
    DictInfoList   dict_info_list;
    void clear();
    PosibErr<void> fill(Config *, StringListImpl &);
    bool has_data() {return module_info_list.head_ != 0; }
    PosibErr<void> fill_pwli_dir_list();
  };

  struct MDInfoListofLists
  {
    MDInfoListAll * data;
  
    int       offset;
    int       size;
  
    int valid_pos(int pos) {return offset <= pos && pos < size + offset;}

    MDInfoListofLists();
    ~MDInfoListofLists();

    void clear(Config * c);
    int find(const StringListImpl &);

    PosibErr<MDInfoListAll *> get_lists(Config * c);

    void flush() {} // unimplemented
  };

  static MDInfoListofLists md_info_list_of_lists;

  /////////////////////////////////////////////////////////////////
  //
  // Info Default List
  //

  struct MDInfoDefItem {
    const char * dir;
    const char * name;
    const char * data;
  };


  /////////////////////////////////////////////////////////////////
  //
  // Utility functions declaration
  //

  static const char * strnchr(const char * i, char c, unsigned int size);
  static const char * strnrchr(const char * stop, char c, unsigned int size);

  /////////////////////////////////////////////////////////////////
  //
  // Node impl
  //

  bool operator< (const DictInfoNode & r, const DictInfoNode & l)
  {
    const DictInfo & rhs = r.c_struct;
    const DictInfo & lhs = l.c_struct;
    int res = strcmp(rhs.code, lhs.code);
    if (res < 0) return true;
    if (res > 0) return false;
    res = strcmp(rhs.jargon,lhs.jargon);
    if (res < 0) return true;
    if (res > 0) return false;
    if (rhs.size < lhs.size) return true;
    if (rhs.size > lhs.size) return false;
    res = strcmp(rhs.module->name,lhs.module->name);
    if (res < 0) return true;
    return false;
  }


  /////////////////////////////////////////////////////////////////
  //
  // Info Lists Impl.
  //

  void MDInfoList::clear() 
  {
    while (head_ != 0) {
      MDInfoNode * to_del = head_;
      head_ = head_->next;
      delete to_del;
    }
  }

  PosibErr<void> MDInfoList::fill(MDInfoListAll & list_all,
				  Config * config,
				  const StringListImpl & dirs,
				  const char * suffix)
  {
    MDInfoDefList def_list = default_list();
    for (const MDInfoDefItem * i = def_list.begin; i != def_list.end; ++i)
    {
      StringIStream in(i->data);
      proc_file(list_all, config, i->dir, i->name, strlen(i->name), in);
    }

    StringListEnumeration els = dirs.elements_obj();
    const char * dir;
    while ( (dir = els.next()) != 0) {
      DIR * d = opendir(dir);
      if (d==0) continue;
    
      struct dirent * entry;
      while ( (entry = readdir(d)) != 0) {
	const char * name = entry->d_name;
	const char * dot_loc = strrchr(name, '.');
	unsigned int name_size = dot_loc == 0 ? strlen(name) :  dot_loc - name;
      
	// check if it ends in suffix
	if (strcmp(name + name_size, suffix) != 0)
	  continue;
      
	String path;
	path += dir;
	path += '/';
	path += name;
	FStream in;
	RET_ON_ERR(in.open(path, "r"));
	RET_ON_ERR(proc_file(list_all, config, dir, name, name_size, in));
      }
    }
    return no_err;
  }

  MDInfoDefList MDInfoList::default_list() const 
  {
    return MDInfoDefList();
  }

  /////////////////////////////////////////////////////////////////
  //
  // ModuleInfoList Impl
  //
  
  PosibErr<void> ModuleInfoList::proc_file(MDInfoListAll &,
					   Config * config,
					   const char * dir,
					   const char * name,
					   unsigned int name_size,
					   IStream & in)
  {
    MDInfoNode * * prev = &head_;
    ModuleInfoNode * to_add = new ModuleInfoNode();
    to_add->c_struct.name = 0;
    to_add->c_struct.order_num = -1;
    to_add->c_struct.lib_dir = 0;
    to_add->c_struct.pwli_dirs = 0;

    to_add->name.assign(name, name_size);
    to_add->c_struct.name = to_add->name.c_str();
    
    PosibErr<void> err;

    String key, data;
    while (getdata_pair(in, key, data)) {
      if (key == "order-num") {
	char * tailptr;
	to_add->c_struct.order_num = strtod(data.c_str(), &tailptr);
	if (*tailptr != '\0' ||
	    !(0 < to_add->c_struct.order_num && 
	      to_add->c_struct.order_num < 1)) 
	  {
	    err.prim_err(bad_value, key, data,
			 "a number between 0 and 1");
	    goto ERROR;
	  }
      } else if (key == "lib-dir") {
	to_add->lib_dir = data;
	to_add->c_struct.lib_dir = to_add->lib_dir.c_str();
      } else if (key == "pwli-dir") {
	to_add->c_struct.pwli_dirs = &(to_add->pwli_dirs);
	to_add->pwli_dirs.add(data);
      } else {
	err.prim_err(unknown_key, key);
	goto ERROR;
      }
    }
  
    while (*prev != 0 && 
	   ((ModuleInfoNode *)*prev)->c_struct.order_num > to_add->c_struct.order_num)
      prev = &(*prev)->next;
    to_add->next = *prev;
    *prev = to_add;
    return err;

  ERROR:
    delete to_add;
    return err;
  }

  ModuleInfoNode * ModuleInfoList::find(const char * to_find, 
					unsigned int to_find_len)
  {
    for (ModuleInfoNode * n = (ModuleInfoNode *)head_; 
	 n != 0; 
	 n = (ModuleInfoNode *)n->next)
      if (n->name.size() == to_find_len 
	  && strncmp(n->name.c_str(), to_find, to_find_len) == 0) return n;
    return 0;
  }

  static const MDInfoDefItem module_info_list_def_list[] = {
    {"", "default", "order-num 0.50"}
  };
  
  MDInfoDefList ModuleInfoList::default_list() const {
    return MDInfoDefList(module_info_list_def_list,
			 module_info_list_def_list 
			 + sizeof(module_info_list_def_list)/sizeof(MDInfoDefItem));
  }

  /////////////////////////////////////////////////////////////////
  //
  // DictInfoList Impl
  //

  PosibErr<void> DictInfoList::proc_file(MDInfoListAll & list_all,
					 Config * config,
					 const char * dir,
					 const char * name,
					 unsigned int name_size,
					 IStream & in)
  {
    MDInfoNode * * prev = &head_;
    DictInfoNode * to_add = new DictInfoNode();
    const char * p0;
    const char * p1;
    const char * p2;
    p0 = strnchr(name, '-', name_size - 5);
    p2 = strnrchr(name, '-', name_size - 5);
    p1 = p2;
    assert (p0 != 0);
    if (p0 + 2 < p1 && isdigit(p1[-1]) && isdigit(p1[-2]) && p1[-3] == '-')
      p1 -= 2;
  
    to_add->code.assign(name, p0-name);
    to_add->c_struct.code = to_add->code.c_str();

    ModuleInfoNode * mod 
      = list_all.module_info_list.find(p2+1, name_size - (p2+1-name));
    //FIXME: Check for null and return and possibly return an error
    //       on an unknown module
    to_add->c_struct.module = &(mod->c_struct);
  
    if (p0 + 1 < p1)
      to_add->jargon.assign(p0+1, p1 - p0 - 1);
    to_add->c_struct.jargon = to_add->jargon.c_str();
  
    if (p1 != p2) 
      to_add->size_str.assign(p1, 2);
    else
      to_add->size_str = "60";
    to_add->c_struct.size_str = to_add->size_str.c_str();
    to_add->c_struct.size = atoi(to_add->c_struct.size_str);

    to_add->file  = dir;
    to_add->file += '/';
    to_add->file += name;
    to_add->c_struct.file = to_add->file.c_str();

    while (*prev != 0 && *(DictInfoNode *)*prev < *to_add)
      prev = &(*prev)->next;
    to_add->next = *prev;
    *prev = to_add;

    return no_err;
  }


  /////////////////////////////////////////////////////////////////
  //
  // Lists of Info Lists Impl
  //

  void get_data_dirs (Config * config,
		      StringListImpl & lst)
  {
    String dir = config->retrieve("data-dir");
    lst.clear();
    lst.add(dir);
  }

  void MDInfoListAll::clear()
  {
    module_info_list.clear();
    pwli_dir_list.clear();
    dict_info_list.clear();
  }

  PosibErr<void> MDInfoListAll::fill(Config * c,
				     StringListImpl & dirs)
  {
    for_dirs = dirs;
    PosibErr<void> err;
    err = module_info_list.fill(*this, c, for_dirs, ".asmi");
    if (err.has_err()) goto ERROR;
    err = fill_pwli_dir_list();
    if (err.has_err()) goto ERROR;
    pwli_dir_list = dirs;
    err = dict_info_list.fill(*this, c, pwli_dir_list, ".pwli");
    if (err.has_err()) goto ERROR;
    return err;
  ERROR:
    clear();
    return err;
  }

  PosibErr<void> MDInfoListAll::fill_pwli_dir_list()
  {
    for (MDInfoNode * n = module_info_list.head_; n != 0; n = n->next) {
      StringListEnumeration e = 
	((ModuleInfoNode *)n)->pwli_dirs.elements_obj();
      const char * dir;
      while ( (dir = e.next()) != 0 )
	pwli_dir_list.add(dir);
    }
    return no_err;
  }

  MDInfoListofLists::MDInfoListofLists()
    : data(0), offset(0), size(0)
  {
  }

  MDInfoListofLists::~MDInfoListofLists() {
    for (int i = offset; i != offset + size; ++i)
      data[i].clear();
    delete[] data;
  }

  void MDInfoListofLists::clear(Config * c)
  {
    StringListImpl dirs;
    get_data_dirs(c, dirs);
    int pos = find(dirs);
    if (pos == -1) {
      data[pos - offset].clear();
    }
  }

  int MDInfoListofLists::find(const StringListImpl & dirs)
  {
    for (int i = 0; i != size; ++i) {
      if (data[i].for_dirs == dirs)
	return i + offset;
    }
    return -1;
  }

  PosibErr<MDInfoListAll *>
  MDInfoListofLists::get_lists(Config * c)
  {
    Config * config = (Config *)c;
    int & pos = config->md_info_list_index;
    StringListImpl dirs;
    if (!valid_pos(pos)) {
      get_data_dirs(config, dirs);
      pos = find(dirs);
    }
    if (!valid_pos(pos)) {
      MDInfoListAll * new_data = new MDInfoListAll[size + 1];
      for (int i = 0; i != size; ++i) {
	new_data[i] = data[i];
      }
      ++size;
      delete[] data;
      data = new_data;
      pos = size - 1 + offset;
    }
    MDInfoListAll & list_all = data[pos - offset];
    if (list_all.has_data())
      return &list_all;

    RET_ON_ERR(list_all.fill(config, dirs));

    return &list_all;
  }

  /////////////////////////////////////////////////////////////////
  //
  // utility functions
  //

  static const char * strnchr(const char * i, char c, unsigned int size)
  {
    const char * stop = i + size;
    while (i != stop) {
      if (*i == c)
	return i;
      ++i;
    }
    return 0;
  }

  static const char * strnrchr(const char * stop, char c, unsigned int size)
  {
    const char * i = stop + size - 1;
    --stop;
    while (i != stop) {
      if (*i == c)
	return i;
      --i;
    }
    return 0;
  }

  /////////////////////////////////////////////////////////////////
  //
  // user visable functions and enumeration impl
  //

  //
  // ModuleInfo
  //

  ModuleInfoList * get_module_info_list(Config * c)
  {
    MDInfoListAll * la = md_info_list_of_lists.get_lists(c);
    if (la == 0) return 0;
    else return &la->module_info_list;
  }

  ModuleInfoEnumeration * ModuleInfoList::elements() const
  {
    return new ModuleInfoEnumeration((ModuleInfoNode *)head_);
  }

  unsigned int ModuleInfoList::size() const
  {
    return size_;
  }

  bool ModuleInfoList::empty() const
  {
    return size_ != 0;
  }

  ModuleInfoEnumeration * ModuleInfoEnumeration::clone () const
  {
    return new ModuleInfoEnumeration(*this);
  }

  void ModuleInfoEnumeration::assign(const ModuleInfoEnumeration * other)
  {
    *this = *other;
  }
  
  bool ModuleInfoEnumeration::at_end () const
  {
    return node_ == 0;
  }

  const ModuleInfo * ModuleInfoEnumeration::next ()
  {
    if (node_ == 0) return 0;
    const ModuleInfo * data = &(node_->c_struct);
    node_ = (ModuleInfoNode *)(node_->next);
    return data;
  }

  //
  // DictInfo
  //

  DictInfoList * get_dict_info_list(Config * c)
  {
    MDInfoListAll * la = md_info_list_of_lists.get_lists(c);
    if (la == 0) return 0;
    else return &la->dict_info_list;
  }

  DictInfoEnumeration * DictInfoList::elements() const
  {
    return new DictInfoEnumeration(static_cast<DictInfoNode *>(head_));
  }

  unsigned int DictInfoList::size() const
  {
    return size_;
  }

  bool DictInfoList::empty() const
  {
    return size_ != 0;
  }

  DictInfoEnumeration * DictInfoEnumeration::clone() const
  {
    return new DictInfoEnumeration(*this);
  }

  void DictInfoEnumeration::assign(const DictInfoEnumeration * other)
  {
    *this = *other;
  }

  bool DictInfoEnumeration::at_end() const
  {
    return node_ == 0;
  }

  const DictInfo * DictInfoEnumeration::next ()
  {
    if (node_ == 0) return 0;
    const DictInfo * data = &(node_->c_struct);
    node_ = (DictInfoNode *)(node_->next);
    return data;
  }

}
