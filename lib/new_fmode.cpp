// This file is part of The New Aspell
// Copyright (C) 2002 by Christoph Hintermüller (JEH) under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.
#include "settings.h"

#include <sys/types.h>
#include <regex.h>

#include <string.hpp>
#include <vector.hpp>
#include "config.hpp"
#include "errors.hpp"
#include "filter.hpp"
#include "string_enumeration.hpp"
#include "string_list.hpp"
#include "posib_err.hpp"
#include "file_util.hpp"
#include "fstream.hpp"
#include "getdata.hpp"
#include "directory.hpp"
#include "strtonum.hpp"
#include "asc_ctype.hpp"
#include "iostream.hpp"
#include <stdio.h>

#define DEBUG {fprintf(stderr,"File: %s(%i)\n",__FILE__,__LINE__);}


namespace acommon {

using namespace std;//needed for vector

  class FilterMode {
  public:
    class MagicString {
    public:
      MagicString(const String & mode):_magic(),_mode(mode),fileExtensions() {}
      MagicString(const String & magic,const String & mode)
        :_magic(magic),_mode(mode),fileExtensions() {} 
      MagicString(const MagicString & m)
        :_magic(m._magic),_mode(m._mode),fileExtensions(m.fileExtensions) {}
      MagicString & operator = (const MagicString & m) {
        _magic = m._magic;
        _mode  = m._mode;
        return *this;
      }
      bool matchFile(FILE * in,const String & ext);
      static PosibErr<bool> testMagic(FILE * seekIn,String & magic,const String mode);
      void addExtension(const String & ext) { fileExtensions.push_back(ext); }
      bool hasExtension(const String & ext);
      void remExtension(const String & ext);
      MagicString & operator += (const String ext) {addExtension(ext);return *this;}
      MagicString & operator -= (const String ext) {remExtension(ext);return *this;}
      MagicString & operator = (const String ext) { 
        fileExtensions.clear();
        addExtension(ext);
        return *this; 
      }
      const String & magic() const { return _magic; }
      const String & magicMode() const { return _mode; }
      ~MagicString() {}
    private:
      String _magic;
      String _mode;
      vector<String> fileExtensions;
    };

    FilterMode(const FilterMode & b)
      :_name(b._name),_desc(b._desc),magicKeys(b.magicKeys),expansion(b.expansion) {}
    FilterMode(const String & name);
    PosibErr<bool> addModeExtension(const String & ext,String toMagic);
    PosibErr<bool> remModeExtension(const String & ext,String toMagic);
    bool lockFileToMode(const String & fileName,FILE * in = NULL);
    const String modeName() const;
    void setDescription(const String & desc) {_desc = desc;}
    const String & getDescription() {return _desc;}
    PosibErr<void> expand(Config * config);
    PosibErr<void> build(FILE * in,Config * config,int line = 1,const char * name = "mode file");

    ~FilterMode();
  private:
     //map extensions to magic keys 
     String _name;
     String _desc;
     vector<MagicString> magicKeys;
     vector< vector< String > > expansion;
  };

  FilterMode::FilterMode(const String & name)
  : _name(name),
    _desc(),
    magicKeys(),
    expansion()
  {
  }

  PosibErr<bool> FilterMode::addModeExtension(const String & ext, String toMagic) {

    bool extOnly = false;
    
    if (    ( toMagic == "" )
         || ( toMagic == "<nomagic>" )
         || ( toMagic == "<empty>" ) ) {
      extOnly = true;
    }
    else {

      PosibErr<bool> pe = FilterMode::MagicString::testMagic(NULL,toMagic,_name);

      if ( pe.has_err() ) {
        return PosibErrBase(pe);
      }
    } 

    vector<MagicString>::iterator it;

    for ( it = magicKeys.begin() ; it != magicKeys.end() ; it++ ) {
      if (    (    extOnly
                && ( it->magic() == "" ) )
           || ( it->magic() == toMagic ) ) {
        (*it) += ext;
        return true;
      }
    }
    if ( it != magicKeys.end() ) {
      return false;
    }
    if ( extOnly ) {
      magicKeys.push_back(MagicString(_name));
    }
    else {
      magicKeys.push_back(MagicString(toMagic,_name));
    }
    for ( it = magicKeys.begin() ; it != magicKeys.end() ; it++ ) {
      if (    (    extOnly
                && ( it->magic() == "" ) )
           || ( it->magic() == toMagic ) ) {
        (*it) += ext;
        return true;
      }
    }
    return make_err(mode_extend_expand,_name.c_str());
  }

