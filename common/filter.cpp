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
#ifdef HAVE_LIBDL
#include <dlfcn>
#endif
#include <stdio.h>
#include <cstdio>

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
/* NOTE: if aspell has to use this default case, than any of the programmers in
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

}

