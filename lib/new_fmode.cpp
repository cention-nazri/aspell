// This file is part of The New Aspell
// Copyright (C) 2002 by Christoph Hintermüller (JEH) under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.
#include "settings.h"

#include <sys/types.h>
#include <regex.h>

#include "string.hpp"
#include "vector.hpp"
#include "config.hpp"
#include "errors.hpp"
#include "filter.hpp"
#include "string_enumeration.hpp"
#include "string_list.hpp"
#include "posib_err.hpp"
#include "file_util.hpp"
#include "fstream.hpp"
#include "getdata.hpp"
#include "strtonum.hpp"
#include "asc_ctype.hpp"
#include "iostream.hpp"

namespace acommon {

  class FilterMode {
  public:
    class MagicString {
    public:
      MagicString(const String & mode) : mode_(mode), fileExtensions() {}
      MagicString(const String & magic, const String & mode)
        : magic_(magic), mode_(mode) {} 
      bool matchFile(FILE * in, const String & ext);
      static PosibErr<bool> testMagic(FILE * seekIn, String & magic, const String & mode);
      void addExtension(const String & ext) { fileExtensions.push_back(ext); }
      bool hasExtension(const String & ext);
      void remExtension(const String & ext);
      MagicString & operator += (const String & ext) {addExtension(ext);return *this;}
      MagicString & operator -= (const String & ext) {remExtension(ext);return *this;}
      MagicString & operator = (const String & ext) { 
        fileExtensions.clear();
        addExtension(ext);
        return *this; 
      }
      const String & magic() const { return magic_; }
      const String & magicMode() const { return mode_; }
      ~MagicString() {}
    private:
      String magic_;
      String mode_;
      Vector<String> fileExtensions;
    };

    FilterMode(const String & name);
    PosibErr<bool> addModeExtension(const String & ext, String toMagic);
    PosibErr<bool> remModeExtension(const String & ext, String toMagic);
    bool lockFileToMode(const String & fileName,FILE * in = NULL);
    const String modeName() const;
    void setDescription(const String & desc) {desc_ = desc;}
    const String & getDescription() {return desc_;}
    PosibErr<void> expand(Config * config);
    PosibErr<void> build(FStream &, Config * config, int line = 1, 
                         const char * name = "mode file");

    ~FilterMode();
  private:
    //map extensions to magic keys 
    String name_;
    String desc_;
    String file_;
    Vector<MagicString> magicKeys;
    struct KeyValue {
      String key;
      String value;
      KeyValue() {}
      KeyValue(ParmStr k, ParmStr v) : key(k), value(v) {}
    };
    Vector<KeyValue> expansion;
  };

  FilterMode::FilterMode(const String & name)
  : name_(name) {}

  PosibErr<bool> FilterMode::addModeExtension(const String & ext, String toMagic) {

    bool extOnly = false;
    
    if (    ( toMagic == "" )
         || ( toMagic == "<nomagic>" )
         || ( toMagic == "<empty>" ) ) {
      extOnly = true;
    }
    else {

      RET_ON_ERR(FilterMode::MagicString::testMagic(NULL,toMagic,name_));

    } 

    Vector<MagicString>::iterator it;

    for ( it = magicKeys.begin() ; it != magicKeys.end() ; it++ ) {
      if (    (    extOnly
                && ( it->magic() == "" ) )
           || ( it->magic() == toMagic ) ) {
        *it += ext;
        return true;
      }
    }
    if ( it != magicKeys.end() ) {
      return false;
    }
    if ( extOnly ) {
      magicKeys.push_back(MagicString(name_));
    }
    else {
      magicKeys.push_back(MagicString(toMagic,name_));
    }
    for ( it = magicKeys.begin() ; it != magicKeys.end() ; it++ ) {
      if (    (    extOnly
                && ( it->magic() == "" ) )
           || ( it->magic() == toMagic ) ) {
        *it += ext;
        return true;
      }
    }
    return make_err(mode_extend_expand,name_.str());
  }

  PosibErr<bool> FilterMode::remModeExtension(const String & ext, String toMagic) {

    bool extOnly = false;

    if (    ( toMagic == "" )
         || ( toMagic == "<nomagic>" )
         || ( toMagic == "<empty>" ) ) {
      extOnly = true;
    }
    else {

      PosibErr<bool> pe = FilterMode::MagicString::testMagic(NULL,toMagic,name_);

      if ( pe.has_err() ) {
        return PosibErrBase(pe);
      }
    }

    for ( Vector<MagicString>::iterator it = magicKeys.begin() ;
          it != magicKeys.end() ; it++ ) {
      if (    (    extOnly
                && ( it->magic() == "" ) )
           || ( it->magic() == toMagic ) ) {
        *it -= ext;
        return true;
      }
    }
    return false;
  }