  PosibErr<bool> FilterMode::remModeExtension(const String & ext, String toMagic) {

    bool extOnly = false;

    if (    ( toMagic == "" )
         || ( toMagic == "<nomagic>" )
         || ( toMagic == "<empty>" ) ) {
      extOnly = true;
    }
    else {

      PosibErr<bool> pe = FilterMode::MagicString::testMagic(NULL,toMagic,_name);

      if ( pe.has_err() ) {
        return PosibErrBase(pe);
      }
    }

    for ( vector<MagicString>::iterator it = magicKeys.begin() ;
          it != magicKeys.end() ; it++ ) {
      if (    (    extOnly
                && ( it->magic() == "" ) )
           || ( it->magic() == toMagic ) ) {
        (*it) -= ext;
        return true;
      }
    }
    return false;
  }

  bool FilterMode::lockFileToMode(const String & fileName,FILE * in) {

    vector<unsigned int> extStart;
    int first_point = fileName.length();

    while ( first_point > 0 ) {
      while (    ( --first_point >= 0 )
              && ( fileName[first_point] != '.' ) ) {
      }
      if (    ( first_point >= 0 )
           && ( fileName[first_point] == '.' ) ) {
        extStart.push_back(first_point + 1);
      }
    }
    if ( extStart.size() < 1 )  {
      return false;
    }

    bool closeFile = false;

    if ( in == NULL ) {
      in = fopen(fileName.c_str(),"rb");
      closeFile= true;
    }
    for ( vector<unsigned int>::iterator extSIt = extStart.begin() ;
          extSIt != extStart.end() ; extSIt ++ ) {
    
      String ext(fileName);

      ext.erase(0,*extSIt);
      for ( vector<MagicString>::iterator it = magicKeys.begin() ;
            it != magicKeys.end() ; it++ ) {
        PosibErr<bool> magicMatch = it->matchFile(in,ext);
        if (    magicMatch 
             || magicMatch.has_err() ) {
          if ( closeFile ) {
            fclose ( in );
          }
          if ( magicMatch.has_err() ) {
            magicMatch.ignore_err();
            return false;
          }
          return true;
        }
      }
    }
    if ( closeFile ) {
      fclose(in);
    }
    return false;
  }

  const String FilterMode::modeName() const {
    return _name;
  }

  FilterMode::~FilterMode() {
  }

  bool FilterMode::MagicString::hasExtension(const String & ext) {
    for ( vector<String>::iterator it = fileExtensions.begin() ;
          it != fileExtensions.end() ; it++ ) {
      if ( *it == ext ) {
        return true;
      }
    }
    return false;
  }

  void FilterMode::MagicString::remExtension(const String & ext) {
    for ( vector<String>::iterator it = fileExtensions.begin() ;
          it != fileExtensions.end() ; it++ ) {
      if ( *it == ext ) {
        fileExtensions.erase(it);
      }
    }
  }


  bool FilterMode::MagicString::matchFile(FILE * in,const String & ext) {

    vector<String>::iterator extIt;

    for ( extIt = fileExtensions.begin() ; 
          extIt != fileExtensions.end() ; extIt ++ ) {
      if ( *extIt == ext ) {
        break;
      }
    }
    if ( extIt == fileExtensions.end() ) {
      return false;
    }

    PosibErr<bool> pe = testMagic(in,_magic,_mode);

    if ( pe.has_err() ) {
      pe.ignore_err();
      return false;
    }
    return true;
  }


