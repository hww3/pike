/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id: version.c,v 1.141 2003/09/02 12:55:47 mast Exp $
*/

#include "global.h"
#include "svalue.h"
#include "interpret.h"
#include "stralloc.h"
#include "version.h"

RCSID("$Id: version.c,v 1.141 2003/09/02 12:55:47 mast Exp $");

/*! @decl string version()
 *!
 *! Report the version of Pike. Does the same as
 *! @code
 *! sprintf("Pike v%d.%d release %d", __REAL_VERSION__,
 *!         __REAL_MINOR__, __REAL_BUILD__);
 *! @endcode
 *!
 *! @seealso
 *!   @[__VERSION__], @[__MINOR__], @[__BUILD__],
 *!   @[__REAL_VERSION__], @[__REAL_MINOR__], @[__REAL_BUILD__],
 */
void f_version(INT32 args)
{
  pop_n_elems(args);
  push_constant_text ("Pike v"
		      DEFINETOSTR (PIKE_MAJOR_VERSION)
		      "."
		      DEFINETOSTR (PIKE_MINOR_VERSION)
		      " release "
		      DEFINETOSTR (PIKE_BUILD_VERSION));
}
