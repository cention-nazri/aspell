/* This file is part of The New Aspell
   Copyright (C) 2002 Sergey Poznyakoff

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Library Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */
     
#include "settings.h"
#ifdef HAVE_LIBDL

#include "indiv_filter.hpp"
#include "config.hpp"
#include "iostream.hpp"

#include <dlfcn.h>

namespace acommon {

  extern "C" {
    typedef int (*process_fp) (char *start, char *end);
  }
  
  class LoadableFilter: public IndividualFilter
  {
    void *handle;
    process_fp proc;
  public:
    PosibErr<bool> setup (Config *);
    void reset () {}
    void process (FilterChar *&, FilterChar *&);
    ~LoadableFilter() {
      if (handle)
	dlclose (handle);
    }
  };

  PosibErr<bool> LoadableFilter::setup(Config *opts)
  {
    name_ = "loadable";
    order_num_ = 0.98;

    String modname = opts->retrieve("loadable-name");
    handle = dlopen (modname.c_str(), RTLD_NOW);
    if (!handle)
      {
	fprintf(stderr, _("Can't load filter \"%s\": %s\n"),
		modname.c_str(), dlerror ());
	return false;
      }

    proc = (process_fp) dlsym (handle, "process");
    if (!proc)
      {
	fprintf(stderr, _("Filter \"%s\" does not export symbol \"process\""),
		modname.c_str());
	dlclose (handle);
	handle = NULL;
	return false;
      }

    return true;
  }

  void LoadableFilter::process(FilterChar * & str0, FilterChar * & end)
  {
    if (!handle)
      return;
    int length = end - str0 + 1;
    char *buf = new char[length];
    char *p;
    FilterChar * fp;
    for (fp = str0, p = buf; fp < end; fp++, p++)
      *p = fp->chr;
    buf[length-1] = 0;
    proc (buf, buf + length);
    for (fp = str0, p = buf; fp < end; fp++, p++)
      *fp = *p;
    delete [] buf;
  }

  IndividualFilter *new_loadable_filter ()
  {
    return new LoadableFilter ();
  }

  static const KeyInfo loadable_options[] = {
    {"loadable-name", KeyInfoString, "",
     N_("Name of external filter to load")}
  };
  const KeyInfo *loadable_options_begin = loadable_options;
  const KeyInfo *loadable_options_end = loadable_options +
       sizeof(loadable_options)/sizeof(loadable_options[0]);
  
}

#endif
