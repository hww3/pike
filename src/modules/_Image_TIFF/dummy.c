/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id$
*/

/*
 * Glue needed on WIN32 if tiff.lib needs the symbol _vfprintf.
 *
 * Henrik Grubbstr�m 2008-11-03
 */

#include "global.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

int _Image_TIFF_dummy_dum_dum(FILE *fp, const char *fmt, va_list ap)
{
  return vfprintf(fp, fmt, ap);
}
