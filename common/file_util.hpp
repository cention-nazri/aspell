// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#ifndef PSPELL_FILE_UTIL__HPP
#define PSPELL_FILE_UTIL__HPP

#include <time.h>

#include "string.hpp"
#include "posib_err.hpp"

namespace pcommon {

  class FStream;

  bool need_dir(ParmString file);
  String add_possible_dir(ParmString dir, ParmString file);
  String figure_out_dir(ParmString dir, ParmString file);

  // FIXME: Possible remove
  //void open_file(FStream & in, const string & file,
  //               ParmString mode = "r");
  time_t get_modification_time(FStream & f);
  PosibErr<void> open_file_readlock(FStream& in, ParmString file);
  PosibErr<bool> open_file_writelock(FStream & in, ParmString file);
  // returns true if the file already exists
  void truncate_file(FStream & f, ParmString name);
  bool remove_file(ParmString name);
  bool file_exists(ParmString name);
  bool rename_file(ParmString orig, ParmString new_name);
  // will return NULL if path is NULL.
  const char * get_file_name(const char * path);
}
#endif
