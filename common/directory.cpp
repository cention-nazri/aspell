// This file is part of The New Aspell
// Copyright (C) 2002 by Christoph Hintermüller under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.
// 
// Added by Christoph Hintermüller during layout of extended Filter
// interface
// 
// This file contains classes for seeking file in a given list of directories
// right now only UNIX systems are supported
// Windows system support will be added during Aspell windows port or alike
// the relevant parts are marked by "Win(Dos)

#include <stdio.h>
#include <cstdio>
#include "directory.hpp"
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#else
/*FIXME for Win(dos)*/
/* 
 * see directory.hpp file for something like an explanation
 */
#endif

namespace acommon{
  Directory::Directory( void )
  : name()
  {
    browsing=NULL;
    contentcounter=0;
  }
    
  Directory::Directory(const Directory & brother)
  : name()
  {
    browsing=NULL;
    contentcounter=0;
    name=brother.name;
  }
  
  Directory::Directory(const String & newname)
  : name(newname)
  {
    browsing=NULL;
    contentcounter=0;
  }
  
  
  void Directory::operator=(const Directory & brother){
  //+1+ as browsing is only a pointer to system internals which becomes invalid on
  //+   closing the directory either by brother it is not allowed to copy it
  //+   on the other hand on copying brother we are not interested anymore in the
  //+   old directory therefor it is closed on assignment if open
    close();
    name=brother.name;
    contentcounter=brother.contentcounter;
  //+2+ never copy refcounter of brother as this is not the amount of objects
  //+   holding a reference to us
  }
  
