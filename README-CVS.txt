$Id: README-CVS.txt,v 1.19 2002/07/24 14:09:47 mast Exp $


HOW TO BUILD PIKE FROM CVS

If you like to live at the bleeding edge you can download pike from CVS
with all the latest additions from the developers.  There are two
major branches in the archive, the latest stable branch and latest
development branch.  Stable versions have an even minor version number,
i.e. 7.0.x, 7.2.x, 7.4.x, whereas the development branches have an odd
minor version.

Keep in mind that the cvs versions are under heavy development and
have not been tested nearly as well as the offcial releases.  You use
the code at YOUR OWN RISK.

There are a few simple steps to get pike from cvs:

1. Get a recent version of cvs.

2. Login to the cvs-server:

         cvs -d :pserver:anon@cvs.roxen.com:/cvs login

   Just hit enter on the password prompt.

3. Check out the source:

         cvs -z3 -d :pserver:anon@cvs.roxen.com:/cvs co Pike/7.3

   substitute 7.3 for whatever version you want to get.

Building Pike from cvs, in addition to the requirements of a normal
build, requires a working Pike.  You will also need autoconf, gnu m4,
bison and gcc (to generate the dependency files; another compiler can
be used to do the actual compilation).

The top-level makefile (in this directory, not the src directory) has
all the magic you need to build pike directly from CVS.  Just type
'make'.  It is preferable to build from the toplevel since it avoids
contaminating the source tree with object files and other generated
files.

Other interesting make targets are:

install             Compile and install in default location.
install_interactive Interactive install.
tinstall            Test install, i.e. install in build directory.
verify              Do a test install and run the testsuite with the
                    installed pike.
just_verify         Run the testsuite directly with the pike binary in
                    the build tree.
run_hilfe           Run hilfe without installing pike.
pike                Build only the pike core, do not recurse into the
                    module directories.
documentation       Build the reference documentation from the
                    source.  See the refdoc subdirectory.
depend              Build the files that tracks dependencies between
                    the source files.  This is necessary to ensure
                    correct rebuilding if some of the source files
                    change, but not if you only intend to use the
                    build tree once.  It's not run by default.  See
                    also the note below about this target.
source              Prepare the source tree for compiliation without
                    the need for a preexisting installed pike.
force_autoconfig    Forces a build of the configure scripts.  This is
                    useful e.g. if a new module directory is added in
                    the CVS.
force_configure     Forces configure to be run (recursively).  If
                    you've installed a new library and want pike to
                    detect it, then the simplest way is to remove
                    config.cache in the build directory (or perhaps
                    just delete the relevant variables in it) and then
                    use this target.
dump_modules        Dumps the Pike modules directly in the build tree.
                    That makes pike load faster if it's run directly
                    from there, e.g. through the bin/pike script (see
                    below).  These dumped modules are not used for
                    anything else.  After this has been run once, any
                    changed Pike modules will be redumped
                    automatically by the main build targets.
undump_modules      Removes any modules dumped by dump_modules, and
                    removes the redump step described above.
force_dump_modules  Forces all Pike modules to be redumped, not just
                    those whose source files have changed.
snapshot            Create a snapshot export tarball.
export              Create a source dist and bump up the build number
                    (if you have cvs write access).  Please do not
                    check in the generated files.
clean               Removes all the built binary files.
cvsclean            Removes all files that are generated automatically,
                    i.e. brings the tree back to the state as if it
                    was checked out from the CVS.

If you want to pass arguments to the configure script (see below), the
simplest way is to use the CONFIGUREARGS variable, like this:

    make CONFIGUREARGS="--prefix=/usr/local/my-pike --with-security"

The arguments passed through CONFIGUREARGS are remembered in the build
tree and reused if CONFIGUREARGS is undefined or the empty string.
You therefore don't need to repeat them every time, but you can still
change them later if you like.  There's a special case for the --help
argument: If CONFIGUREARGS is set to '--help' then the help text from
the configure script is shown and nothing else is done, and the stored
CONFIGUREARGS setting isn't affected.

The build targets also creates a script 'pike' in the bin subdirectory
which runs the built Pike directly without installing it first.  If
you want to use Pike this way (which is mainly useful if you update
from CVS often), you should consider doing 'make dump_modules' to make
it start faster.

