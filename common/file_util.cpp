// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "settings.h"

//#include "iostream.hpp"

#include "config.hpp"
#include "file_util.hpp"
#include "fstream.hpp"
#include "errors.hpp"
#include "string_list.hpp"

#ifdef USE_FILE_LOCKS
#  include <fcntl.h>
#  include <unistd.h>
#  include <sys/types.h>
#endif

#include <stdio.h>
#include <sys/stat.h>

#if defined(WIN32) || defined(_WIN32)
#  include <io.h>
#  include "minwin.h" //minimum windows declarations.
#  include "asc_ctype.hpp"
#  define F_OK 0 //does the file exist?
#else
#  include <unistd.h>
#   include <dirent.h>
#endif

#if defined(_MSC_VER)
#  define ACCESS _access
#else
#  define ACCESS access
#endif

namespace acommon {

  // Return false if file is already an absolute path and does not need
  // a directory prepended.
  bool need_dir(ParmString file) {
    if (file[0] == '/' || (file[0] == '.' && file[1] == '/')
#if defined(WIN32) || defined(_WIN32)
        || (asc_isalpha(file[0]) && file[1] == ':')
        || file[0] == '\\' || (file[0] == '.' && file[1] == '\\')
#endif
      )
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
    int s = file.size() - 1;
    while (s != -1 && file[s] != '/') --s;
    if (need_dir(file)) {
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
    fcntl(fd, F_SETLKW, &fl); // ignore errors
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
    fcntl(fd, F_SETLKW, &fl); // ignore errors
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
  }

  bool rename_file(ParmString orig_name, ParmString new_name)
  {
    remove(new_name);
    return rename(orig_name, new_name) == 0;
  }
 
  unsigned find_file(const Config * config, const char * option, String & filename)
  {
    StringList sl;
    config->retrieve_list(option, &sl);
    return find_file(sl, filename);
  }

  unsigned find_file(const StringList & sl, String & filename)
  {
    StringListEnumeration els = sl.elements_obj();
    const char * dir;
    String path;
    while ( (dir = els.next()) != 0 ) 
    {
      path = dir;
      if (path.back() != '/') path += '/';
      unsigned dir_len = path.size();
      path += filename;
      if (file_exists(path)) {
        filename.swap(path);
        return dir_len;
      }
    }
    return 0;
  }

#ifndef WIN32PORT
  //Unix version
  PathBrowser::PathBrowser(const StringList & sl, const char * suf)
    : dir_handle(0)
  {
    els = sl.elements();
    suffix = suf;
  }

  PathBrowser::~PathBrowser() 
  {
    delete els;
    if (dir_handle) closedir((DIR *)dir_handle);
  }

  const char * PathBrowser::next()
  {
    if (dir_handle == 0) goto get_next_dir;
  begin: {
      struct dirent * entry = readdir((DIR *)dir_handle);
      if (entry == 0) goto try_again;
      const char * name = entry->d_name;
      unsigned name_len = strlen(name);
      if (suffix.size() != 0 && 
          !(name_len > suffix.size() 
            && memcmp(name + name_len - suffix.size(), suffix.str(), suffix.size()) == 0))
        goto begin;
      path = dir;
      if (path.back() != '/') path += '/';
      path += name;
    }
    return path.str();
  try_again:
    if (dir_handle) closedir((DIR *)dir_handle);
    dir_handle = 0;
  get_next_dir:
    dir = els->next();
    if (!dir) return 0;
    dir_handle = opendir(dir);
    if (dir_handle == 0) goto try_again;
    goto begin;
  }
#else
  //Windows version
  PathBrowser::PathBrowser(const StringList & sl, const char * suf) 
  {
    els = sl.elements();
    suffix = suf;
    dir_handle = INVALID_HANDLE_VALUE;
  }

  PathBrowser::~PathBrowser() 
  {
    delete els;
    if (INVALID_HANDLE_VALUE != dir_handle)
      FindClose(dir_handle);
  }

  //
  // Get the next directory from the els list, and start reading from it.
  bool PathBrowser::GetNextDir()
  {
    dir = els->next();
    if (dir) {
      if (INVALID_HANDLE_VALUE != dir_handle)
        FindClose(dir_handle);
      String pattern = dir; pattern += "/*"; pattern += suffix;
      dir_handle = FindFirstFile(pattern.c_str(),&BrowseData);
      return true;
    }
    return false;
  }

  //
  // If we do not have a valid dir_handle we can not
  // find files, return false so that we can step to the
  // next directory and get fresh dir_handle.
  // If we have a handle, get the next file.
  // If we can not find anymore files, close the handle
  // and set dir_handle to invalid.
  // return true if we have valid data in BrowseData.
  bool PathBrowser::GetNextFile()
  {
    if (INVALID_HANDLE_VALUE == dir_handle)
      return false; //invalid dir_handle
    bool ok = FindNextFile(dir_handle,&BrowseData);
    if (!ok) {
      FindClose(dir_handle);
      dir_handle = INVALID_HANDLE_VALUE;
}
    return ok;
  }

  //
  // This just gets the next file that matches the suffix.
  const char * PathBrowser::next()
  {
    bool first = false;
    do {
      if (INVALID_HANDLE_VALUE == dir_handle) {
        if (! GetNextDir())
          return 0; //no more directories
        first = INVALID_HANDLE_VALUE != dir_handle; //FindFirstFile fills BrowseData.
      }
      while (first || GetNextFile()) {
        first = false;
        ParmString name = BrowseData.cFileName;
        if ( "."  == name || ".." == name )
          continue; //special directories
        path = dir;
        if (path.back() != '/') path += '/';
        path += name;
        return path.c_str();
      }
    } while (true);
  }
#endif
}//namespace