  PosibErr<bool> FilterMode::MagicString::testMagic(FILE * seekIn,String & magic,const String mode) {

    if ( magic.length() == 0 ) {
      return true;
    }
 
    unsigned int magicFilePosition = 0;

    while (    ( magicFilePosition < magic.length() )
            && ( magic[magicFilePosition] != ':' ) ) {
      magicFilePosition++;
    }

    String number(magic);

    number.erase(magicFilePosition,magic.length() - magicFilePosition);

    char * num = (char *)number.c_str();
    char * numEnd = num + number.length();
    char * endHere = numEnd;
    long position = 0;

    if (    ( number.length() == 0 ) 
         || ( (position = strtoi_c(num,&numEnd)) < 0 )
         || ( numEnd != endHere ) ) {
      return make_err(file_magic_pos,"",magic.c_str());
    }
    if (    ( magicFilePosition >= magic.length() )
         || (    ( seekIn != NULL )
              && ( fseek(seekIn,position,SEEK_SET) < 0 ) ) ) {
      if ( seekIn != NULL ) {
        rewind(seekIn);
      }
      return false;
    }

    //increment magicFilePosition to skip the `:'
    unsigned int seekRangePos = ++ magicFilePosition; 

    while (    ( magicFilePosition < magic.length() )
            && ( magic[magicFilePosition] != ':' ) ) {
      magicFilePosition++;
    }

    String magicRegExp(magic);

    magicRegExp.erase(0,magicFilePosition + 1);
    if ( magicRegExp.length() == 0 ) {
      if ( seekIn != NULL ) {
        rewind(seekIn);
      }
      return make_err(missing_magic,mode.c_str(),magic.c_str()); //no regular expression given
    }
    
    number = magic;
    number.erase(magicFilePosition,magic.length() - magicFilePosition);
    number.erase(0,seekRangePos);//already incremented by one see above
    num = (char*)number.c_str();
    endHere = numEnd = num + number.length();

    if (    ( number.length() == 0 )
         || ( (position = strtoi_c(num,&numEnd)) < 0 )
         || ( numEnd != endHere ) ) {
      if ( seekIn != NULL ) {
        rewind(seekIn);
      }
      return make_err(file_magic_range,mode.c_str(),magic.c_str());//no magic range given
    }

    regex_t seekMagic;
    int regsucess = 0;

    if ( (regsucess = regcomp(&seekMagic,magicRegExp.c_str(),
                              REG_NEWLINE|REG_NOSUB|REG_EXTENDED)) ){
      if ( seekIn != NULL ) {
        rewind(seekIn);
      }

      char regError[256];
      CERR.printl(magicRegExp.c_str());
      regerror(regsucess,&seekMagic,&regError[0],256);
      return make_err(bad_magic,mode.c_str(),magic.c_str(),regError);
    }
    if ( seekIn == NULL ) {
      regfree(&seekMagic);
      return true;
    }

    char * buffer = new char[(position + 1)];

    if ( buffer == NULL ) {
      regfree(&seekMagic);
      rewind(seekIn);
      return false;
    }
    memset(buffer,0,(position + 1));
    if ( (position = fread(buffer,1,position,seekIn)) == 0 ) {
      rewind(seekIn);
      regfree(&seekMagic);
      delete[] buffer;
      return false;
    }
    if ( regexec(&seekMagic,buffer,0,NULL,0) ) {
      delete[] buffer;
      regfree(&seekMagic);
      rewind(seekIn);
      return false;
    }
    delete[] buffer;
    regfree(&seekMagic);
    rewind(seekIn);
    return true;
  }

  PosibErr<void> FilterMode::expand(Config * config) {

    config->replace("rem-all-filter","");
    for ( vector< vector< String > >::iterator it = expansion.begin() ;
          it != expansion.end() ; it++ ) {

      String key((*it)[0]);
      String value((*it)[1]);
      String occursInAt((*it)[2]);
      String lineNumber(occursInAt);
      unsigned int split = occursInAt.rfind(':');

      occursInAt.erase(split,occursInAt.length() - split);
      lineNumber.erase(0,split);

      if ( value == "" ) {

        bool haveremall = false;
        String rmKey(key);

        if (rmKey.prefix("rem-all-")) {
          rmKey.erase(0,8);
          haveremall = true;
        }
        else if (rmKey.prefix("rem-") || rmKey.prefix("add-")) {
          rmKey.erase(0,4);
        }
        else if (rmKey.prefix("dont-")) {
          rmKey.erase(0,5);
        }

        PosibErr<const KeyInfo *> kte = config->keyinfo(rmKey.c_str());

        if ( kte.has_err() ) {
          return make_err(error_on_line,occursInAt.c_str(),lineNumber.c_str(),
                          kte.get_err()->mesg);
        }
        if (    kte.data->type != KeyInfoBool
             && (    kte.data->type != KeyInfoList
                  || !haveremall ) ) {
          return make_err(empty_non_bool,occursInAt.c_str(),lineNumber.c_str());
        }
      }

      PosibErr<void> repErr = config->replace(key.c_str(),value.c_str());

      if ( repErr.has_err() ) {
        return make_err(error_on_line,occursInAt.c_str(),lineNumber.c_str(),
                        repErr.get_err()->mesg);
      }      
    }
    return no_err;  
  }

