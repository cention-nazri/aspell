
#include "settings.h"

#include "lock.hpp"

#if ENABLE_NLS

static acommon::Mutex lock;

static bool did_init = false;

extern "C" void aspell_gettext_init()
{
  {
    acommon::Lock l(&lock);
    if (did_init) return;
    did_init = true;
  }
  bindtextdomain("aspell", LOCALEDIR);
}

#endif
