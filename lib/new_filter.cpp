// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson and Christoph Hintermüller under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.
//
// Expansion for loading filter libraries and collections upon startup
// was added by Christoph Hintermüller

#include "settings.h"


#include "asc_ctype.hpp"
#include "config.hpp"
#include "directory.hpp"
#include "enumeration.hpp"
#include "errors.hpp"
#include "filter.hpp"
#include "filter_entry.hpp"
#include "fstream.hpp"
#include "getdata.hpp"
#include "indiv_filter.hpp"
#include "iostream.hpp"
#include "itemize.hpp"
#include "key_info.hpp"
#include "parm_string.hpp"
#include "posib_err.hpp"
#include "stack_ptr.hpp"
#include "string_enumeration.hpp"
#include "string_list.hpp"
#include "string_map.hpp"

#ifdef HAVE_LIBDL
#include <dlfcn.h>
#endif
#define DEBUG {fprintf(stderr,"File: %s(%i)\n",__FILE__,__LINE__);}

#ifdef NULL
#undef NULL
#endif
#define NULL 0

namespace acommon
{

#include "static_filters.src.cpp"

  //
  // filter modes
  // 

  const char * filter_modes = "none,url,email,sgml,tex";
  
  class IndividualFilter;

  static int filter_modules_referencing=0;

  //
  // actual code
  //

  FilterEntry * find_individual_filter(ParmString);

  class ExtsMap : public StringMap 
  {
    const char * cur_mode;
  public:
    void set_mode (ParmString mode)
    {
      cur_mode = mode;
    }
    PosibErr < bool > add (ParmString key)
    {
      insert (key, cur_mode);
      return true;
    }
  };

  void set_mode_from_extension (Config * config, ParmString filename)
  {

    // Initialize exts mapping
    StringList modes;
    itemize(filter_modes, modes);
    StringListEnumeration els = modes.elements_obj();
    const char * mode;
    ExtsMap exts;
    while ((mode = els.next ()) != 0)
      {
      exts.set_mode(mode);
      String to_find = mode;
      to_find += "-extension";
      PosibErr<void> err = config->retrieve_list(to_find, &exts);
      err.ignore_err();
    }
    const char * ext0 = strrchr(filename, '.');
    if (ext0 == 0)
      ext0 = filename;
    else
      ++ext0;
    String ext = ext0;
    for (unsigned int i = 0; i != ext.size(); ++i)
      ext[i] = asc_tolower(ext[i]);
    mode = exts.lookup(ext);
    if (mode != 0)
      config->replace("mode", mode);
  }

  class FilterHandle {
  public:
    FilterHandle() : handle(0) {}
    ~FilterHandle() {
      if (handle) {
        dlclose(handle);
      }
    }
    void * release() {
      void * tmp = handle;
      handle = 0;
      return tmp;
    }
    operator bool() {
      return handle != NULL;
    }
    void * val() {
      return handle;
    }
//The direct interface usually when new_filter ... functions are coded
//manually
    FilterHandle & operator= (void * h) {
//FIXME only true for first filter but not for multible filters 
//      assert(handle == NULL);
      handle=h; return *this;
    }
  private:
    void * handle;
  };