  void Directory::operator=(const String & newname){
  //+1+ not interested anymore in the old directory close it before doing
  //+   anything else
    close();
    name=newname;
  }
  
  
  bool Directory::open(void){
    if (browsing) {
  //+1+ Directory is already opened just call rewind to restart :)
      return rewind();
    }
  #ifndef _WIN32
    if ((browsing=opendir(name.c_str())) == NULL) {
  #else
  // replace by win(dos) pendant of opendir
    if (!true) {
  #endif
  //+2+ something went wrong during opening the directory
      return false;
    }
    return true;
  }
  
  bool Directory::open(String & newname){
    *this=newname;
    return open();
  }
  
  
  bool Directory::read(int & type,String * content){
    int processtype=type;
    String localname=name;
  #ifndef _WIN32
    struct dirent * directorycontent=NULL;
    struct stat filedescription;
  #else
  //FIXME win(dos) again
  #endif
  
    if ((content == NULL) || (browsing == NULL)) {
      return false;
    }
    if (localname[localname.length()-1] != '/') {
      localname+="/";
    }
  #ifndef _WIN32
    if ((directorycontent = readdir(browsing)) != NULL) {
  #else
  //FIXME win(dos) again 
    if (false) {
  #endif
  #ifndef SYSTEM_HAS_TELLDIR_SEEKDIR
      contentcounter++;
  #endif
      localname+=directorycontent->d_name;
      processtype=0;
  #ifndef _WIN32
      if (stat(localname.c_str(),&filedescription) < 0) {
  #else
  //FIXME win(dos) again
      if (false) {
  #endif
  // some one might install an warning here that content couldn't be stat'ed
  // right now it is simply ignored
        type=0;
        return true;
      }
  #ifndef _WIN32
      if (S_ISREG(filedescription.st_mode)) {
  #else
  //FIXME win(dos) again
      if (false) {
  #endif
        processtype|=ORDINARY;    
      }
  #ifndef _WIN32
      if (S_ISREG(filedescription.st_mode)) {
  #else
  //FIXME win(dos) again
      if (false) {
  #endif
        processtype|=DIRECTORY;    
      }
  #ifndef _WIN32
      if (S_ISREG(filedescription.st_mode)) {
  #else
  //FIXME win(dos) again
      if( false ){
  #endif
        processtype|=LINK;    
      }
  #ifndef _WIN32
      if (S_ISREG(filedescription.st_mode)) {
  #else
  //FIXME win(dos) again
      if (false) {
  #endif
        processtype|=FIFO;    
      }
  #ifndef _WIN32
      if (S_ISREG(filedescription.st_mode)) {
  #else
  //win(dos) again
      if (false) {
  #endif
        processtype|=DEVICE;    
      }
      *content=localname;
      type=processtype;
      return true;
    }
    return false;
  }
  
    
  void Directory::hold(void) {
    int localcount=contentcounter;
    if (!browsing) {
      return;
    }
  #ifndef _WIN32
  #ifdef SYSTEM_HAS_TELLDIR_SEEKDIR
    localcount=telldir(browsing);
  #endif
  #else
  //FIXME win(dos)
  #endif
    close();
    contentcounter=localcount;
  }
  
  
  bool Directory::resume(void) {
    int localcounter=contentcounter;
    if (!open()) {
      return false;
    }
  #ifndef _WIN32
  #ifdef SYSTEM_HAS_TELLDIR_SEEKDIR
    seekdir(browsing,contentcounter);
  #else
    while (localcounter-- && readdir(browsing)) {
    } 
    if (++localcounter) {
      return false;
    }
  #endif 
  #else
  //FIXME win(dos)
  #endif
    return true;
  }
  
  
  bool Directory::rewind(void) {
    if (!browsing) {
      return false;
    }
  #ifndef _WIN32
    rewinddir(browsing);
  #else
  //FIXME win(dos)
  #endif
    return true;
  }
  
  
  bool Directory::close(void) {
  #ifndef _WIN32
    if (browsing && (closedir(browsing) < 0)) {
  #else
    if (browsing && !false) {
  #endif
      browsing=NULL;
      return false;
    }
    contentcounter=0;
    browsing=NULL;
    return true;
  }
  
  
  Directory::~Directory(void) {
    close();
  }
  
  
  PathBrowser::PathBrowser(void)
  : browsebase(),
    unloop()
  {
    nextbase=0;
  }
  
  void PathBrowser::reset(void) {
  
    browsebase.resize(0);
    nextbase=0;
    unloop.resize(0);
  }
  
  void PathBrowser::operator=(const PathBrowser & brother) {
  
    reset();
    browsebase=brother.browsebase;
    nextbase=brother.nextbase;
    unloop=brother.unloop;
  }
    
  void PathBrowser::set_base_path(const String & pathlist) {
    int actualpath=0;
    String path;
    String unprocessed=pathlist;
    char resolvelink[1024];
    int resolvedlength=0;

#ifndef _WIN32
    char pathseperator=':';
#else
    char pathseperator=';';
#endif
  
    reset();
    while (!unprocessed.empty()) {
      if ((actualpath=unprocessed.find(pathseperator)) >= 0) {
        (path=unprocessed).erase(actualpath,unprocessed.length()-actualpath);
        unprocessed.erase(0,actualpath+1);
      }
      else {
        path=unprocessed;
        unprocessed.clear();
      }
#ifndef _WIN32
      if ((resolvedlength=readlink(path.c_str(),&resolvelink[0],1024)) > 0) {
        resolvelink[resolvedlength]='\0';
        path=&resolvelink[0];
      }
#else
//FIXME again and again Win(Dos)
#endif
      browsebase.resize(browsebase.size()+1);
      browsebase[browsebase.size()-1]=path;
    }
  }    

  void PathBrowser::set_base_path(const StringList & pathlist) {
    int actualpath=0;
    StringListEnumeration listofpathes=pathlist.elements_obj();
    String unprocessed;
    String path;
    const char* un_processed=NULL;
    char resolvelink[1024];
    int resolvedlength=0;

#ifndef _WIN32
    char pathseperator=':';
#else
    char pathseperator=';';
#endif

    reset();
    while ((un_processed=listofpathes.next()) != NULL) {
      unprocessed=un_processed;
      while (!unprocessed.empty()) {
        if ((actualpath=unprocessed.find(pathseperator)) >= 0) {
           (path=unprocessed).erase(actualpath,unprocessed.size()-actualpath);
          unprocessed.erase(0,actualpath+1);
        }
        else{
          path=unprocessed;
          unprocessed.clear();
        }
        if (path.empty()) {
          continue;
        }
#ifndef _WIN32
        if ((resolvedlength=readlink(path.c_str(),&resolvelink[0],1024)) > 0) {
          resolvelink[resolvedlength]='\0';
          path = resolvelink;
        }
#else
//FIXME again and again Win(Dos)
#endif
        browsebase.resize(browsebase.size()+1);
        browsebase[browsebase.size()-1]=path;
      }
    }
  }
  
      
  bool PathBrowser::expand_filename(String & filename, bool cont) {
    String content;
    Directory * actual;
    int type=0;
#ifndef _WIN32
    struct stat filedescription;
#else
#endif
  
    if (!cont) {
      nextbase=0;
    }
#ifndef _WIN32
    if (filename.length() &&
        (filename[0]=='/') &&
        (lstat(filename.c_str(),&filedescription) > 0)) {
      return true;
    }
#else
//FIXME win(dos) again
#endif
    for ( ;nextbase < browsebase.size();nextbase++) {
      actual=&browsebase[nextbase];
      if (actual->resume()) {
        while (actual->read(type,&content)) {
          if (content.suffix(filename)) {
            filename=content;
            actual->close();
            return true;
          }
        }
        actual->close();
      }
      else {
        fprintf(stderr,"Can't open\n\t`%s'\n\t\tomitted\n",
                actual->getname().c_str());
      }
    }
    nextbase=0;
    return false;
  }
  
  
  bool PathBrowser::expand_file_part(regex_t * expression, String & filename, 
                                     bool restart) {
    String content;
    Directory * actual;
    int type=0;
    char * startname=NULL;
    char * new_startname=NULL;
#ifndef _WIN32
    struct stat filedescription;
#else
#endif
  
    if (restart) {
      nextbase=0;
    }
     
    for ( ;nextbase < browsebase.size();nextbase++) {
      actual=&browsebase[nextbase];
      if (actual->resume()) {
        while (actual->read(type,&content)) {
//FIXME if windows has different library functions for
//      expanding regular expressions;
          startname=(char*)content.c_str();
          while ((new_startname=strchr(startname,'/'))) {
            startname=new_startname+1;
          }
          if (!regexec(expression,startname,0,NULL,0)) {
            filename=content; 
            return true;
          }
        }
        actual->close();
      }
      else {
        fprintf(stderr,"Can't open\n\t`%s'\n\t\tomitted\n",
                actual->getname().c_str());
      }
    }
    nextbase=0;
    return false;
  }
  
  
  PathBrowser::~PathBrowser(void) {
    reset();
  }
}
