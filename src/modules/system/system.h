/*
 * $Id: system.h,v 1.5 2001/02/03 23:08:41 mirar Exp $
 *
 * Prototypes for the Pike system-module
 *
 * Henrik Grubbstr�m 1997-01-28
 */

#ifndef PIKE_MODULES_SYSTEM_H
#define PIKE_MODULES_SYSTEM_H

/*
 * Includes
 */

#include "global.h"
#include "pike_types.h"

/*
 * Prototypes
 */

/*
 * passwords.c
 */
void f_getgrnam(INT32 args);
void f_getgrgid(INT32 args);
void f_getpwnam(INT32 args);
void f_getpwuid(INT32 args);
void f_setpwent(INT32 args);
void f_endpwent(INT32 args);
void f_getpwent(INT32 args);
void f_setgrent(INT32 args);
void f_endgrent(INT32 args);
void f_getgrent(INT32 args);

/*
 * syslog.c
 */
void f_openlog(INT32 args);
void f_syslog(INT32 args);
void f_closelog(INT32 args);

#endif /* PIKE_MODULES_SYSTEM_H */

/*
 * memory.c
 */

struct memory_storage
{
   unsigned char *p;
   size_t size;
   
#define MEM_READ        0x01
#define MEM_WRITE       0x02
#define MEM_FREE_FREE   0x10
#define MEM_FREE_MUNMAP 0x20   
   unsigned long flags;
};

