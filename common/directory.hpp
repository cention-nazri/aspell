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
// the relevant parts are marked by "Win(Dos)" 

#ifndef SYSTERM_DIRCETORY_BRFOWSE_WRAPPER
#define SYSTERM_DIRCETORY_BRFOWSE_WRAPPER

#include "string.hpp"
#include "vector.hpp"
#include "can_have_error.hpp"
#include "string_list.hpp"
//FIXME below if Win(dos) is different
#include <regex.h>

#ifndef _WIN32
#include <sys/types.h>
#include <dirent.h>
#else
/*FIXME
 * place here all the system includes needed for windows containing similar
 * opaque data structure as DIR data type used by opendir, readdir, rewinddir,
 * closedir and if available by seekdir and telldir;
 * 
 * if this is needed at all by windows
 */
#endif


#define ORDINARY 1
#define DIRECTORY 2
#define LINK 4
#define FIFO 8
#define DEVICE 16


namespace acommon {

  class Directory{
  private:
//!Absolute pathname of directory being handled
    String name;
#ifndef _WIN32
//!pointer to systems data structure, to keep track of operation on this entire
//!directory returned by opendir and needed by all functions operating on the
//!directory itself
    DIR *browsing;
#else
/*FIXME
 *  place here Member named browsing having data type via which windows remembers which
 *  directory is browsed just now
 *  If System uses this for remembering position in Directory as Unix does does not
 *  matter as long as handing the data contained or pointed to by browsing
 *  let's Windows return the next entry contained in directory and not the first one
 *  ...
 *  If unclear see implemented Unix part for how this is meant.
 */
#endif    
//!if directory is processed it remembers at which position it was held to step
//!down into a subdirectory
    int contentcounter;
  public:
//@{
//!constructors
    Directory(void);
    Directory(const Directory & brother);
    Directory(const String & newname);
//@}

//!assignment Operators
    void operator=(const Directory & brother);
    void operator=(const String & newname);
//@}

//@{
//!Opens the directory for browsing
    bool open(void);
//!replaces the pathname of the directory to browse before opening it
    bool open(String & newname);
//@}

//!Reads the next entry from directory if opened and not at end of directory
    bool read(int & type, String * content);
//!Remembers position of directory pointer before closing it
    void hold(void);
//!restarts reading of directory at the position remembered before via
//!Directory::hold
    bool resume(void);
//!restarts reading of directory without intermediate closing.
    bool rewind(void);
//!closes the directory
    bool close(void);
//!destructor
    virtual ~Directory(void);
//!get the name of directory browsed
    String& getname(void){ return name; }
  };


  class PathBrowser {
    Vector< Directory > browsebase;
    Vector< String > unloop;
    unsigned int nextbase; 
    String actual_expression;

    void reset();
  public:
    PathBrowser(void);
    PathBrowser(const PathBrowser & brother): browsebase(), unloop() { *this=brother; }
    PathBrowser(const String & pathlist): browsebase(), unloop() { set_base_path(pathlist); }
    PathBrowser(const StringList & pathlist): browsebase(), unloop() { set_base_path(pathlist); }
    void operator=(const PathBrowser & brother);
    void operator=(const String & pathlist){ set_base_path(pathlist); }
    void operator=(const StringList & pathlist){ set_base_path(pathlist); }
    void set_base_path(const String & pathlist);
    void set_base_path(const StringList & pathlist);
    bool expand_filename(String & filename ,bool cont=false );
    bool expand_file_part(regex_t * expression,String & filename,
                          bool restart=false);
    ~PathBrowser(void);
  };
}
#endif