Note about the depend target: Dependencies are typically between *.c
files and the *.h files they include.  Since many *.c files are
generated from other input, the depend target often trigs other
targets to generate them.  Some of those targets use the pike binary
in the build tree, so if you do make depend before pike is first built
in a new build tree, you're likely to end up building everything.
That can seem like a bit of catch 22, but since the dependency files
are only needed to correctly rebuild in an old tree, it's perfectly ok
to wait with make depend until you have built Pike once.


CONFIGURE OPTIONS AND BUILD VARIABLES

Some options for the configure script are:

--prefix=/foo/bar         if you want to install Pike in /foo/bar,
                          default is /usr/local.
--without-gdbm            compile without gdbm support
--without-bignums         disable support for large integers
--without-gmp             compile without gmp support (implies
                          --without-bignums)
--with-rtldebug           compile with runtime debug checks
--without-cdebug          compile without debug symbols (-g)
--with-debug              same as --with-rtldebug --with-cdebug
--without-debug           same as --without-rtldebug --without-cdebug
--without-copt            compile without -O2
--without-threads         compile without threads support (see
                          also the section 'If It Doesn't Work' below)
--without-zlib            compile without gzip compression libary
                          support
--without-dynamic-modules compile statically, no dynamic loading
                          used (makes the binary larger)
--without-mysql           compile without mysql support
--with-profiling          enables profiling pike code but slows
                          down interpreter a little
--with-poll               use poll instead of select
--with-dmalloc            compile with memory tracking, makes pike
                          very slow, use for debugging only.
--with-security           enable internal object security system

You might also want to set the following environment variables:

CFLAGS     Put extra flags for your C compiler here.
CPPFLAGS   Put extra flags for your C preprocessor here
           (such as -I/usr/gnu/include)
LDFLAGS    Put extra flags to your linker here, such as
           -L/usr/gnu/lib and -R/usr/gnu/lib


MANUAL BUILDING

Instructions if you want to do the build more manually:

1. cd src ; ./run_autoconfig
   This creates configure files and Makefile.in files.

2. Create a build directory an cd to it.  Do NOT build in the source
   dir, doing so will make it impossible to do 'make export' later.

3. Run the newly created configure file located in the src dir from
   the build dir.  Make sure to use an absolute path! This creates the
   Makefiles you need, e.g. Makefile from Makefile.in and machine.h
   from machine.h.in.  If you don't use an absolute path the debug
   information will be all warped...

4. Run 'make depend'
   This updates all the Makefile.in files in the source dir to contain
   the correct dependencies.

5. If needed, edit config.h and Makefile to suit your purposes.  We
   have tried to make it so that you don't have to change config.h or
   Makefile at all.  If you need to do what you consider 'unnecessary
   changes' then mail us and we'll try to fit it into configure.  If
   possible, use gnu make, gcc, gnu sed and bison.

6. Run 'make'
   This builds pike.

7. Optionally, run 'make verify' to check that the compiled driver
   works as it should (might be a good idea).  This will take a little
   time and use quite a lot of memory, because the test program is
   quite large.  If everything works out fine no extra messages are
   written.

8. If you want to install Pike, type 'make install'.

After doing this, DO NOT commit the generated files.  They are placed
in .cvsignore files so you shouldn't have to bother with them.


IF IT DOESN'T WORK

 o Try again.

 o Your sh might be too buggy to run ./configure (this is the case on
   A/UX).  Try using bash, zsh or possibly ksh.  To use bash, first
   run /bin/sh and type:

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


BUGS

If you find a bug in the interpreter, typically if Pike dumps core,
the first thing to do is to make sure it is compiled with the
--with-rtldebug configure flag.  If not, reconfigure and recompile
with that and see if you get another error.  When you've done this,
please report the bug to us at http://community.roxen.com/crunch/ and
include as much as you can muster of the following:

  o The Pike version.  (Try pike --version or look in src/version.h)
  o What kind of system hardware/software you use (OS, compiler, etc.)
  o The piece of code that crashes or bugs, preferably in a very
    small pike-script with the bug isolated.  Please send a complete
    running example of something that causes the bug.
  o A description of what it is that bugs and when.
  o If you know how, then also give us a backtrace and dump of vital
    variables at the point of crash.
  o Or, if you found the error and corrected it, just send us the
    bugfix along with a description of what you did and why.
