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
#include "strtonum.hpp"
#include <stdio.h>

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

  // FIXME moved to new_config and modified 
  // filter modes
  /* 

  const char * filter_modes = "none,url,email,sgml,tex";
  
  */

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
//FIXME shouldn't this set handle to NULL ??? and make the below FIXME obsolete??
//      hm test later !!!
      handle = NULL;
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
//FIXME only true for first filter but not for multiple filters 
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
      // existance of libdl.
      // In case libdl is not available a filter is only available if made
      // one of the standard filters. This is done by statically linking
      // the filter sources.
      // On systems providing libdl or in case libtool mimics libdl 
      // The following code parts assure that all filters needed and requested
      // by user are loaded properly or be reported to be missing.
      // 
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
    conf->set_filter_modules(filter_modules_begin, filter_modules_end);
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


  FilterOptionExpandNotifier::~FilterOptionExpandNotifier(void) 
  {
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
          if (filter_modules_begin[countextended].desc != NULL) {
            free((char*)filter_modules_begin[countextended].desc);
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
        config->set_filter_modules(filter_modules_begin, filter_modules_end);
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
    String option_name = "";
    String filter_name = "lib";
    FStream options;
    String option_value;
    String version = PACKAGE_VERSION;
    KeyInfo * begin = NULL;
    KeyInfo * cur_opt = NULL;
    int optsize = 0;
    ConfigModule * mbegin = NULL;
    int modsize = filter_modules_end-filter_modules_begin;
    StringList filt_path;
    StringList opt_path;
    char line_number[9]="0";
    int active_option = 0;
    String expand = "filter-";
    String buf; DataPair d;

    if ((name_len == 6) &&
        !strncmp(key->name,"filter",6)){
      while (current < filter_modules_end) {
        if (!strncmp(value.str(), current->name,
                     value.size() <= strlen(current->name) 
		     ? value.size()
		     : strlen(current->name))) 
          return no_err;
        current++;
      }

      String filter_description = "";

      if (current >= filter_modules_end) {
        option_name += value;
        option_name += "-filter.opt";
        filter_name += value;
        filter_name += "-filter.so";
        if (!filter_path.expand_filename(filter_name)) {
          return make_err(no_such_filter, "add-filter", value);
        }
        if (!option_path.expand_filename(option_name)) {
          return make_err(no_options,"add_filter",option_name,"Options missing");
        }
        if (config->have(value)) {
//FEATURE ? rescale priority instead and continue ???
          fprintf(stderr,"warning: specifying filter twice makes no sense\n");
          return no_err;
        }
        RET_ON_ERR(options.open(option_name,"r"));

        while (getdata_pair(options,d,buf))
	{
	  to_lower(d.key);
	  option_value = d.value;
          sprintf(line_number,"%i",d.line_num);

	  //
	  // key == aspell
	  //
          if (d.key == "aspell") 
	  {
            if ( d.value == NULL || *(d.value) == '\0' ) {
              return make_err(confusing_version,"add_filter",option_name,line_number);
            }

            char * requirement = d.value.str;
            char * relop = requirement;
            char swap = '\0';
            
            if ( *requirement == '>' || *requirement == '<' || 
                 *requirement == '!' ) {
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
              return make_err(confusing_version,"add_filter",option_name,line_number);
            }
	    if ( peb == false ) {
              peb.ignore_err();
              return make_err(bad_version,"add_filter",option_name,line_number); 
            }
            continue;
	  } 
	  
	  //
	  // key == option
	  //
	  if (d.key == "option" ) {
            expand  = "filter-";
            expand += option_value;
            if (config->have(expand)) {
              if (begin != NULL) {
                release_options(begin,begin+optsize);
                free(begin);
              }
              option_value.insert(0,"(filter-)", 9);
              return make_err(identical_option,"add_filter",option_name,line_number);
            }

            KeyInfo * expandopt = NULL;

            if (    (    ( begin == NULL )
                      && (     (expandopt = (KeyInfo *)malloc(sizeof(KeyInfo) * (optsize + 1)))
                           == NULL ) )
                 || (    (expandopt = (KeyInfo *) realloc(begin,sizeof(KeyInfo) * (optsize + 1)))
                      == NULL ) ) {
              if ( begin != NULL ) {
                release_options(begin,begin + optsize);
                free(begin);
              }
              return make_err(cant_extend_options,"add_filter",value);
            }
            begin = expandopt;

            // let cur_opt point to newly generated last element 
            // ( begin + optsize ) and increment optsize after to indicate
            // actual number of options 
            // ( !!! postfix increment !!! returns value before increment !!! )
            cur_opt = begin + optsize;
            optsize++;
            
	    char * n = (char *)malloc(7 + value.size() + 1 + option_value.size() + 1);

	    cur_opt->name = n;
	    memcpy(n, "filter-", 7);              n += 7;
	    memcpy(n, value.str(), value.size()); n += value.size();
	    *n = '-'; ++n;
	    memcpy(n, option_value.c_str(), option_value.size() + 1);
	    cur_opt->type = KeyInfoBool;
            cur_opt->def  = NULL;
            cur_opt->desc = NULL;
            cur_opt->flags = 0;
            cur_opt->other_data = 0;
            active_option = 1;
            continue;
          }

	  //
	  // key == static
	  //
          if (d.key == "static") {
            active_option = 0;
            continue;
          }

	  //
	  // key == description
	  //
          if ((d.key == "desc") ||
              (d.key == "description")) {

            //
            // filter description
            // 
            if (!active_option) {
              filter_description = option_value;
            }

            //
            //option description
            //
            else {

              //avoid memory leak;
              if ( cur_opt->desc != NULL) {
                free((char *)cur_opt->desc);
                cur_opt->desc = NULL;
              }
              cur_opt->desc = strdup(option_value.c_str());
            }
            continue;
          }
	  
	  //
	  // key = endfile
	  //
          if (d.key == "endfile") {
            break;
          }

	  //
	  // !active_option
	  //
          if (!active_option) {
            if (begin != NULL) {
              free(begin);
            }
            return make_err(options_only,"add_filter",option_name,line_number);
          }

	  //
	  // key == type
	  //
          if (d.key == "type") {
	    to_lower(d.value); // This is safe since normally option_value is used
            if (d.value == "list") 
	      cur_opt->type = KeyInfoList;
            else if (d.value == "int" || d.value == "integer") 
              cur_opt->type = KeyInfoInt;
            else if (d.value == "string")
              cur_opt->type = KeyInfoString;
	    //FIXME why not force user to ommit type specifier or explicitly say bool ???
	    else
	      cur_opt->type = KeyInfoBool;
            continue;
          }

	  //
	  // key == default
	  //
          if (d.key == "def" || d.key == "default") {
            
            if ( cur_opt->type == KeyInfoList && cur_opt->def != NULL) {
              option_value += ",";
              option_value += cur_opt->def;
              free((void*)cur_opt->def);
            }

            // FIXME
            //may try some syntax checking
            //if ( cur_opt->type == KeyInfoBool ) {
            //  check for valid bool values true false 0 1 on off ...
            //  and issue error if wrong or assume false ??
            //}
            //if ( cur_opt->type == KeyInfoInt ) {
            //  check for valid integer or double and issue error if not
            //}
            if ( ( (cur_opt->def)=strdup(option_value.c_str()) ) == NULL ) {
              if (begin != NULL) {
                release_options(begin,begin+optsize);
                free(begin);
              }
              return make_err(cant_extend_options,"add_filter",value);
            }
            continue;
          }

          
 	  //
 	  // key == flags
 	  //
          if (d.key == "flags") {
            if (d.value == "utf-8" || d.value == "UTF-8")
              cur_opt->flags = KEYINFO_UTF8;
            continue;
          }
           
	  //
	  // key == endoption
	  //
          if (d.key=="endoption") {
            active_option = 0;
            continue;
          }

	  // 
	  // error
	  // 
          if (begin != NULL) {
            release_options(begin,begin+optsize);
            free(begin);
          }
          return make_err(invalid_option_modifier,"add_filter",option_name,line_number);
        
	} // end while getdata_pair_c
     
        if ( filter_modules_begin == filter_modules ) {
          //avoid memory leaks
          if ( (mbegin = (ConfigModule*)malloc(modsize*sizeof(ConfigModule))) == NULL ) {     
            if ( begin != NULL ) {
              release_options(begin,begin+optsize);
              free(begin);
            }
            return make_err(cant_extend_options,"add_filter",value);
          }
          memcpy(mbegin,filter_modules_begin,(modsize)*sizeof(ConfigModule));
          filter_modules_begin = mbegin;
          filter_modules_end   = mbegin + modsize;
          config->set_filter_modules(filter_modules_begin,filter_modules_end);
        }

        //avoid memory leaks
        if ( (mbegin = (ConfigModule*)realloc((ConfigModule*)filter_modules_begin,
                                              ++modsize*sizeof(ConfigModule))) == NULL) {     
          if (begin != NULL) {
            release_options(begin,begin+optsize);
            free(begin);
          }
          return make_err(cant_extend_options,"add_filter",value);
        }
        mbegin[modsize-1].name  = strdup(value);
        mbegin[modsize-1].load  = strdup(filter_name.c_str());
        mbegin[modsize-1].desc  = strdup(filter_description.c_str());
        mbegin[modsize-1].begin = begin;
        mbegin[modsize-1].end   = begin+optsize;
        filter_modules_begin = mbegin;
        filter_modules_end   = mbegin+modsize;
        config->set_filter_modules(filter_modules_begin,filter_modules_end);
        return no_err;
      }// end if (current >= filter_modules_end) 
      return make_err(no_such_filter,"add_filter",value);
    }// end if "filter"
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
}