  PosibErr<void> setup_filter(Filter & filter, 
			      Config * config, 
			      bool use_decoder, 
			      bool use_filter, bool use_encoder)
  {
    StringList sl;
    config->retrieve_list("filter", &sl);
    StringListEnumeration els = sl.elements_obj();
    StackPtr<IndividualFilter> ifilter;
    const char * filter_name;
    String filtername;
    FilterHandle filterhandle[3];
    FilterEntry dynamicfilter;
    int addcount=0;
    ConfigModule * currentfilter=NULL;

    filter.clear();
    while ((filter_name = els.next()) != 0) {
      filterhandle[0]=filterhandle[1]=filterhandle[2]=(void*)NULL;
      addcount=0;
      //fprintf(stderr, "Loading %s ... \n", filter_name);
      FilterEntry * f = find_individual_filter(filter_name);
      // Changed for reflecting new filter loadability dependent uppon
      // existance of libdl if there is not libdl than the following
      // behaves like as ever means filter name different from
      // standard filters is rejected if libdl is existent filter name
      // different from standard filters is checked against all
      // filters added. If the filter is contained the corresponding
      // decoder encoder and filter is loaded.
#ifdef HAVE_LIBDL
      if (!f) {

        for (currentfilter = (ConfigModule*)filter_modules_begin+standard_filters_size;
	     currentfilter < (ConfigModule*)filter_modules_end; 
	     currentfilter++) 
	  {
	    if (strcmp(currentfilter->name,filter_name) == 0) {
	      break;
          }
        }
        if (currentfilter >= filter_modules_end) {
          return make_err(other_error);
        }
        if (((filterhandle[0]=dlopen(currentfilter->load,RTLD_NOW)) == NULL) ||
            ((filterhandle[1]=dlopen(currentfilter->load,RTLD_NOW)) == NULL) ||
            ((filterhandle[2]=dlopen(currentfilter->load,RTLD_NOW)) == NULL)) {
          return make_err(cant_dlopen_file,"filter setup",filter_name,dlerror());
        }
        dynamicfilter.decoder = (FilterFun *)dlsym(filterhandle[0].val(),"new_decoder");
        dynamicfilter.encoder = (FilterFun *)dlsym(filterhandle[1].val(),"new_encoder");
        dynamicfilter.filter  = (FilterFun *)dlsym(filterhandle[2].val(),"new_filter");
        if (!dynamicfilter.decoder &&
	    !dynamicfilter.encoder &&
	    !dynamicfilter.filter) {
          return make_err(empty_filter,"filter setup",filter_name);
        }
        dynamicfilter.name=filter_name;
        f=&dynamicfilter;
      } else {
        addcount=1;
      }
#else
      assert(f); //FIXME: Return Error Condition
#endif
      if (use_decoder && f->decoder && (ifilter=f->decoder())) {
        RET_ON_ERR_SET(ifilter->setup(config), bool, keep);
	if (!keep) {
	  ifilter.del();
	} else {
          filter.add_filter(ifilter.release(),filterhandle[0].release(),
                            Filter::DECODER);
        }
      } 
      if (use_filter && f->filter && (ifilter=f->filter())) {
        RET_ON_ERR_SET(ifilter->setup(config), bool, keep);
        if (!keep) {
          ifilter.del();
        } else {
          filter.add_filter(ifilter.release(), filterhandle[2].release(),
                            Filter::FILTER);
        }
      }
      if (use_encoder && f->encoder && (ifilter=f->encoder())) {
        RET_ON_ERR_SET(ifilter->setup(config), bool, keep);
        if (!keep) {
          ifilter.del();
        } else {
          filter.add_filter(ifilter.release(), filterhandle[1].release(),
                            Filter::ENCODER);
        }
      }
    }
    return no_err;
  }

//This wasn't changed at all
  FilterEntry * find_individual_filter(ParmString filter_name) {
    unsigned int i = 0;
    while (i != standard_filters_size) {
      if (standard_filters[i].name == filter_name) {
	return (FilterEntry *) standard_filters + i;
      }
      ++i;
    }
    return 0;
  }

  
  // the FilterOptionExpandNotifier was added in order to be able to
  // expand filter and corresponding Option list during runtime.
  // It implements the entire loadability if not loaded and handed to
  // Config class via addnotifier there will not be any filter
  // loadability
  // If shared between multiple config objects having their own
  // FilterOptionExpandNotifier class each of them increments the
  // filter_modules_referencing counter in order to indicate that they
  // too changes the filter modules structure
  class FilterOptionExpandNotifier : public Notifier {
    PathBrowser optionpath;
    PathBrowser filterpath;
    FilterOptionExpandNotifier(void) {
      filter_modules_referencing++;
    }
    FilterOptionExpandNotifier(const FilterOptionExpandNotifier & brother);
    void operator=(const FilterOptionExpandNotifier & brother);
    void release_options(const KeyInfo * begin,const KeyInfo * end);
  public:
    Config * config;

    FilterOptionExpandNotifier(Config * conf);
    virtual ~FilterOptionExpandNotifier(void);
    virtual Notifier * clone(Config * conf);
    virtual PosibErr<void> item_added(const KeyInfo * key, ParmString value);
    virtual PosibErr<void> item_updated(const KeyInfo * key, ParmString value);
//FIXME Add item_removed member to clear away filter options 
  };


  void activate_dynamic_filteroptions(Config * config){
    config->add_notifier(new FilterOptionExpandNotifier(config));
  }


