
#include "settings.h"

#include <stdio.h>

#if ENABLE_NLS

struct GettextInit
{
  GettextInit() {bindtextdomain(PACKAGE, LOCALEDIR);}
};

static GettextInit dummy;

#endif
