/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id: time_stuff.h,v 1.12 2002/10/11 01:39:39 nilsson Exp $
*/

#ifndef TIME_STUFF_H
#define TIME_STUFF_H

#include "machine.h"

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  if HAVE_TIME_H
#   include <time.h>
#  endif
# endif
#endif

#ifdef HAVE_WINSOCK_H
# include <winsock.h>
#endif

#undef HAVE_SYS_TIME_H
#undef HAVE_TIME_H
#undef TIME_WITH_SYS_TIME

#define my_timercmp(tvp, cmp, uvp) \
  ( (tvp)->tv_sec == (uvp)->tv_sec ? \
    (tvp)->tv_usec cmp (uvp)->tv_usec : \
    (tvp)->tv_sec cmp (uvp)->tv_sec )

#define my_subtract_timeval(X, Y)		\
  do {						\
    struct timeval *_a=(X), *_b=(Y);		\
    _a->tv_sec -= _b->tv_sec;			\
    _a->tv_usec -= _b->tv_usec;			\
    if(_a->tv_usec < 0) {			\
      _a->tv_sec--;				\
      _a->tv_usec+=1000000;			\
    }						\
  } while(0)

#define my_add_timeval(X, Y)			\
  do {						\
    struct timeval *_a=(X), *_b=(Y);		\
    _a->tv_sec += _b->tv_sec;			\
    _a->tv_usec += _b->tv_usec;			\
    if(_a->tv_usec >= 1000000) {		\
      _a->tv_sec++;				\
      _a->tv_usec-=1000000;			\
    }						\
  } while(0)

#ifndef STRUCT_TIMEVAL_DECLARED
#define STRUCT_TIMEVAL_DECLARED
#endif

#ifndef HAVE_STRUCT_TIMEVAL
struct timeval
{
  long tv_sec;
  long tv_usec;
};
#endif


#endif