  FilterOptionExpandNotifier::FilterOptionExpandNotifier(const FilterOptionExpandNotifier & brother)
  : optionpath(),
    filterpath()
  {
    *this=brother;
    filter_modules_referencing++;
  }
  void  FilterOptionExpandNotifier::operator=(const FilterOptionExpandNotifier & brother) {
    optionpath=brother.optionpath;
    filterpath=brother.filterpath;
  }

  void FilterOptionExpandNotifier::release_options(const KeyInfo * begin,const KeyInfo * end) {
    KeyInfo * current=NULL;
    
    if (begin == NULL) {
      return;
    }
    for (current=(KeyInfo*)begin;current < end; current++) {
      if (current->name) {
        free((char*)current->name);
      }
      if (current->def) {
        free((char*)current->def);
      }
      if (current->desc) {
        free((char*)current->desc);
      }
    }
  }


  FilterOptionExpandNotifier::FilterOptionExpandNotifier(Config * conf) 
  : optionpath(),
    filterpath(), 
    config(conf) 
  {
    filter_modules_referencing++;
    do {
      StringList test;
      config->retrieve_list("option-path",&test);
      optionpath=test;
    } while (false);
    do {
      StringList test;
      config->retrieve_list("filter-path",&test);
      filterpath=test;
    } while (false);
  }

//  extern const size_t filter_modules_size;

  FilterOptionExpandNotifier::~FilterOptionExpandNotifier(void) {
    int countextended=filter_modules_size/sizeof(ConfigModule);

    if (--filter_modules_referencing == 0) {
      if (filter_modules_begin != &filter_modules[0]) {
        for( ; countextended < filter_modules_end-filter_modules_begin;
            countextended++ ){
          if (filter_modules_begin[countextended].name != NULL) {
            free((char*)filter_modules_begin[countextended].name);
          }
          if (filter_modules_begin[countextended].load != NULL) {
            free((char*)filter_modules_begin[countextended].load);
          }
          release_options(filter_modules_begin[countextended].begin,
                          filter_modules_begin[countextended].end);
          if (filter_modules_begin[countextended].begin) {
            free((KeyInfo*)filter_modules_begin[countextended].begin);
          }
        }
        free((ConfigModule*)filter_modules_begin);
        filter_modules_begin=(ConfigModule*)&filter_modules[0];
        filter_modules_end=(ConfigModule*)filter_modules_begin+filter_modules_size/
                                                               sizeof(ConfigModule);
      }
      if (config != NULL) {
        config->set_modules(filter_modules_begin, filter_modules_end);
      }
    }
  }

  Notifier * FilterOptionExpandNotifier::clone(Config * conf) {
    return new FilterOptionExpandNotifier(conf); 
  }