  PosibErr<void> FilterMode::build(FILE * in,Config * config, int line0, const char * name) {

    String buf;
    DataPair dp;
    dp.line_num = line0;
    vector<String> filters;
    FStream toParse(in,false);

    while ( getdata_pair(toParse, dp, buf) ) {
      to_lower(dp.key);
      if ( dp.key == "filter" ) {
        to_lower(dp.value);
        filters.push_back(dp.value.str);
            
        char lineNumber[12];

        sprintf(&lineNumber[0],"%i",dp.line_num);

        String line_and_file(name);

        line_and_file += ":";
        line_and_file += lineNumber;

        vector<String> expander;

        expander.push_back("add-filter");
        expander.push_back(dp.value);
        expander.push_back(line_and_file);
        expansion.push_back(expander);
        continue;
      }
      if ( dp.key == "!filter" ) {
        to_lower(dp.value);
        for ( vector<String>::iterator it = filters.begin() ;
              it != filters.end() ; it ++ ) {
          if ( *it == dp.value.str ) {
            filters.erase(it);
            
            char lineNumber[12];

            sprintf(&lineNumber[0],"%i",dp.line_num);

            String line_and_file(name);

            line_and_file += ":";
            line_and_file += lineNumber;


            vector<String> expander;

            expander.push_back("rem-filter");
            expander.push_back(dp.value);
            expander.push_back(line_and_file);
            expansion.push_back(expander);
            break;
          }
        }
        continue;
      }
      if ( dp.key == "option" ) {

        char * optionBegin = dp.value;
        char * option = optionBegin;
        char * optionEnd = dp.value + dp.value.size;

        while (    ( option != optionEnd )
                && ( *option != '\0' )
                && !asc_isspace(*option) ) {
          option++;
        }
        if ( option == optionBegin ) {

          char lineNumber[12];

          sprintf(&lineNumber[0],"%i",dp.line_num);
          return make_err(mode_option_name,name,lineNumber);
        }

        char * optVal = option;

        if ( *option != '\0' ) {
          *option = '\0';
          optVal ++;
        }

        char * optValBegin = optVal;
        char * optValEnd = optionEnd;

        optionEnd = option;
            
        String optFilter(optionBegin);
        String optSubstValue;

        if ( optFilter.prefix("rem-") ) { 
          optFilter.erase(0,4);
          optSubstValue = "rem";
        }
        else if ( optFilter.prefix("add-") ) {
          optFilter.erase(0,4);
          optSubstValue = "add";
        }
        else if ( optFilter.prefix("dont-") ) {
          optFilter.erase(0,5);
          optSubstValue = "dont";
        }
        if ( optFilter.prefix("all-") ) {
          optFilter.erase(0,4);
          optSubstValue += "-all";
        }
        if ( optFilter.prefix("filter-") ) {
          optFilter.erase(0,7);
        }
        
        for ( vector<String>::iterator filtNIt = filters.begin() ;
              filtNIt != filters.end() ; filtNIt++ ) {
          if ( optFilter.prefix(*filtNIt) ) {
            while (    ( optVal != optValEnd )
                    && ( *optVal != '\0' )
                    && asc_isspace(*optVal) ) {
              optVal++;
            }
/*            if (    ( optVal == optValEnd )
                 || ( *optVal == '\0' ) ) {

              StringList dum;
              PosibErr<void> pl = config->retrieve_list(optionBegin,&dum);
              PosibErr<bool> pb = config->retrieve_bool(optionBegin);
              if (    (    pl.has_err() 
                        && Fixme remove this comment.has_err() )
                   || (    !pl.has_err() 
                        && ( FIXME remove this comment != "rem-all" ) ) ) {

                pl.ignore_err();
                Fixme remove this comment.ignore_err();
                char lineNumber[12];

                sprintf(&lineNumber[0],"%i",line);
                return make_err(empty_non_bool,name,lineNumber);
              }
              pl.ignore_err();
              Fixme remove this comment.ignore_err();
            }*/
            
            char lineNumber[12];

            sprintf(&lineNumber[0],"%i",dp.line_num);

            String line_and_file(name);

            line_and_file += ":";
            line_and_file += lineNumber;

            String optionValue(optVal);
            unescape(optionValue);

            vector<String> expander;
            expander.push_back(optionBegin);
            expander.push_back(optionValue);
            expander.push_back(line_and_file);
            expansion.push_back(expander);
            goto fine_next_line;//hm ok breaking continue;
          }
        }

        char lineNumber[12];

        sprintf(&lineNumber[0],"%i",dp.line_num);
        return make_err(no_filter_to_option,name,lineNumber,optionBegin);
fine_next_line:
        continue;
      }

      char lineNumber[12];

      sprintf(&lineNumber[0],"%i",dp.line_num);
      return make_err(bad_mode_key,name,lineNumber,dp.key);
    }
    return no_err;
  }
    
