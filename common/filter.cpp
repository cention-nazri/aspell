/* This file is part of The New Aspell
 * Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */


#include "config.hpp"
#include "filter.hpp"
#include "speller.hpp"
#include "indiv_filter.hpp"
#include "copy_ptr-t.hpp"
#include "strtonum.hpp"
#include "errors.hpp"
#undef HAVE_LIBDL // FIXME
#ifdef HAVE_LIBDL
#include <dlfcn.h>
#endif
#include <stdio.h>
#include <cstdio>
#define DEBUG {fprintf(stderr,"File: %s(%i)\n",__FILE__,__LINE__);}

namespace acommon {

  Filter::Filter() {}

  void Filter::add_filter(IndividualFilter * filter,void* handles, int type)
  {
    switch (type) {
      case DECODER: {
        Filters::iterator cur, end;
        Handled::iterator handle;
        cur = decoders_.begin();
        end = decoders_.end();
        handle=handledDecoder.begin();
        while ((cur != end) && (filter->order_num() > (*cur)->order_num())){
          ++cur;
          ++handle;
        }
        decoders_.insert(cur, filter);
        handledDecoder.insert(handle,handles);
        break;
      }
      case FILTER: {
        Filters::iterator cur, end;
        Handled::iterator handle;
        cur = filters_.begin();
        end = filters_.end();
        handle=handledFilter.begin();
        while ((cur != end) && (filter->order_num() > (*cur)->order_num())){
          ++cur;
          ++handle;
        }
        filters_.insert(cur, filter);
        handledFilter.insert(handle,handles);
        break;
      }
      case ENCODER: {
        Filters::iterator cur, end;
        Handled::iterator handle;
        cur = encoders_.begin();
        end = encoders_.end();
        handle=handledEncoder.begin();
        while ((cur != end) && (filter->order_num() > (*cur)->order_num())){
          ++cur;
          ++handle;
        }
        encoders_.insert(cur, filter);
        handledEncoder.insert(handle,handles);
        break;
      }
      default: {
/* NOTE: if Aspell has to use this default case, than any of the programmers in
 * charge of maintaining Aspell or at least some lines of code have made some
 * mistake when calling Filter::addFilter function.
 */
        delete filter;
#ifdef HAVE_LIBDL
        if(*handles!=NULL){
          dlclose(*handles);
        }
#endif
      }
    }
  }

  void Filter::reset()
  {
    Filters::iterator cur, end;
    cur = decoders_.begin();
    end = decoders_.end();
    for (; cur != end; ++cur)
      (*cur)->reset();
    cur = filters_.begin();
    end = filters_.end();
    for (; cur != end; ++cur)
      (*cur)->reset();
    cur = encoders_.begin();
    end = encoders_.end();
    for (; cur != end; ++cur)
      (*cur)->reset();
  }

  void Filter::decode(FilterChar * & start, FilterChar * & stop){
    Filters::iterator cur, end;
    cur = decoders_.begin();
    end = decoders_.end();

    for (; cur != end; ++cur)
      (*cur)->process(start, stop);
  }

  void Filter::process(FilterChar * & start, FilterChar * & stop)
  {
    Filters::iterator cur, end;
    cur = filters_.begin();
    end = filters_.end();
    for (; cur != end; ++cur)
      (*cur)->process(start, stop);
  }

  void Filter::encode(FilterChar * & start, FilterChar * & stop){
    Filters::iterator cur, end;
    cur = encoders_.begin();
    end = encoders_.end();
    for (; cur != end; ++cur)
      (*cur)->process(start, stop);
  }

  void Filter::clear()
  {
    Filters::iterator cur, end;
    Handled::iterator handle;
    cur = decoders_.begin();
    end = decoders_.end();
    handle=handledDecoder.begin();
    for (; cur != end; ++cur){
      delete *cur;
#ifdef HAVE_LIBDL
      if(*handle!=NULL){
        dlclose(*handle);
      }
#endif
    }
    decoders_.clear();
    handledDecoder.clear();
    cur = filters_.begin();
    end = filters_.end();
    handle=handledFilter.begin();
    for (; cur != end; ++cur){
      delete *cur;
#ifdef HAVE_LIBDL
      if(*handle!=NULL){
        dlclose(*handle);
      }
#endif
    }
    filters_.clear();
    handledFilter.clear();
    cur = encoders_.begin();
    end = encoders_.end();
    handle=handledEncoder.begin();
    for (; cur != end; ++cur){
      delete *cur;
#ifdef HAVE_LIBDL
      if(*handle!=NULL){
        dlclose(*handle);
      }
#endif
    }
    encoders_.clear();
    handledEncoder.clear();
  }

  Filter::~Filter() 
  {
    clear();
  }

  PosibErr<bool> verifyVersion(const char * relOp, const char * actual, 
                                const char * required,const char * module) {
    //FIXME translate following if into assertion 
    if ( actual == NULL || required == NULL ) {
      return false;
    }
    char * actVers = (char *) actual;
    char * reqVers = (char *) required;
    while ( * actVers != '\0' || * reqVers != '\0'  ) {

      char * nextActVers = actVers;
      char * nextReqVers = reqVers;
      int actNum = strtoi_c(actVers,&nextActVers);
      int reqNum = strtoi_c(reqVers,&nextReqVers);


      if ( nextReqVers ==  reqVers) {
        while ( *nextReqVers == 'x' || *nextReqVers == 'X' ) {
          nextReqVers++;
        }
        if ( reqVers == nextReqVers && reqVers != '\0') {
          return make_err(bad_version_string);
        }
        else if ( reqVers != '\0' ) {
          reqNum = actNum;
        }
      }
      if (    ( nextActVers == actVers )
           && ( actVers != '\0' ) ) {
        return make_err(bad_version_string);
      }
      if ( relOp != NULL && relOp[0] == '>' && actVers != '\0' && 
           ( reqVers == '\0' || actNum > reqNum ) ) {
        return true;
      }
      if ( relOp != NULL && relOp[0] == '<' && reqVers != '\0' &&
           ( actVers == '\0' || actNum < reqNum ) ) {
        return true;
      }
      if ( actNum == reqNum ) {
        actVers = nextActVers;
        reqVers = nextReqVers;
        while ( *actVers == '.' ) {
          actVers++;
        }
        while ( *reqVers == '.' ) {
          reqVers++;
        }
        continue;
      }
      if ( relOp != NULL && relOp[0] == '!' ) {
        return true;
      }
      return false;
    }
    if ( relOp != NULL && relOp[0] != '\0' &&
         ( relOp[0] == '!'  || 
           ( relOp[1] != '=' && relOp[0] != '=' ) ) ) {
      return false;
    }
    return true;
  }

}