  bool FilterMode::lockFileToMode(const String & fileName,FILE * in) {

    Vector<unsigned int> extStart;
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
      in = fopen(fileName.str(),"rb");
      closeFile= true;
    }
    for ( Vector<unsigned int>::iterator extSIt = extStart.begin() ;
          extSIt != extStart.end() ; extSIt ++ ) {
    
      String ext(fileName);

      ext.erase(0,*extSIt);
      for ( Vector<MagicString>::iterator it = magicKeys.begin() ;
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
    return name_;
  }

  FilterMode::~FilterMode() {
  }

  bool FilterMode::MagicString::hasExtension(const String & ext) {
    for ( Vector<String>::iterator it = fileExtensions.begin() ;
          it != fileExtensions.end() ; it++ ) {
      if ( *it == ext ) {
        return true;
      }
    }
    return false;
  }

  void FilterMode::MagicString::remExtension(const String & ext) {
    for ( Vector<String>::iterator it = fileExtensions.begin() ;
          it != fileExtensions.end() ; it++ ) {
      if ( *it == ext ) {
        fileExtensions.erase(it);
      }
    }
  }


  bool FilterMode::MagicString::matchFile(FILE * in,const String & ext) {

    Vector<String>::iterator extIt;

    for ( extIt = fileExtensions.begin() ; 
          extIt != fileExtensions.end() ; extIt ++ ) {
      if ( *extIt == ext ) {
        break;
      }
    }
    if ( extIt == fileExtensions.end() ) {
      return false;
    }

    PosibErr<bool> pe = testMagic(in,magic_,mode_);

    if ( pe.has_err() ) {
      pe.ignore_err();
      return false;
    }
    return true;
  }


  PosibErr<bool> FilterMode::MagicString::testMagic(FILE * seekIn,String & magic,const String & mode) {

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

    char * num = (char *)number.str();
    char * numEnd = num + number.length();
    char * endHere = numEnd;
    long position = 0;

    if (    ( number.length() == 0 ) 
         || ( (position = strtoi_c(num,&numEnd)) < 0 )
         || ( numEnd != endHere ) ) {
      return make_err(file_magic_pos,"",magic.str());
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
      return make_err(missing_magic,mode.str(),magic.str()); //no regular expression given
    }
    
    number = magic;
    number.erase(magicFilePosition,magic.length() - magicFilePosition);
    number.erase(0,seekRangePos);//already incremented by one see above
    num = (char*)number.str();
    endHere = numEnd = num + number.length();

    if (    ( number.length() == 0 )
         || ( (position = strtoi_c(num,&numEnd)) < 0 )
         || ( numEnd != endHere ) ) {
      if ( seekIn != NULL ) {
        rewind(seekIn);
      }
      return make_err(file_magic_range,mode.str(),magic.str());//no magic range given
    }

    regex_t seekMagic;
    int regsucess = 0;

    if ( (regsucess = regcomp(&seekMagic,magicRegExp.str(),
                              REG_NEWLINE|REG_NOSUB|REG_EXTENDED)) ){
      if ( seekIn != NULL ) {
        rewind(seekIn);
      }

      char regError[256];
      regerror(regsucess,&seekMagic,&regError[0],256);
      return make_err(bad_magic,mode.str(),magic.str(),regError);
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

    config->replace("clear-filter","");
    for ( Vector<KeyValue>::iterator it = expansion.begin() ;
          it != expansion.end() ; it++ ) 
    {
      PosibErr<void> pe = config->replace(it->key, it->value);
      if (pe.has_err()) return pe.with_file(file_);
    }
    return no_err;  
  }

  PosibErr<void> FilterMode::build(FStream & toParse, Config * config, int line0, const char * name) {

    String buf;
    DataPair dp;
    dp.line_num = line0;

    while ( getdata_pair(toParse, dp, buf) ) {

      to_lower(dp.key);

      if ( dp.key == "filter" ) {

        to_lower(dp.value);
        expansion.push_back(KeyValue("add-filter", dp.value));

      } else if ( dp.key == "option" ) {

        split(dp);
        // FIXME: Add check for empty key

        expansion.push_back(KeyValue(dp.key, dp.value));

      } else {
        
        return make_err(bad_mode_key,dp.key).with_file(name,dp.line_num);
      }
    }

    return no_err;
  }

  // FIXME: Use cache
  static Vector<FilterMode> filterModes;
  int filterModesRef = 0;



  void set_mode_from_extension (Config * config, ParmString filename, FILE * in) {
    for ( Vector<FilterMode>::iterator it = filterModes.begin() ;
         it != filterModes.end() ; it++ ) {
      if ( it->lockFileToMode(filename,in) ) {
        config->replace("mode", it->modeName().str());
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

    PosibErr<void> item_updated(const KeyInfo * ki, ParmStr);
    PosibErr<void> item_added(const KeyInfo * ki, ParmStr, int pos);

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

  PosibErr<void> ModeNotifierImpl::item_updated(const KeyInfo * ki, ParmStr value)
  {
    if ( strcmp(ki->name, "mode") == 0 ) {
      for ( Vector<FilterMode>::iterator it = filterModes.begin() ;
            it != filterModes.end() ; it++ ) {
        if ( it->modeName() == value )
          return it->expand(config);
      }
      return make_err(unknown_mode, value); 
    }
    return no_err;
  }

  PosibErr<void> ModeNotifierImpl::item_added(const KeyInfo * ki, ParmStr value, int)
  {
    if (strcmp(ki->name, "filter-path") == 0) {
      return intLoadModes();
    }
    return no_err;
  }

  PosibErr<void> ModeNotifierImpl::intLoadModes(const bool reset) {


//FIXME is filter-path proper for filter mode files ???
//      if filter-options-path better ???
//      do we need a filter-mode-path ???
//      should change to use genetic data-path once implemented
//        and then search filter-path - KevinA
    RET_ON_ERR(config->retrieve_list("filter-path",&mode_path));
    if (mode_path.empty()) return no_err;
    
    PathBrowser els(mode_path, ".amf");

    String possMode;
    String possModeFile;

    const char * file;
    while ((file = els.next()) != NULL) 
    {
      possModeFile = file;
      possMode.assign(possModeFile.str(), possModeFile.size() - 4);

      unsigned pathPos = 0;
      unsigned pathPosEnd = 0;

      while (    ( (pathPosEnd = possMode.find('/',pathPos)) < possMode.length() )
              && ( pathPosEnd >= 0 ) ) {
        pathPos = pathPosEnd + 1;
      }
      possMode.erase(0,pathPos);

      Vector<FilterMode>::iterator fmIt = filterModes.begin();

      for ( fmIt = filterModes.begin() ; 
            fmIt != filterModes.end() ; fmIt++ ) {
        if ( (*fmIt).modeName() == possMode ) {
          break;
        }
      }
      if ( fmIt != filterModes.end() ) {
        continue;
      }

      FStream toParse;

      RET_ON_ERR(toParse.open(possModeFile.str(),"rb"));

      String buf;
      DataPair dp;

      bool get_sucess = getdata_pair(toParse, dp, buf);
      
      to_lower(dp.key);
      to_lower(dp.value);
      if (    !get_sucess
           || ( dp.key != "mode" ) 
           || ( dp.value != possMode.lower().str() ) )
        return make_err(expect_mode_key,"mode").with_file(possModeFile, dp.line_num);

      get_sucess = getdata_pair(toParse, dp, buf);
      to_lower(dp.key);
      if (    !get_sucess
           || ( dp.key != "aspell" )
           || ( dp.value == NULL )
           || ( *(dp.value) == '\0' ) )
        return make_err(mode_version_requirement).with_file(possModeFile, dp.line_num);

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

      PosibErr<bool> peb = verify_version(relOp.str(),act,requirement,"add_filter");

      if ( peb.has_err() ) {
        peb.ignore_err();
        return make_err(confusing_mode_version).with_file(possModeFile, dp.line_num);
      }
      if ( peb == false ) {
        return make_err(bad_mode_version).with_file(possModeFile, dp.line_num);
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
               || ( *(++regbegin) == '\0' ) )
            return make_err(missing_magic_expression).with_file(possModeFile, dp.line_num);
          
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
          if ( regend == regbegin )
            return make_err(missing_magic_expression).with_file(possModeFile, dp.line_num);

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
            if ( regend == regbegin ) 
            {
              char charCount[64];
              sprintf(&charCount[0],"%i",regbegin - (char *)dp.value);
              return  make_err(empty_file_ext,charCount).with_file(possModeFile,dp.line_num);
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
            if ( regend == regbegin ) 
            {
              char charCount[64];
              sprintf(&charCount[0],"%i",regbegin - (char *)dp.value);
              return  make_err(empty_file_ext,charCount).with_file(possModeFile,dp.line_num);
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

            if ( remove )
              pe = collect.remModeExtension(ext,magic);
            else
              pe = collect.addModeExtension(ext,magic);

            if ( pe.has_err() )
              return pe.with_file(possModeFile, dp.line_num);
          }

          if (extCount > 0 ) continue;

          char charCount[64];
          sprintf(&charCount[0],"%i",strlen((char *)dp.value));
          return  make_err(empty_file_ext,charCount).with_file(possModeFile,dp.line_num);
        }

        return make_err(expect_mode_key,"ext[tension]/magic/desc[ription]/rel[ation]")
          .with_file(possModeFile,dp.line_num);
      
      }//while getdata_pair
      
      RET_ON_ERR(collect.build(toParse,config,dp.line_num,possMode.str()));

      filterModes.push_back(collect);
    }
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
    for ( Vector<FilterMode>::iterator it = filterModes.begin() ;
          it != filterModes.end() ; it++ ) {
      fprintf(helpScreen,"  %-10s ",(*it).modeName().str());

      String desc = (*it).getDescription();
      int preLength = (*it).modeName().length() + 4;

      if ( preLength < 13 ) {
        preLength = 13;
      }
      while ( (int)desc.size() > 74 - preLength ) {

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
        fprintf(helpScreen,"%s\n             ",prDesc.str());
        desc.erase(0,locate);
        if (    ( desc.length() > 0 )
             && (    ( desc[0] == ' ' )
                  || ( desc[0] == '\t' )
                  || ( desc[0] == '\n' ) ) ) {
          desc.erase(0,1);
        }
        preLength = 13;
      }
      fprintf(helpScreen,desc.str());
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

