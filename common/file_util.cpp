// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "settings.h"

#include "file_util.hpp"
#include "fstream.hpp"
#include "errors.hpp"

#ifdef USE_FILE_LOCKS
 
// POSIX headers
#include <fcntl.h>
 
#endif
 
// This needs to be <stdio.h> and not <cstdio>
#include <stdio.h>
#include <sys/stat.h>

#ifdef WIN32

#  include <io.h>
#  define ACCESS _access

#else

#  include <unistd.h>
#  define ACCESS access

#endif


namespace acommon {

  bool need_dir(ParmString file) {
    if (file[0] == '/' 
	|| (file[0] == '.' && file[1] == '/')
	// For Win32:
	|| (file[0] != '\0' && file[1] == ':')
	|| (file[0] == '\\'))
      return false;
    else
      return true;
  }

  String add_possible_dir(ParmString dir, ParmString file) {
    if (need_dir(file)) {
      String path;
      path += dir;
      path += '/';
      path += file;
      return path;
    } else {
      return file;
    }
  }

  String figure_out_dir(ParmString dir, ParmString file)
  {
    String temp;
    int s = strlen(file) - 1;
    while (s != -1 && file[s] != '/') --s;
    if (file[0] != '.' && file[0] != '/') {
      temp += dir;
      temp += '/';
    }
    if (s != -1) {
      temp.append(file, s);
    }
    return temp;
  }

  time_t get_modification_time(FStream & f) {
    struct stat s;
    fstat(f.file_no(), &s);
    return s.st_mtime;
  }

  PosibErr<void> open_file_readlock(FStream & in, ParmString file) {
    RET_ON_ERR(in.open(file, "r"));
#ifdef USE_FILE_LOCKS
    int fd = in.file_no();
    struct flock fl;
    fl.l_type   = F_RDLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;
    if (fcntl(fd, F_SETLKW, &fl) == -1) {
      in.close();
      return make_err(cant_read_file, file);
    }
#endif
    return no_err;
  }

  PosibErr<bool> open_file_writelock(FStream & inout, ParmString file) {
    typedef PosibErr<bool> Ret;
#ifndef USE_FILE_LOCKS
    bool exists = file_exists(file);
#endif
    {
     Ret pe = inout.open(file, "r+");
     if (pe.get_err() != 0)
       pe = inout.open(file, "w+");
     if (pe.has_err())
       return pe;
    }
#ifdef USE_FILE_LOCKS
    int fd = inout.file_no();
    struct flock fl;
    fl.l_type   = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;
    if (fcntl(fd, F_SETLKW, &fl) == -1)
      return Ret().prim_err(cant_write_file, file);
    struct stat s;
    fstat(fd, &s);
    return s.st_size != 0;
#else
    return exists;
#endif
  }

  void truncate_file(FStream & f, ParmString name) {
#ifdef USE_FILE_LOCKS
    f.restart();
    ftruncate(f.file_no(),0);
#else
    f.close();
    f.open(name, "w+");
#endif
  }

  bool remove_file(ParmString name) {
    return remove(name) == 0;
  }

  bool file_exists(ParmString name) {
    return ACCESS(name, F_OK) == 0;
    //struct stat fileinfo;
    //return stat(name, &fileinfo) == 0;
  }

  bool rename_file(ParmString orig_name, ParmString new_name)
  {
    remove(new_name);
    return rename(orig_name, new_name) == 0;
  }
 
  const char * get_file_name(const char * path) {
    const char * file_name;
    if (path != 0) {
      file_name = strrchr(path,'/');
      if (file_name == 0)
        file_name = path;
    } else {
      file_name = 0;
    }
    return file_name;
  }
}
