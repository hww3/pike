/*
 * Sybase driver for the Pike programming language.
 *
 * By Francesco Chemolli <kinkie@roxen.com> 10/12/1999
 * (C) Roxen IS
 *
 */

#include "sybase_config.h"

#ifndef __PIKE_SYBASE_SYBASE_H
#define __PIKE_SYBASE_SYBASE_H
#ifdef HAVE_SYBASE

#include "threads.h"
#include <ctpublic.h>

#define SYBASE_DRIVER_VERSION "9"

typedef struct {
  CS_CONTEXT *context;
  CS_CONNECTION *connection;
  CS_COMMAND *cmd;
  char busy; /* only one pending command per connection */
  
  char had_error; /* non-zero if had error */
  char error[256]; /* The last error string. The size is determined by the */
                    /* sybase API */

  char **results;
  CS_INT *results_lengths;
  CS_SMALLINT *nulls;

  int numcols; /* the number of columns */

#ifdef _REENTRANT
  MUTEX_T lock;
#endif

} pike_sybase_connection;


#endif /* HAVE_SYBASE */
#endif /* __PIKE_SYBASE_SYBASE_H */