  static vector<FilterMode> filterModes;
  int filterModesRef = 0;

  

  void set_mode_from_extension (Config * config, ParmString filename, FILE * in) {
    for ( vector<FilterMode>::iterator it = filterModes.begin() ;
         it != filterModes.end() ; it++ ) {
      if ( (*it).lockFileToMode(filename,in) ) {
        config->replace("mode", (*it).modeName().c_str());
        break;
      }
    }
  }


  void activate_filter_modes(Config *config);


  class ModeNotifierImpl : public Notifier
  {
  private:
    Config * config;
    StringList mode_path;

    ModeNotifierImpl();
    ModeNotifierImpl(const ModeNotifierImpl &);
    ModeNotifierImpl & operator= (const ModeNotifierImpl & b);
    PosibErr<void> intLoadModes(const bool reset = false);
  public:
    ModeNotifierImpl(Config * c);
    
    ModeNotifierImpl * clone(Config * c) const {return new ModeNotifierImpl(c);}

    PosibErr<void> item_updated(const KeyInfo * ki, ParmString value);
    PosibErr<void> item_added(const KeyInfo * ki, ParmString value);

    PosibErr<void> initModes();

    ~ModeNotifierImpl();
  };

  PosibErr<void> ModeNotifierImpl::initModes() {

    if (    ( filterModesRef > 0 )
         && ( filterModes.size() > 0 ) ) {
      return no_err;
    }
    filterModes.clear();
    return intLoadModes();
  }

  ModeNotifierImpl::ModeNotifierImpl() 
  : config(NULL),
    mode_path()
  {
    filterModesRef++;
  }

  ModeNotifierImpl::ModeNotifierImpl(Config * c) 
  : config(c),
    mode_path()
  {
    filterModesRef++;
  }

  ModeNotifierImpl::ModeNotifierImpl(const ModeNotifierImpl & b) 
  : config(b.config),
    mode_path(b.mode_path)
  {
    filterModesRef++;
  }

  ModeNotifierImpl::~ModeNotifierImpl() {
    if ( --filterModesRef < 1 ) {
      filterModes.clear();
    }
  }
    
  ModeNotifierImpl & ModeNotifierImpl::operator= (const ModeNotifierImpl & b) {
    config = b.config;
    mode_path = b.mode_path;
    return *this;
  }

  PosibErr<void> ModeNotifierImpl::item_updated(const KeyInfo * ki, ParmString value) {
    if ( strcmp(ki->name, "-t" ) == 0 ) {
      return (config->replace("mode","tex"));
    }
    if ( strcmp(ki->name, "-H" ) == 0 ) {
      return (config->replace("mode","html"));
    }
    if ( strcmp(ki->name, "-e" ) == 0 ) {

      return (config->replace("mode","email"));
    }
    if ( strcmp(ki->name, "mode") == 0 ) {
      for ( vector<FilterMode>::iterator it = filterModes.begin() ;
            it != filterModes.end() ; it++ ) {
        if ( (*it).modeName() == value ) {
          config->replace("rem-all-filter","");
          return (*it).expand(config);
        }
      }
      return make_err(unknown_mode,value); 
    }
    return no_err;
  }

