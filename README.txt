Pike by Fredrik H�binette 1994-2001

Permission to copy, modify, and distribute this source for any legal
purpose granted as long as my name is still attached to it.  More
specifically, the GPL, LGPL and MPL licenses apply to this software.

New releases can be found on ftp://pike.ida.liu.se/pub/pike/
Report bugs at http://community.roxen.com/crunch/
There is also a mailing list, to subscribe to it mail:
pike-request@roxen.com


HOW TO BUILD PIKE

The top-level makefile (in this directory, not the src directory) has
all the magic you need to build pike directly from CVS.  Just type
'make'.

You will need autoconf, automake, gnu m4, bison, a C compiler and the
GMP library.  You probably also want to use GNU make and libz.

If that doesn't work, or you like making things difficult for
yourself, try the Old instructions:

1) cd src ; ./run_autoconfig
   This creates configure files and Makefile.in files.

2) Create a build directory an cd to it.  Do NOT build in the source
   dir, doing so will make it impossible to do 'make export' later.

3) Run the newly created configure file located in the src dir from
   the build dir.  Make sure to use an absolute path! This creates the
   Makefiles you need, e.g. Makefile from Makefile.in and machine.h
   from machine.h.in.  If you don't use an absolute path the debug
   information will be all warped...

   Some options for ./configure are:
   --prefix=/foo/bar         if you want to install Pike in /foo/bar,
                             default is /usr/local
   --without-gdbm            compile without gdbm support
   --without-gmp             compile without gmp support
   --without-rtldebug        compile without runtime debugging
   --without-cdebug          compile without debug symbols (-g)
   --without-debug           compile without rtldbug and cdebug
   --without-threads         compile without threads support (See
                             below)
   --without-zlib            compile without gzip compression libary
                             support
   --without-dynamic-modules compile statically, no dynamic loading
                             used (makes binary larger)
   --without-mysql           compile without mysql support
   --with-profiling          enables profiling pike code but slows
                             down interpreter a little
   --with-poll               use poll instead of select
   --with-dmalloc            compile with memory tracking, makes pike
                             very slow, use for debugging only
   --without-copt            compile without -O2
   --without-bignums         disable support for large integers
   --with-security           enable internal object security system

   You might also want to set the following environment variables:
   CFLAGS     Put extra flags for your C compiler here.
   CPPFLAGS   Put extra flags for your C preprocessor here
              (such as -I/usr/gnu/include)
   LDFLAGS    Put extra flags to your linker here, such as
              -L/usr/gnu/lib  and -R/usr/gnu/lib

   Use the above flags to make sure the configure script finds the
   gdbm and gmp libraries and include files if you need or want those
   modules.  If the configure script doesn't find them, Pike will
   still compile, but without those modules.

4) Run 'make depend'
   This updates all the Makefile.in files in the source dir to contain
   the correct dependencies

5) If needed, edit config.h and Makefile to suit your purposes.  I've
   tried to make it so that you don't have to change config.h or
   Makefile at all.  If you need to do what you consider 'unnecessary
   changes' then mail us and we'll try to fit it into configure.  If
   possible, use gnu make, gcc, gnu sed and bison

6) Run 'make'
   This builds pike.

7) Optionally, run 'make verify' to check that the compiled driver
   works as it should (might be a good idea).  This will take a little
   time and use quite a lot of memory, because the test program is
   quite large.  If everything works out fine no extra messages are
   written.

8) If you want to install Pike, write 'make install'.  This will put
   your pike in <prefix>/pike/<version>/. This way, you can install
   many pike versions in parallell on the system if you want to.  To
   put it below <prefix> directly, as other packages usually do, run
   'make INSTALLARGS="--traditional" install' instead.

After doing this, DO NOT, commit the generated files.  They are placed
in .cvsignore files so you shouldn't have to bother with them.  Doing
'make export' will create a tar file with all the needed files, but
they should NOT be in the CVS archive!


IF IT DOESN'T WORK:

 o Try again.

 o Your sh might be too buggy to run ./configure. (This is the case on
   A/UX) Try using bash, zsh or possibly ksh.  To use bash, first run
   /bin/sh and
   type:
   $ CONFIG_SHELL=full_path_for_bash
   $ export CONFIG_SHELL
   $ $CONFIG_SHELL ./configure

 o If you are not using GNU make, compile in the source dir rather
   than using a separate build dir.

 o ./configure relies heavily on sed, if you have several sed in your
   path try another sed (preferably gnu sed).

 o configure might have done something wrong, check machine.h and
   report any errors back to us.

 o Your gmp/gdbm libraries might not be working or incorrectly
   installed; start over by running configure with the appropriate
   --without-xxx arguments.  Also note that threads might give
   problems with I/O and signals.  If so you need to run configure
   --without-threads.

 o Try a different compiler, malloc, compiler-compiler and/or make
   (if you have any other).


THREADS SUPPORT

Getting threads support might be hairy on some platforms, most
platforms have threads support but quite a few have problems running
external processes (through create_process).  By default threads
support is disabled on platforms where threading is known not to work
100% properly.

IRIX: Starting many processes causes a resource error which sometimes
      causes a complete hang and 100% cpu usage.

FreeBSD 3.x: Symptoms are similar to IRIX, but the problem has
      something to do with signal handling. (And as you may know,
      process handling uses signals on UNIX...)

Linux: Not all linux variations have 100% working threads, in fact
      most libc5 systems do not work very well with threads.  Threads
      support is enabled by default on Linux, but I recommend running
      'make verify' after compilation.  This will (hopefully) tell you
      if your threads are not working.


BUGS

If you find a bug in the interpreter, the first thing to do is to make
sure the interpreter is compiled with PIKE_DEBUG defined.  If not,
recompile with PIKE_DEBUG and see if you get another error.  When
you've done this, please report the bug to us at
http://community.roxen.com/crunch/ and include as much as you can
muster of the following:

  o The version of the driver. (Try pike --version or look in
    src/version.h)
  o What kind of system hardware/software you use (OS, compiler, etc.)
  o The piece of code that crashes or bugs, preferably in a very
    small pike-script with the bug isolated.  Please send a complete
    running example of something that makes the interpreter bug.
  o A description of what it is that bugs and when.
  o If you know how, then also give us a backtrace and dump of vital
    variables at the point of crash.
  o Or, if you found the error and corrected it, just send us the
    bugfix along with a description of what you did and why.