  PosibErr<void> FilterOptionExpandNotifier::item_added(const KeyInfo * key, ParmString value) {
    int namelength=strlen(key->name);
    ConfigModule * current = (ConfigModule*)filter_modules_begin;
    String optionname="";
    String filtername="lib";
    FStream options;
    String optionkey;
    String optionkeyvalue;
    String version=PACKAGE_VERSION;
    unsigned int optionstart=0;
    unsigned int optioncount=0;
    KeyInfo * begin=NULL;
    int optsize=0;
    ConfigModule * mbegin=NULL;
    int modsize=filter_modules_end-filter_modules_begin;
    void * help=NULL;
    StringList filtpath;
    StringList optpath;
    bool greater;
    bool equal;
    bool less;
    int linecount=0;
    char linenumber[9]="0";
    int activeoption=0;
    String expand="filter-";
    int norealoption=0;


    if ((namelength == 6) &&
        !strncmp(key->name,"filter",6)){
      //fprintf(stderr,"Expanding for %s ... \n",value.str());
      while (current < filter_modules_end) {
        if (!strncmp(value.str(), current->name,
                     value.size() <= strlen(current->name)?
                     value.size():strlen(current->name))) {
          return no_err;
        }
        current++;
      }
      if (current >= filter_modules_end) {
        optionname+=value;
        optionname+="-filter.opt";
        filtername+=value;
        filtername+="-filter.so";
        if (!filterpath.expand_filename(filtername)) {
          filtername=value;
          filtername+=".flt";
          if (!filterpath.expand_filename(filtername)) {
            return make_err(no_such_filter, "add-filter", value);
          }
          RET_ON_ERR(options.open(filtername,"r"));

bool emptyfile=true;

          while (getdata_pair(options,optionkey,optionkeyvalue)) {
            if (((optionkey == "add-filter") ||
                 (optionkey == "rem-filter")) &&
                 (optionkeyvalue == value)) {
              fprintf(stderr,"warning: specifying filter twice makes no sense\n"
                             "\tignoring `%s %s'\n",
                             optionkey.c_str(),optionkeyvalue.c_str());
              continue;
            }
            emptyfile=false;
            RET_ON_ERR(config->replace(optionkey.c_str(),optionkeyvalue.c_str()));
          }
          if (emptyfile) {
            return make_err(empty_filter,"filter setup",filtername);
          }
          config->replace("rem-filter",value);
          return no_err;
        }
        else if (!optionpath.expand_filename(optionname)) {
          return make_err(no_options,"add_filter",optionname,"Options missing");

        }
        if (config->have(value)) {
          fprintf(stderr,"warning: specifying filter twice makes no sense\n");
          return no_err;
        }
        RET_ON_ERR(options.open(optionname,"r"));
        greater=equal=less=false;
        while (getdata_pair(options,optionkey,optionkeyvalue)) {
          linecount++;
          if (optionkey.no_case() == "aspell") {
            optionstart=0;
            if ((optionkeyvalue.length() > optionstart) &&
                (optionkeyvalue[optionstart] == '>')) {
              greater=true;
              optionstart++;
            }
            if ((optionkeyvalue.length() > optionstart) &&
                (optionkeyvalue[optionstart] == '<')) {
              less=true;
              optionstart++;
            }
            if ((optionkeyvalue.length() > optionstart) &&
                (optionkeyvalue[optionstart] == '=')) {
              equal=true;
              optionstart++;
            }
            if (optionstart == 0) {
              equal=true;
            }
            if ((optionkeyvalue.length() > optionstart) &&
                !asc_isdigit(optionkeyvalue[optionstart])) {
              sprintf(linenumber,"%i",linecount);
              return make_err(confusing_version,"add_filter",optionname,linenumber);
            }
            optionkeyvalue.erase(0,optionstart);
            for (optioncount=0;(optioncount < optionkeyvalue.length()) &&
                               (optioncount < version.length());
                 optioncount++) {
              if (asc_isdigit(optionkeyvalue[optioncount]) &&
                  asc_isdigit(version[optioncount])) {
                if (greater &&
                    ((optionkeyvalue[optioncount] < version[optioncount]) ||
                     ((optionkeyvalue[optioncount] == version[optioncount]) &&
                      (optionkeyvalue.length()-1 == optioncount) &&
                      (optionkeyvalue.length() < version.length())))) {
                   break;
                }
                if (less &&
                    ((optionkeyvalue[optioncount] > version[optioncount]) ||
                     ((optionkeyvalue[optioncount] == version[optioncount]) &&
                      (version.length()-1 == optioncount) &&
                      (optionkeyvalue.length() > version.length())))) {
                  break;
                }
                if (optionkeyvalue[optioncount] == version[optioncount]) {
                  if (equal &&
                      (version.length()-1 == optioncount) &&
                      (optionkeyvalue.length()-1 == optioncount)) {
                    break;
                  }
                  else if ((version.length()-1 > optioncount) &&
                           (optionkeyvalue.length()-1 > optioncount)) {
                    continue;
                  }
                }
                sprintf(linenumber,"%i",linecount);
                return make_err(bad_version,"add-filter",optionname,linenumber);
              }
              if (less &&
                  asc_isdigit(optionkeyvalue[optioncount]) &&
                  (version[optioncount] == '.' ) &&
                  (version.length()-1 > optioncount)) {
                break;
              }
              if (greater &&
                  asc_isdigit(version[optioncount]) &&
                  (optionkeyvalue[optioncount] == '.') &&
                  (optionkeyvalue.length()-1 > optioncount)) {
                break;
              }
              if ((version[optioncount] == '.') &&
                  (optionkeyvalue[optioncount] == '.') &&
                  (version.length()-1 > optioncount) &&
                  (optionkeyvalue.length()-1 > optioncount)) {
                continue;
              }
              sprintf(linenumber,"%i",linecount);
              return make_err(confusing_version,"add_filter",optionname,linenumber);
            }
            continue;
          }
          if ((optionkey.no_case() == "option") || 
              (!activeoption && 
               ((optionkey.no_case() == "desc") ||
                (optionkey.no_case() == "description")) && 
               (norealoption=1))) {
            if (!norealoption && config->have(optionkeyvalue.c_str())) {
              fprintf(stderr,"option %s: might conflict with Aspell option\n"
                             "try to prefix it by `filter-'\n",
                      optionkeyvalue.c_str());
            }
            if ((!norealoption) && 
                (begin == NULL)) {
              if ((help=realloc(begin,(optsize+=1)*sizeof(KeyInfo))) == NULL) {
                if (begin != NULL) {
                  release_options(begin,begin+optsize-1);
                  free(begin);
                }
                return make_err(cant_extend_options,"add_filter",value);
              }
              begin=(KeyInfo*)help;
              begin[optsize-1].name=begin[optsize-1].def=begin[optsize-1].desc=NULL;
              begin[optsize-1].type=KeyInfoDescript;
            }
            if (!norealoption || (begin == NULL)) {
              if ((help=realloc(begin,(optsize+=1)*sizeof(KeyInfo)) ) == NULL) {
                if (begin != NULL) {
                  release_options(begin,begin+optsize-1);
                  free(begin);
                }
                return make_err(cant_extend_options,"add_filter",value);
              }
              begin=(KeyInfo*)help;
              begin[optsize-1].name=begin[optsize-1].def=begin[optsize-1].desc=NULL;
            }
            if (norealoption && (begin != NULL)) {
              begin[0].type = KeyInfoDescript;
              begin[0].def  = NULL;
              if (begin[0].desc != NULL) {
                free((char*)begin[0].desc);
                begin[0].desc = NULL;
              }
              if (optionkeyvalue.length() == 0) {
                optionkeyvalue="-";
              }
              if ((begin[0].desc=strdup(optionkeyvalue.c_str())) == NULL ) {
                if (begin !=NULL) {
                  release_options(begin,begin+optsize);
                  free(begin);
                }
                return make_err(cant_describe_filter,"add_filter",value);
              }
              if (begin[0].name == NULL) {
                if ((begin[0].name = 
		     (const char *)malloc(strlen(value)+strlen("filter-")+1)) == NULL) {
                  if (begin !=NULL) {
                    release_options(begin,begin+optsize);
                    free(begin);
                  }
                  return make_err(cant_describe_filter,"add_filter",value);
                }
                ((char *)begin[0].name)[0]='\0';
                strncat((char*)begin[0].name,"filter-",7);
                strncat((char*)begin[0].name,value,strlen(value));
              }
            }
            else {
              expand="filter-";
              expand+=optionkeyvalue;
              if (config->have(expand.c_str())) {
                if (begin != NULL) {
                  release_options(begin,begin+optsize);
                  free(begin);
                }
                optionkeyvalue.insert(0,"(filter-)");
                sprintf(linenumber,"%i",linecount);
                return make_err(identical_option,"add_filter",optionname,linenumber);
              }
              if (((begin[optsize-1].name)=strdup(optionkeyvalue.c_str())) == NULL) {
                if (begin !=NULL) {
                  release_options(begin,begin+optsize);
                  free(begin);
                }
                return make_err(cant_extend_options,"add_filter",value);
              }
              begin[optsize-1].type=KeyInfoBool;
              begin[optsize-1].def  = NULL;
              begin[optsize-1].desc = NULL;
              begin[optsize-1].otherdata[0]='\0';
              activeoption=1;
            }
            norealoption=0;
            continue;
          }
          if (optionkey.no_case() == "static") {
            //fprintf(stderr,"Filter %s consists of `%s'\n",value.str(),
            //        optionkeyvalue.c_str());
            activeoption=0;
            continue;
          }
          if (!activeoption) {
            if (begin != NULL) {
              free(begin);
            }
            sprintf(linenumber,"%i",linecount);
            return make_err(options_only,"add_filter",optionname,linenumber);
          }
          if (optionkey.no_case() == "type") {
            if (optionkeyvalue.no_case() == "list") {
              begin[optsize-1].type=KeyInfoList;
              continue;
            }
            if ((optionkeyvalue.no_case() == "int") ||
                (optionkeyvalue.no_case() == "integer")) {
              begin[optsize-1].type=KeyInfoInt;
              continue;
            }
            if (optionkeyvalue.no_case() == "string") {
              begin[optsize-1].type=KeyInfoString;
              continue;
            }
            begin[optsize-1].type=KeyInfoBool;
            continue;
          }
          if ((optionkey.no_case() == "def") ||
              (optionkey.no_case() == "default")) {
//Type detection ???
            if (begin[optsize-1].type == KeyInfoList) {
              if (begin[optsize-1].def != NULL) {
                optionkeyvalue+=",";
                optionkeyvalue+=begin[optsize-1].def;
                free((void*)begin[optsize-1].def);
              }
            }  
            if (((begin[optsize-1].def)=strdup(optionkeyvalue.c_str()) ) == NULL) {
              if (begin != NULL) {
                release_options(begin,begin+optsize);
                free(begin);
              }
              return make_err(cant_extend_options,"add_filter",value);
            }
            continue;
          }
          if ((optionkey.no_case() == "desc") ||
              (optionkey.no_case() == "description")) {
            if (((begin[optsize-1].desc)=strdup(optionkeyvalue.c_str())) == NULL) {
              if (begin != NULL) {
                release_options(begin,begin+optsize);
                free(begin);
              }
              return make_err(cant_extend_options,"add_filter",value);
            }
            continue;
          }
          if (optionkey.no_case() == "other") {
            strncpy(begin[optsize-1].otherdata,optionkeyvalue.c_str(),15);
            begin[optsize-1].otherdata[15]='\0';
            continue;
          }
          if (optionkey.no_case()=="endoption") {
            activeoption=0;
            continue;
          }
          if (optionkey.no_case() == "endfile") {
            break;
          }
          if (begin != NULL) {
            release_options(begin,begin+optsize);
            free(begin);
          }
          sprintf(linenumber,"%i",linecount);
          return make_err(invalid_option_modifier,"add_filter",optionname,linenumber);
        }
        if ((begin == NULL) || 
            (begin[0].type != KeyInfoDescript) ||
            (begin[0].name == NULL) || 
            (begin[0].desc == NULL)) {
          if (begin != NULL) {
            release_options(begin,begin+optsize);
            free(begin);
          }
          return make_err(cant_extend_options,"add_filter",value);
        }
        if (filter_modules_begin == filter_modules) {
          if ((mbegin=(ConfigModule*)malloc(modsize*sizeof(ConfigModule))) == NULL) {     
            if (begin != NULL) {
              release_options(begin,begin+optsize);
              free(begin);
            }
            return make_err(cant_extend_options,"add_filter",value);
          }
          memcpy(mbegin,filter_modules_begin,(modsize)*sizeof(ConfigModule));
          filter_modules_begin = mbegin;
          filter_modules_end   = mbegin+modsize;
          config->set_modules(filter_modules_begin,filter_modules_end);
        }
        if ((mbegin=(ConfigModule*)realloc((ConfigModule*)filter_modules_begin,
                     ++modsize*sizeof(ConfigModule))) == NULL) {     
          if (begin != NULL) {
            release_options(begin,begin+optsize);
            free(begin);
          }
          return make_err(cant_extend_options,"add_filter",value);
        }
        mbegin[modsize-1].name  = strdup(value);
        mbegin[modsize-1].load  = strdup(filtername.c_str());
        mbegin[modsize-1].begin = begin;
        mbegin[modsize-1].end   = begin+optsize;
        filter_modules_begin = mbegin;
        filter_modules_end   = mbegin+modsize;
        config->set_modules(filter_modules_begin,filter_modules_end);
        return no_err;
      }
      return make_err(no_such_filter,"add_filter",value);
    }
    else if ((namelength == 11) &&
             !strncmp(key->name,"filter-path",11)) {
      RET_ON_ERR(config->retrieve_list("filter-path",&filtpath));
      filterpath=filtpath;
    }
    else if ((namelength == 11) &&
             !strncmp(key->name,"option-path",15)) {
      RET_ON_ERR(config->retrieve_list("option-path",&optpath));
      optionpath=optpath;
    }
    return no_err;
  }

  PosibErr<void> FilterOptionExpandNotifier::item_updated(const KeyInfo * key, ParmString value){
    int namelength=strlen(key->name);
    String optionname="";
    String filtername="lib";
    FStream options;
    String optionkey;
    String optionkeyvalue;
    String version=PACKAGE_VERSION;

    if ((namelength == 13) &&
        !strncmp(key->name,"loadable-name",13) &&
        ((String)value).length() &&
        (((String)value)[0] != '/')) {
      filtername+=value;
      filtername+="-filter.so";
      if (!filterpath.expand_filename(filtername)) {
          return make_err(no_such_filter,"add_filter",value);
      }
      RET_ON_ERR(config->replace("loadable-name",filtername));
    }
    return no_err;
  }

}
