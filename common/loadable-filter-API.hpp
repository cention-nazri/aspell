// This file is part of The New Aspell
// Copyright (C) 2002 by Christoph Hintermüller under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.
//
// Added by Christoph Hintermüller
// This file contains macros useful within filter classes
// These macros are required by mk-flt-opt.pl script
//
// Further the FILTER_API_EXPORTS and FILTER_API_IMPORTS macros needed
// for making filter usable in Windows too without changes 
//             
#ifndef LOADABLE_FILTER_API_HEADER
#define LOADABLE_FILTER_API_HEADER

#include <stdio.h>

#include "posib_err.hpp"
#include "config.hpp"
#include "indiv_filter.hpp"

#include "settings.h"

#if defined(__CYGWIN__) || defined (_WIN32)
#define FILTER_API_EXPORTS __declspec(dllexport)
#define FILTER_API_IMPORTS __declspec(dllimport)
#else
#define FILTER_API_EXPORTS
#define FILTER_API_IMPORTS
#endif

#ifdef FILTER_PROGRESS_CONTROL
static FILE * controllout=stderr;
#define FDEBUGCLOSE do {\
  if ((controllout != stdout) && (controllout != stderr)) {\
    fclose(controllout);\
    controllout=stderr;\
  } } while (false)

#define FDEBUGNOTOPEN do {\
  if ((controllout == stdout) || (controllout == stderr)) {\
    FDEBUGOPEN; \
  } } while (false)

#define FDEBUGOPEN do {\
  FDEBUGCLOSE; \
  if ((controllout=fopen(FILTER_PROGRESS_CONTROL,"w")) == NULL) {\
    controllout=stderr;\
  }\
  setbuf(controllout,NULL);\
  fprintf(controllout,"Debug Destination %s\n",FILTER_PROGRESS_CONTROL);\
  } while (false)

#define FDEBUG fprintf(controllout,"File: %s(%i)\n",__FILE__,__LINE__)
#define FDEBUGPRINTF(a) fprintf(controllout,a)
#endif // FILTER_PROGRESS_CONTROL

/* ACTIVATE_ENCODER, ACTIVATE_FILTER, ACTIVATE_DECODER:
 *          Before a encoding, decoding or processing filter can be
 *          loaded (if Aspell is built within a shared environment) and used
 *          by aspell filter interface it has to be activated.
 *          If a specific class is denoted as encoder, decoder or filter
 *          by one of the above macros use the following ones to activate
 *          the entire filter feature.
 *          
 *              extern "C" acommon::IndividualFilter * new_<feature>(void){
 *                return new <YourFilterClass>;
 *              }
 *
 *          <feature> stands for either encoder, filter or decoder;
 *          <YourFilterClass> is the name of the corresponding class.
 *          
 *
 *          In case Aspell is built on a system without shared objects and 
 *          dynamic linking or the  COMPILE_IN_FILTER macro is defined
 *          the filter is activated via the following line:
 *          
 *              extern "C" acommon::IndividualFilter * new_<filter>_<feature>(void){
 *                return new <YourFilterClass>;
 *              }
 *
 *          <filter> stands for the name of the filter
 *          
 *
 * Note: call this outside your filter class declaration. If your filter class
 *       does not contain to any specific namespace or these macros are called
 *       inside the same namespace as the class containing the entire feature, 
 *       nspace may be empty (not tested);
 */

#ifndef COMPILE_IN_FILTER

#define ACTIVATE_ENCODER(nspace,class_name,filter) \
extern "C" { \
  FILTER_API_EXPORTS acommon::IndividualFilter * new_encoder(void) {\
    return new nspace::class_name;\
  }\
}
#define ACTIVATE_FILTER(nspace,class_name,filter) \
extern "C" { \
  FILTER_API_EXPORTS acommon::IndividualFilter * new_filter(void) {\
    return new nspace::class_name;\
  }\
}
#define ACTIVATE_DECODER(nspace,class_name,filter) \
extern "C" { \
  FILTER_API_EXPORTS acommon::IndividualFilter * new_decoder(void) {\
    return new nspace::class_name;\
  }\
}

#else // COMPILE_IN_FILTER

#define ACTIVATE_ENCODER(nspace,class_name,filter) \
  FILTER_API_EXPORTS acommon::IndividualFilter * nspace::new_ ## filter ## _encoder(void) {\
    return new nspace::class_name;\
  }
#define ACTIVATE_FILTER(nspace,class_name,filter) \
  FILTER_API_EXPORTS acommon::IndividualFilter * nspace::new_ ## filter ## _filter(void) {\
    return new nspace::class_name;\
  }
#define ACTIVATE_DECODER(nspace,class_name,filter) \
  FILTER_API_EXPORTS acommon::IndividualFilter * nspace::new_ ## filter ## _decoder(void) {\
    return new nspace::class_name;\
  }

#endif // COMPILE_IN_FILTER

#endif
