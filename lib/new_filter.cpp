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

  static int filter_modules_referencing = 0;

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
    // The direct interface usually when new_filter ... functions are coded
    // manually
    FilterHandle & operator= (void * h) {
//FIXME only true for first filter but not for multible filters 
//      assert(handle == NULL);
      handle = h; return *this;
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
    FilterEntry dynamic_filter;
    int addcount = 0;
    ConfigModule * current_filter = NULL;

    filter.clear();
    while ((filter_name = els.next()) != 0) {
      filterhandle[0] = filterhandle[1] = filterhandle[2] = (void*)NULL;
      addcount = 0;
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

        for (current_filter = (ConfigModule*)filter_modules_begin+standard_filters_size;
	     current_filter < (ConfigModule*)filter_modules_end; 
	     current_filter++) 
	  {
	    if (strcmp(current_filter->name,filter_name) == 0) {
	      break;
          }
        }
        if (current_filter >= filter_modules_end) {
          return make_err(other_error);
        }
        if (((filterhandle[0] = dlopen(current_filter->load,RTLD_NOW)) == NULL) ||
            ((filterhandle[1] = dlopen(current_filter->load,RTLD_NOW)) == NULL) ||
            ((filterhandle[2] = dlopen(current_filter->load,RTLD_NOW)) == NULL)) {
          return make_err(cant_dlopen_file,"filter setup",filter_name,dlerror());
        }
        dynamic_filter.decoder = (FilterFun *)dlsym(filterhandle[0].val(),"new_decoder");
        dynamic_filter.encoder = (FilterFun *)dlsym(filterhandle[1].val(),"new_encoder");
        dynamic_filter.filter  = (FilterFun *)dlsym(filterhandle[2].val(),"new_filter");
        if (!dynamic_filter.decoder &&
	    !dynamic_filter.encoder &&
	    !dynamic_filter.filter) {
          return make_err(empty_filter,"filter setup",filter_name);
        }
        dynamic_filter.name = filter_name;
        f = &dynamic_filter;
      } else {
        addcount = 1;
      }
#else
      assert(f); //FIXME: Return Error Condition
#endif
      if (use_decoder && f->decoder && (ifilter = f->decoder())) {
        RET_ON_ERR_SET(ifilter->setup(config), bool, keep);
	if (!keep) {
	  ifilter.del();
	} else {
          filter.add_filter(ifilter.release(),filterhandle[0].release(),
                            Filter::DECODER);
        }
      } 
      if (use_filter && f->filter && (ifilter = f->filter())) {
        RET_ON_ERR_SET(ifilter->setup(config), bool, keep);
        if (!keep) {
          ifilter.del();
        } else {
          filter.add_filter(ifilter.release(), filterhandle[2].release(),
                            Filter::FILTER);
        }
      }
      if (use_encoder && f->encoder && (ifilter = f->encoder())) {
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
    PathBrowser option_path;
    PathBrowser filter_path;
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
  : option_path(),
    filter_path()
  {
    *this = brother;
    filter_modules_referencing++;
  }
  void  FilterOptionExpandNotifier::operator=(const FilterOptionExpandNotifier & brother) {
    option_path = brother.option_path;
    filter_path = brother.filter_path;
  }

  void FilterOptionExpandNotifier::release_options(const KeyInfo * begin,const KeyInfo * end) {
    KeyInfo * current = NULL;
    
    if (begin == NULL) {
      return;
    }
    for (current = (KeyInfo*)begin;current < end; current++) {
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
  : option_path(),
    filter_path(), 
    config(conf) 
  {
    filter_modules_referencing++;
    do {
      StringList test;
      config->retrieve_list("option-path",&test);
      option_path = test;
    } while (false);
    do {
      StringList test;
      config->retrieve_list("filter-path",&test);
      filter_path = test;
    } while (false);
  }

//  extern const size_t filter_modules_size;

  FilterOptionExpandNotifier::~FilterOptionExpandNotifier(void) {
    int countextended = filter_modules_size/sizeof(ConfigModule);

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
        filter_modules_begin = (ConfigModule*)&filter_modules[0];
        filter_modules_end = (ConfigModule*)filter_modules_begin+filter_modules_size/
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

  PosibErr<void> FilterOptionExpandNotifier::item_added(const KeyInfo * key, ParmString value) 
  {
    int name_len = strlen(key->name);
    ConfigModule * current = (ConfigModule*)filter_modules_begin;
    String option_name="";
    String filter_name="lib";
    FStream options;
    //String option_key;
    String option_value;
    String version = PACKAGE_VERSION;
    unsigned int option_start = 0;
    unsigned int option_count = 0;
    KeyInfo * begin = NULL;
    int optsize = 0;
    ConfigModule * mbegin = NULL;
    int modsize = filter_modules_end-filter_modules_begin;
    void * help = NULL;
    StringList filt_path;
    StringList opt_path;
    bool greater;
    bool equal;
    bool less;
    int line_count = 0;
    char line_number[9]="0";
    int active_option = 0;
    String expand="filter-";
    int norealoption = 0;
    char buf[256]; DataPair d;

    if ((name_len == 6) &&
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
        option_name += value;
        option_name += "-filter.opt";
        filter_name += value;
        filter_name += "-filter.so";
        if (!filter_path.expand_filename(filter_name)) {
          filter_name  = value;
          filter_name += ".flt";
          if (!filter_path.expand_filename(filter_name)) {
            return make_err(no_such_filter, "add-filter", value);
          }
          RET_ON_ERR(options.open(filter_name,"r"));

	  bool empty_file = true;

          while (getdata_pair(options,d,buf,256)) {
            if ((d.key == "add-filter" || d.key == "rem-filter") 
		&& value == value.str())
	    {
              fprintf(stderr,"warning: specifying filter twice makes no sense\n"
		      "\tignoring `%s %s'\n",
		      d.key.str(),d.value.str());
              continue;
            }
            empty_file = false;
            RET_ON_ERR(config->replace(d.key,d.value));
          }
          if (empty_file) {
            return make_err(empty_filter, "filter setup", filter_name);
          }
          config->replace("rem-filter",value);
          return no_err;
        }
        else if (!option_path.expand_filename(option_name)) {
          return make_err(no_options,"add_filter",option_name,"Options missing");

        }
        if (config->have(value)) {
          fprintf(stderr,"warning: specifying filter twice makes no sense\n");
          return no_err;
        }
        RET_ON_ERR(options.open(option_name,"r"));
        greater = equal = less = false;
        while (getdata_pair(options,d,buf,256)) {
	  to_lower(d.key);
	  unescape(d.value);
	  option_value = d.value;
          line_count++;
          if (d.key == "aspell") {
            option_start = 0;
            if ((option_value.size() > option_start) &&
                (option_value[option_start] == '>')) {
              greater = true;
              option_start++;
            }
            if ((option_value.size() > option_start) &&
                (option_value[option_start] == '<')) {
              less = true;
              option_start++;
            }
            if ((option_value.size() > option_start) &&
                (option_value[option_start] == '=')) {
              equal = true;
              option_start++;
            }
            if (option_start == 0) {
              equal = true;
            }
            if ((option_value.size() > option_start) &&
                !asc_isdigit(option_value[option_start])) {
              sprintf(line_number,"%i",line_count);
              return make_err(confusing_version,"add_filter",option_name,line_number);
            }
            option_value.erase(0,option_start);
            for (option_count = 0;(option_count < option_value.size()) &&
                               (option_count < version.size());
                 option_count++) {
              if (asc_isdigit(option_value[option_count]) &&
                  asc_isdigit(version[option_count])) {
                if (greater &&
                    ((option_value[option_count] < version[option_count]) ||
                     ((option_value[option_count] == version[option_count]) &&
                      (option_value.size()-1 == option_count) &&
                      (option_value.size() < version.size())))) {
                   break;
                }
                if (less &&
                    ((option_value[option_count] > version[option_count]) ||
                     ((option_value[option_count] == version[option_count]) &&
                      (version.size()-1 == option_count) &&
                      (option_value.size() > version.size())))) {
                  break;
                }
                if (option_value[option_count] == version[option_count]) {
                  if (equal &&
                      (version.size()-1 == option_count) &&
                      (option_value.size()-1 == option_count)) {
                    break;
                  }
                  else if ((version.size()-1 > option_count) &&
                           (option_value.size()-1 > option_count)) {
                    continue;
                  }
                }
                sprintf(line_number,"%i",line_count);
                return make_err(bad_version,"add-filter",option_name,line_number);
              }
              if (less &&
                  asc_isdigit(option_value[option_count]) &&
                  (version[option_count] == '.' ) &&
                  (version.size()-1 > option_count)) {
                break;
              }
              if (greater &&
                  asc_isdigit(version[option_count]) &&
                  (option_value[option_count] == '.') &&
                  (option_value.size()-1 > option_count)) {
                break;
              }
              if ((version[option_count] == '.') &&
                  (option_value[option_count] == '.') &&
                  (version.size()-1 > option_count) &&
                  (option_value.size()-1 > option_count)) {
                continue;
              }
              sprintf(line_number,"%i",line_count);
              return make_err(confusing_version,"add_filter",option_name,line_number);
            }
            continue;
          }
          if ((d.key == "option") || 
              (!active_option && 
               ((d.key == "desc") ||
                (d.key == "description")) && 
               (norealoption = 1))) {
            if (!norealoption && config->have(option_value.c_str())) {
              fprintf(stderr,"option %s: might conflict with Aspell option\n"
                             "try to prefix it by `filter-'\n",
                      option_value.c_str());
            }
            if ((!norealoption) && 
                (begin == NULL)) {
              if ((help = realloc(begin,(optsize+=1)*sizeof(KeyInfo))) == NULL) {
                if (begin != NULL) {
                  release_options(begin,begin+optsize-1);
                  free(begin);
                }
                return make_err(cant_extend_options,"add_filter",value);
              }
              begin = (KeyInfo*)help;
              begin[optsize-1].name = begin[optsize-1].def = begin[optsize-1].desc = NULL;
              begin[optsize-1].type = KeyInfoDescript;
            }
            if (!norealoption || (begin == NULL)) {
              if ((help = realloc(begin,(optsize+=1)*sizeof(KeyInfo)) ) == NULL) {
                if (begin != NULL) {
                  release_options(begin,begin+optsize-1);
                  free(begin);
                }
                return make_err(cant_extend_options,"add_filter",value);
              }
              begin = (KeyInfo*)help;
              begin[optsize-1].name = begin[optsize-1].def = begin[optsize-1].desc = NULL;
            }
            if (norealoption && (begin != NULL)) {
              begin[0].type = KeyInfoDescript;
              begin[0].def  = NULL;
              if (begin[0].desc != NULL) {
                free((char*)begin[0].desc);
                begin[0].desc = NULL;
              }
              if (option_value.size() == 0) {
                option_value="-";
              }
              if ((begin[0].desc = strdup(option_value.c_str())) == NULL ) {
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
              expand+=option_value;
              if (config->have(expand.c_str())) {
                if (begin != NULL) {
                  release_options(begin,begin+optsize);
                  free(begin);
                }
                option_value.insert(0,"(filter-)");
                sprintf(line_number,"%i",line_count);
                return make_err(identical_option,"add_filter",option_name,line_number);
              }
              if (((begin[optsize-1].name)=strdup(option_value.c_str())) == NULL) {
                if (begin !=NULL) {
                  release_options(begin,begin+optsize);
                  free(begin);
                }
                return make_err(cant_extend_options,"add_filter",value);
              }
              begin[optsize-1].type = KeyInfoBool;
              begin[optsize-1].def  = NULL;
              begin[optsize-1].desc = NULL;
              begin[optsize-1].otherdata[0]='\0';
              active_option = 1;
            }
            norealoption = 0;
            continue;
          }
          if (d.key == "static") {
            //fprintf(stderr,"Filter %s consists of `%s'\n",value.str(),
            //        option_value.c_str());
            active_option = 0;
            continue;
          }
          if (!active_option) {
            if (begin != NULL) {
              free(begin);
            }
            sprintf(line_number,"%i",line_count);
            return make_err(options_only,"add_filter",option_name,line_number);
          }
          if (d.key == "type") {
	    to_lower(d.value); // This is safe since normally option_value is used
            if (d.value == "list") {
              begin[optsize-1].type = KeyInfoList;
              continue;
            }
            if ((d.value == "int") ||
                (d.value == "integer")) {
              begin[optsize-1].type = KeyInfoInt;
              continue;
            }
            if (d.value == "string") {
              begin[optsize-1].type = KeyInfoString;
              continue;
            }
            begin[optsize-1].type = KeyInfoBool;
            continue;
          }
          if ((d.key == "def") ||
              (d.key == "default")) {
//Type detection ???
            if (begin[optsize-1].type == KeyInfoList) {
              if (begin[optsize-1].def != NULL) {
                option_value+=",";
                option_value+=begin[optsize-1].def;
                free((void*)begin[optsize-1].def);
              }
            }  
            if (((begin[optsize-1].def)=strdup(option_value.c_str()) ) == NULL) {
              if (begin != NULL) {
                release_options(begin,begin+optsize);
                free(begin);
              }
              return make_err(cant_extend_options,"add_filter",value);
            }
            continue;
          }
          if ((d.key == "desc") ||
              (d.key == "description")) {
            if (((begin[optsize-1].desc)=strdup(option_value.c_str())) == NULL) {
              if (begin != NULL) {
                release_options(begin,begin+optsize);
                free(begin);
              }
              return make_err(cant_extend_options,"add_filter",value);
            }
            continue;
          }
          if (d.key == "other") {
            strncpy(begin[optsize-1].otherdata,option_value.c_str(),15);
            begin[optsize-1].otherdata[15]='\0';
            continue;
          }
          if (d.key=="endoption") {
            active_option = 0;
            continue;
          }
          if (d.key == "endfile") {
            break;
          }
          if (begin != NULL) {
            release_options(begin,begin+optsize);
            free(begin);
          }
          sprintf(line_number,"%i",line_count);
          return make_err(invalid_option_modifier,"add_filter",option_name,line_number);
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
          if ((mbegin = (ConfigModule*)malloc(modsize*sizeof(ConfigModule))) == NULL) {     
            if (begin != NULL) {
              release_options(begin,begin+optsize);
              free(begin);
            }
            return make_err(cant_extend_options,"add_filter",value);
          }
          memcpy(mbegin,filter_modules_begin,(modsize)*sizeof(ConfigModule));
          filter_modules_begin = mbegin;
          filter_modules_end   = mbegin + modsize;
          config->set_modules(filter_modules_begin,filter_modules_end);
        }
        if ((mbegin = (ConfigModule*)realloc((ConfigModule*)filter_modules_begin,
                     ++modsize*sizeof(ConfigModule))) == NULL) {     
          if (begin != NULL) {
            release_options(begin,begin+optsize);
            free(begin);
          }
          return make_err(cant_extend_options,"add_filter",value);
        }
        mbegin[modsize-1].name  = strdup(value);
        mbegin[modsize-1].load  = strdup(filter_name.c_str());
        mbegin[modsize-1].begin = begin;
        mbegin[modsize-1].end   = begin+optsize;
        filter_modules_begin = mbegin;
        filter_modules_end   = mbegin+modsize;
        config->set_modules(filter_modules_begin,filter_modules_end);
        return no_err;
      }
      return make_err(no_such_filter,"add_filter",value);
    }
    else if ((name_len == 11) &&
             !strncmp(key->name,"filter-path",11)) {
      RET_ON_ERR(config->retrieve_list("filter-path",&filt_path));
      filter_path = filt_path;
    }
    else if ((name_len == 11) &&
             !strncmp(key->name,"option-path",15)) {
      RET_ON_ERR(config->retrieve_list("option-path",&opt_path));
      option_path = opt_path;
    }
    return no_err;
  }

  PosibErr<void> FilterOptionExpandNotifier::item_updated(const KeyInfo * key, ParmString value){
    int name_len = strlen(key->name);
    String option_name;
    String filter_name="lib";
    FStream options;
    String version = PACKAGE_VERSION;

    if ((name_len == 13) &&
        !strncmp(key->name,"loadable-name",13) &&
        !value.empty() &&
        value[0] != '/')
    {
      filter_name += value;
      filter_name += "-filter.so";
      if (!filter_path.expand_filename(filter_name)) {
          return make_err(no_such_filter,"add_filter",value);
      }
      RET_ON_ERR(config->replace("loadable-name",filter_name));
    }
    return no_err;
  }

}
