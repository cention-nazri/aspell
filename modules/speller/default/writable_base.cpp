// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#include <time.h>

#include "data_util.hpp"
#include "errors.hpp"
#include "file_util.hpp"
#include "fstream.hpp"
#include "writable_base.hpp"

namespace aspeller {
  
  PosibErr<void> WritableBaseCode::update_file_date_info(FStream & f) {
    RET_ON_ERR(update_file_info(f));
    cur_file_date = get_modification_time(f);
    return no_err;
  }
  
  PosibErr<void> WritableBaseCode::load(ParmString f0, 
					Config * config)
  {
    set_file_name(f0);
    const String f = file_name();
    FStream in;
    
    if (file_exists(f)) {
      
      RET_ON_ERR(open_file_readlock(in, f));
      RET_ON_ERR(merge(in, f, config));
      
    } else if (f.substr(f.size()-suffix.size(),suffix.size()) 
	       == suffix) {
      
      compatibility_file_name = f.substr(0,f.size() - suffix.size());
      compatibility_file_name += compatibility_suffix;
      
      {
	PosibErr<void> pe = open_file_readlock(in, compatibility_file_name);
	if (pe.has_err()) {compatibility_file_name = ""; return pe;}
      } {
	PosibErr<void> pe = merge(in, compatibility_file_name, config);
	if (pe.has_err()) {compatibility_file_name = ""; return pe;}
      }
      
    } else {
      
      return make_err(cant_read_file,f);
      
    }

    return update_file_date_info(in);
  }

  PosibErr<void> WritableBaseCode::merge(ParmString f0) {
    FStream in;
    DataSet::FileName fn(f0);
    RET_ON_ERR(open_file_readlock(in, fn.path));
    RET_ON_ERR(merge(in, fn.path));
    return no_err;
  }

  PosibErr<void> WritableBaseCode::update(FStream & in, ParmString fn) {
    typedef PosibErr<void> Ret;
    {
      Ret pe = merge(in, fn);
      if (pe.has_err() && compatibility_file_name.empty()) return pe;
    } {
      Ret pe = update_file_date_info(in);
      if (pe.has_err() && compatibility_file_name.empty()) return pe;
    }
    return no_err;
  }
    
  PosibErr<void> WritableBaseCode::save2(FStream & out, ParmString fn) {
    truncate_file(out, fn);
      
    RET_ON_ERR(save(out,fn));

    out.flush();

    return no_err;
  }

  PosibErr<void> WritableBaseCode::save_as(ParmString fn) {
    compatibility_file_name = "";
    set_file_name(fn);
    FStream inout;
    RET_ON_ERR(open_file_writelock(inout, file_name()));
    RET_ON_ERR(save2(inout, file_name()));
    RET_ON_ERR(update_file_date_info(inout));
    return no_err;
  }

  PosibErr<void> WritableBaseCode::save(bool do_update) {
    FStream inout;
    RET_ON_ERR_SET(open_file_writelock(inout, file_name()),
		   bool, prev_existed);

    if (do_update
	&& prev_existed 
	&& get_modification_time(inout) > cur_file_date)
      RET_ON_ERR(update(inout, file_name()));

    RET_ON_ERR(save2(inout, file_name()));
    RET_ON_ERR(update_file_date_info(inout));
    
    if (compatibility_file_name.size() != 0) {
      remove_file(compatibility_file_name.c_str());
      compatibility_file_name = "";
    }

    return no_err;
  }
}