  PosibErr<void> ModeNotifierImpl::item_added(const KeyInfo * ki, ParmString value) {
  
    if ( strcmp(ki->name, "filter-path") == 0) {
      return intLoadModes();
    }
    return no_err;
  }

  PosibErr<void> ModeNotifierImpl::intLoadModes(const bool reset) {


//FIXME is filter-path proper for filter mode files ???
//      if filter-options-path better ???
//      do we need a filter-mode-path ???
    RET_ON_ERR(config->retrieve_list("filter-path",&mode_path));
    if ( mode_path.elements()->at_end() ) {
      return no_err;
    }
    
    PathBrowser mode_files(mode_path);
    regex_t seekfor;

    int reerr = 0;

//FIXME reset regexp to default possix 
    if ( ( reerr = regcomp(&seekfor,"\\w+\\.amf$",REG_NEWLINE|REG_NOSUB|REG_ICASE|REG_EXTENDED) ) ){


      char lineNumber[12];

      sprintf(&lineNumber[0],"%i",__LINE__);
      return make_err(ooups,__FILE__,lineNumber);//remove if no oops debugging desired
                                               //better enclose in some debug ifdef
    }

    String possMode;

    while ( mode_files.expand_file_part(&seekfor,possMode) ) {

      String possModeFile = possMode;

      possMode.erase(possMode.length() - 4,4);
      
      int pathPos = 0;
      int pathPosEnd = 0;

      while (    ( (pathPosEnd = possMode.find('/',pathPos)) < possMode.length() )
              && ( pathPosEnd >= 0 ) ) {
        pathPos = pathPosEnd + 1;
      }
      possMode.erase(0,pathPos);

      vector<FilterMode>::iterator fmIt = filterModes.begin();

      for ( fmIt = filterModes.begin() ; 
            fmIt != filterModes.end() ; fmIt++ ) {
        if ( (*fmIt).modeName() == possMode ) {
          break;
        }
      }
      if ( fmIt != filterModes.end() ) {
        continue;
      }

      FILE * in = NULL;

      if ( (in = fopen(possModeFile.c_str(),"rb")) == NULL ) {
        //FIXME is it desired to issue an warning if file can not be read ?
        //      don't think so.
        continue;
      }

      String buf;
      DataPair dp;

      FStream toParse(in,false);

      bool get_sucess = getdata_pair(toParse, dp, buf);
      
      to_lower(dp.key);
      to_lower(dp.value);
      if (    !get_sucess
           || ( dp.key != "mode" ) 
           || ( dp.value != possMode.lower().c_str() ) ) {
        fclose(in);
        regfree(&seekfor);

          char lineNumber[12];

          sprintf(&lineNumber[0],"%i",dp.line_num);
        return make_err(exspect_mode_key,possModeFile.c_str(),lineNumber,"mode");
      }
      get_sucess = getdata_pair(toParse, dp, buf);
      to_lower(dp.key);
      if (    !get_sucess
           || ( dp.key != "aspell" )
           || ( dp.value == NULL )
           || ( *(dp.value) == '\0' ) ) { 
        fclose(in);
        regfree(&seekfor);

          char lineNumber[12];

          sprintf(&lineNumber[0],"%i",dp.line_num);
        return make_err(mode_version_requirement,possModeFile.c_str(),lineNumber);
      }

      char * requirement = dp.value.str;
      char * relop = requirement;
      char swap = '\0';

      if (    ( *requirement == '>' )
           || ( *requirement == '<' )
           || ( *requirement == '!' ) ) {
        requirement++;
      }
      if ( *requirement == '=' ) {
           requirement++;
      }

      String reqVers(requirement);

      swap = *requirement;
      *requirement = '\0';

      String relOp(relop);

      *requirement = swap;

      char actVersion[] = PACKAGE_VERSION;
      char * act = &actVersion[0];
      char * seek = act;

      while (    ( seek != NULL )
              && ( *seek != '\0' ) 
              && ( *seek < '0' )
              && ( *seek > '9' ) 
              && ( *seek != '.' )
              && ( *seek != 'x' )
              && ( *seek != 'X' ) ) {
        seek++;
      }
      act = seek;
      while (    ( seek != NULL )
              && ( seek != '\0' ) 
              && (    (    ( *seek >= '0' )
                        && ( *seek <= '9' ) )
                   || ( *seek == '.' )
                   || ( *seek == 'x' )
                   || ( *seek == 'X' ) ) ) {
        seek++;
      }
      if ( seek != NULL ) {
        *seek = '\0';
      }

      PosibErr<bool> peb = verifyVersion(relOp.c_str(),act,requirement,"add_filter");

      if ( peb.has_err() ) {
        peb.ignore_err();
        fclose(in);
        regfree(&seekfor);

        char lineNumber[12];

        sprintf(&lineNumber[0],"%i",dp.line_num);
        return make_err(confusing_mode_version,possModeFile.c_str(),lineNumber);
      }
      if ( peb == false ) {
        peb.ignore_err();
        fclose(in);
        regfree(&seekfor);

        char lineNumber[12];

        sprintf(&lineNumber[0],"%i",dp.line_num);
        return make_err(bad_mode_version,possModeFile.c_str(),lineNumber);
      }
      
      FilterMode collect(possMode);
      
      while ( getdata_pair(toParse,dp,buf) ) {
        to_lower(dp.key);
        if (    ( dp.key == "des" )
             || ( dp.key == "desc" ) 
             || ( dp.key == "description" ) ) {
          collect.setDescription(dp.value);
          break;
        }
        if ( dp.key == "magic" ) {

          char * regbegin = dp.value;


          while (    regbegin
                  && ( *regbegin != '/' ) ) {
            regbegin++;
          }
          if (    ( regbegin == NULL )
               || ( *regbegin == '\0' ) 
               || ( *(++regbegin) == '\0' ) ) {
            fclose(in);
            regfree(&seekfor);

            char lineNumber[12];

            sprintf(&lineNumber[0],"%i",dp.line_num);
            return make_err(missing_magic_expression,possModeFile.c_str(),lineNumber);
          }
          

          char * regend = regbegin;
          bool prevslash = false;

          while (    regend
                  && ( *regend != '\0' )
                  && (    prevslash
                       || ( * regend != '/' ) ) )  {
            if ( *regend == '\\' ) {
              prevslash = !prevslash;
            }
            else {
              prevslash = false;
            }
            regend ++ ;
          }
          if ( regend == regbegin ) {
            fclose(in);
            regfree(&seekfor);

            char lineNumber[12];

            sprintf(&lineNumber[0],"%i",dp.line_num);
            return make_err(missing_magic_expression,possModeFile.c_str(),lineNumber);
          }

          char swap = *regend;

          *regend = '\0';
          
          String magic(regbegin);
          
          *regend = swap;

          unsigned int extCount = 0;

          while ( *regend != '\0' ) {
            regend ++;
            extCount ++;
            regbegin = regend;
            while (    ( *regend != '/' ) 
                    && ( *regend != '\0' ) ) {
              regend++;
            }
            if ( regend == regbegin ) {
              fclose(in);
              regfree(&seekfor);

              char lineNumber[12];
              char charCount[64];

              sprintf(&lineNumber[0],"%i",dp.line_num);
              sprintf(&charCount[0],"%i",regbegin - (char *)dp.value);
              return  make_err(empty_file_ext,possModeFile.c_str(),lineNumber,charCount);
            }

            bool remove = false;
            bool add = true;

            if ( *regbegin == '+' ) {
              regbegin++;
            }
            else if ( *regbegin == '-' ) {
              add = false;
              remove = true;
              regbegin++;
            }
            if ( regend == regbegin ) {
              fclose(in);
              regfree(&seekfor);

              char lineNumber[12];
              char charCount[64];

              sprintf(&lineNumber[0],"%i",dp.line_num);
              sprintf(&charCount[0],"%i",regbegin - (char *)dp.value);
              return  make_err(empty_file_ext,possModeFile.c_str(),lineNumber,charCount);
            }
            swap = *regend;
            *regend = '\0';
            
            String ext(regbegin);

            *regend = swap;

            // partially unescape magic
            
            magic.ensure_null_end();
            char * dest = magic.data();
            const char * src  = magic.data();
            while (*src) {
              if (*src == '\\' && src[1] == '/' || src[1] == '#')
                ++src;
              *dest++ = *src++;
            }
            magic.resize(dest - magic.data());

            PosibErr<bool> pe;

            if ( remove ) { 
              pe = collect.remModeExtension(ext,magic);
            }
            else {
              pe = collect.addModeExtension(ext,magic);
            }
            if ( pe.has_err() ) {
              fclose(in);
              regfree(&seekfor);

              char lineNumber[12];

              sprintf(&lineNumber[0],"%i",dp.line_num);
              return make_err(error_on_line,possModeFile.c_str(),lineNumber,
                              pe.get_err()->mesg);
            }
          }
          if ( extCount > 0 ) {
            continue;
          }
          fclose(in);
          regfree(&seekfor);
          
          char lineNumber[12];
          char charCount[64];

          sprintf(&lineNumber[0],"%i",dp.line_num);
          sprintf(&charCount[0],"%i",strlen((char *)dp.value));
          return  make_err(empty_file_ext,possModeFile.c_str(),lineNumber,charCount);
        }
        fclose(in);
        regfree(&seekfor);

        char lineNumber[12];

        sprintf(&lineNumber[0],"%i",dp.line_num);
        return make_err(exspect_mode_key,possModeFile.c_str(),lineNumber,
                        "ext[tension]/magic/desc[ription]/rel[ation]");
      }//while getdata_pair
      
      PosibErr<void> pe = collect.build(in,config,dp.line_num,possMode.c_str());

      fclose(in);
      if ( pe.has_err() ) {
        regfree(&seekfor);
        return PosibErrBase(pe);
      }
      filterModes.push_back(collect);
    }
    regfree(&seekfor);
    return no_err;
  }

  void activate_filter_modes(Config *config) {

    ModeNotifierImpl * activate = NULL;

    config->add_notifier((activate = new ModeNotifierImpl(config)));
//    activate->loadModes();//ensure existence of modes
  }

  void print_mode_help(FILE * helpScreen) {
    fprintf(helpScreen,
      "\n\n[Filter Modes] reconfigured combinations filters optimized for files of\n"
          "               a specific type. A mode is selected by Aspell's `--mode\n"
          "               parameter. This will happen implicitly if Aspell is able\n"
          "               to identify the file type form the extension of the\n"
          "               filename.\n"
          "         Note: If the file type can not be identified uniquely by the\n"
          "               file extension Aspell will in addition test the file\n"
          "               content to ensure proper mode selection.\n\n");
    for ( vector<FilterMode>::iterator it = filterModes.begin() ;
          it != filterModes.end() ; it++ ) {
      fprintf(helpScreen,"  %-10s ",(*it).modeName().c_str());

      String desc = (*it).getDescription();
      int preLength = (*it).modeName().length() + 4;

      if ( preLength < 13 ) {
        preLength = 13;
      }
      while ( desc.length() > 74 - preLength ) {

        int locate = 74 - preLength;

        while (    ( locate > 0 )
                && ( desc[locate - 1] != ' ' ) 
                && ( desc[locate - 1] != '\t' )
                && ( desc[locate - 1] != '\n' ) ) {
          locate--;
        }
        if ( locate == 0 ) {
          locate = 74 - preLength;
        }
        
        String prDesc(desc);

        prDesc.erase(locate,prDesc.length() - locate);
        fprintf(helpScreen,"%s\n             ",prDesc.c_str());
        desc.erase(0,locate);
        if (    ( desc.length() > 0 )
             && (    ( desc[0] == ' ' )
                  || ( desc[0] == '\t' )
                  || ( desc[0] == '\n' ) ) ) {
          desc.erase(0,1);
        }
        preLength = 13;
      }
      fprintf(helpScreen,desc.c_str());
      fprintf(helpScreen,"\n");
    }
  }

  PosibErr<void> intialize_filter_modes(Config * config){
    filterModesRef++;

    ModeNotifierImpl * intializer = new ModeNotifierImpl(config);

    PosibErr<void> init_err = intializer->initModes();
    filterModesRef--;
    return init_err;
  }
};

