dnl $Id: aclocal.m4,v 1.36 2002/01/27 01:48:42 mast Exp $

dnl Some compatibility with Autoconf 2.50+. Not complete.
dnl newer autoconf call substr m4_substr
ifdef([substr], ,m4_copy(m4_substr,substr))


pushdef([AC_PROG_CC_WORKS],
[
  popdef([AC_PROG_CC_WORKS])
  if test "x$enable_binary" != "xno"; then
    if test "${ac_prog_cc_works_this_run-}" != "yes" ; then
      AC_PROG_CC_WORKS
      ac_prog_cc_works_this_run="${ac_cv_prog_cc_works-no}"
      export ac_prog_cc_works_this_run
    else
      AC_MSG_CHECKING([whether the C compiler ($CC $CFLAGS $LDFLAGS) works])
      AC_MSG_RESULT([(cached) yes])
    fi
  fi
])

pushdef([AC_PROG_CC],
[
  popdef([AC_PROG_CC])

  AC_PROG_CC

  AC_MSG_CHECKING([if we are using TCC])
  AC_CACHE_VAL(pike_cv_prog_tcc, [
    case "`$CC -V 2>&1|head -1`" in
      tcc*)
        pike_cv_prog_tcc="yes"
      ;;
      *) pike_cv_prog_tcc="no" ;;
    esac
  ])
  if test "x$pike_cv_prog_tcc" = "xyes"; then
    AC_MSG_RESULT(yes)
    TCC="yes"
    if echo "$CC $CFLAGS $CPPFLAGS" | grep " -Y" >/dev/null; then :; else
      # We want to use the system API's...
      CPPFLAGS="-Ysystem $CPPFLAGS"
    fi
  else
    AC_MSG_RESULT(no)
    TCC=no
  fi
])

dnl option, descr, with, without, default
define([MY_AC_ARG_WITH], [
  AC_ARG_WITH([$1], [$2], [
    if test "x$withval" = "xno"; then
      ifelse([$4], , :, [$4])
    else
      ifelse([$3], , :, [$3])
    fi
  ], [$5])
])

dnl flag, descr
define([MY_DESCR],
       [  substr([$1][                                  ],0,33) [$2]])

define([MY_AC_PROG_CC],
[
  define(ac_cv_prog_CC,pike_cv_prog_CC)
  AC_PROG_CC
  undefine([ac_cv_prog_CC])
  AC_PROG_CPP
  if test "x$enable_binary" = "no"; then
    # Do the check above even when --disable-binary is used, since we
    # need a real $CPP, and AC_PROG_CPP wants AC_PROG_CC to be called
    # earlier.
    CC="$BINDIR/nobinary_dummy cc"
  fi
])

pushdef([AC_CONFIG_HEADER],
[
  CONFIG_HEADERS="$1"
  popdef([AC_CONFIG_HEADER])
  AC_CONFIG_HEADER($1)
])

define([ORIG_AC_CHECK_SIZEOF], defn([AC_CHECK_SIZEOF]))
pushdef([AC_CHECK_SIZEOF],
[
  if test "x$cross_compiling" = "xyes"; then
    changequote(<<, >>)dnl
    define(<<AC_CV_NAME>>, translit(ac_cv_sizeof_$1, [ *], [_p]))dnl
    changequote([, ])dnl
    AC_MSG_CHECKING(size of $1 ... crosscompiling)
    AC_CACHE_VAL(AC_CV_NAME,[
      cat > conftest.$ac_ext <<EOF
dnl This sometimes fails to find confdefs.h, for some reason.
dnl [#]line __oline__ "[$]0"
[#]line __oline__ "configure"
#include "confdefs.h"
#include <stdio.h>
char size_info[[]] = {
  0, 'S', 'i', 'Z', 'e', '_', 'I', 'n', 'F', 'o', '_',
  '0' + sizeof([$1]), 0
};
EOF
      if AC_TRY_EVAL(ac_compile); then
        if test -f "conftest.$ac_objext"; then
	  AC_CV_NAME=`strings "conftest.$ac_objext" | sed -e '/^SiZe_InFo_[[0-9]]$/s/SiZe_InFo_//p' -ed | head -1`
          if test "x$AC_CV_NAME" = "x"; then
	    AC_MSG_WARN([Magic cookie not found.])
	    AC_CV_NAME=ifelse([$2], , 0, [$2])
	  else :; fi
        else
	  AC_MSG_WARN([Object file not found.])
	  AC_CV_NAME=ifelse([$2], , 0, [$2])
        fi
      else
        AC_CV_NAME=0
      fi
      rm -rf conftest*
    ])    
    AC_MSG_RESULT($AC_CV_NAME)
    undefine([AC_CV_NAME])dnl
  elif test "x$enable_binary" = "xno"; then
    translit(ac_cv_sizeof_$1, [ *], [_p])=$2
  fi
  ORIG_AC_CHECK_SIZEOF($1,$2)
])

define([ORIG_CHECK_HEADERS], defn([AC_CHECK_HEADERS]))
pushdef([AC_CHECK_HEADERS],
[
  if test "x$enable_binary" != "xno"; then
    ORIG_CHECK_HEADERS($1,$2,$3)
  else
    for ac_hdr in $1
    do
      ac_safe=`echo "$ac_hdr" | sed 'y%./+-%__p_%'`
      eval "ac_cv_header_$ac_safe=yes"
    done
  fi
])

AC_DEFUN(AC_MY_CHECK_TYPE,
[
AC_MSG_CHECKING([for $1])
AC_CACHE_VAL(ac_cv_type_$1,
[
AC_TRY_COMPILE([
#include <sys/types.h>

#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif

$3

],[
$1 tmp;
],ac_cv_type_$1=yes,ac_cv_type_$1=no)
])

if test $ac_cv_type_$1 = no; then
  AC_DEFINE($1,$2)
  AC_MSG_RESULT(no)
else
  AC_MSG_RESULT(yes)
fi
])


AC_DEFUN(AC_TRY_ASSEMBLE,
[ac_c_ext=$ac_ext
 ac_ext=${ac_s_ext-s}
 cat > conftest.$ac_ext <<EOF
	.file "configure"
[$1]
EOF
if AC_TRY_EVAL(ac_compile); then
  ac_ext=$ac_c_ext
  ifelse([$2], , :, [  $2
  rm -rf conftest*])
else
  echo "configure: failed program was:" >&AC_FD_CC
  cat conftest.$ac_ext >&AC_FD_CC
  ac_ext=$ac_c_ext
ifelse([$3], , , [  rm -rf conftest*
  $3
])dnl
fi
rm -rf conftest*])



dnl 
dnl PIKE_FEATURE_CLEAR()
dnl PIKE_FEATURE(feature,text)
dnl

define(PIKE_FEATURE_CLEAR,[
  rm pike_*.feature 2>/dev/null
])

define(PIKE_FEATURE_RAW,[
  cat >pike_[$1].feature <<EOF
[$2]
EOF])

define([PAD_FEATURE],[substr([$1][................................],0,17) ])

define(PIKE_FEATURE_3,[
  cat >pike_[$1].feature <<EOF
PAD_FEATURE([$2])[$3]
EOF])

define(PIKE_FEATURE,[
  PIKE_FEATURE_3(translit([[$1]],[. ()],[____]),[$1],[$2])
])

define(PIKE_FEATURE_WITHOUT,[
  PIKE_FEATURE([$1],[no (forced without)])
])

define(PIKE_FEATURE_NODEP,[
  PIKE_FEATURE([$1],[no (dependencies failed)])
])

define(PIKE_FEATURE_OK,[
  PIKE_FEATURE([$1],[yes])
])


define([AC_LOW_MODULE_INIT],
[
# $Id: aclocal.m4,v 1.36 2002/01/27 01:48:42 mast Exp $

MY_AC_PROG_CC

AC_DEFINE(POSIX_SOURCE)

AC_SUBST(CONFIG_HEADERS)

AC_SUBST_FILE(dependencies)
dependencies=$srcdir/dependencies

AC_SUBST_FILE(dynamic_module_makefile)
AC_SUBST_FILE(static_module_makefile)

AC_ARG_WITH(root,   [  --with-root=path      specify a cross-compilation root-directory],[
  case "$with_root" in
    /)
      with_root=""
    ;;
    /*)
    ;;
    no)
      with_root=""
    ;;
    *)
      AC_MSG_WARN([Root path $with_root is not absolute. Ignored.])
      with_root=""
    ;;
  esac
],[with_root=""])

if test "x$enable_binary" = "xno"; then
  # Fix makefile rules as if we're cross compiling, to use pike
  # fallbacks etc. Do this without setting $ac_cv_prog_cc_cross to yes
  # since autoconf macros like AC_TRY_RUN will complain bitterly then.
  CROSS=yes
else
  CROSS="$ac_cv_prog_cc_cross"
  # newer autoconf
  if test x"$CROSS" = x; then
     CROSS="$cross_compiling"
  fi
fi

AC_SUBST(CROSS)

if test "x$enable_binary" = "xno"; then
  RUNPIKE="USE_PIKE"
  RUNTPIKE="USE_PIKE"
elif test "x$ac_cv_prog_cc_cross" = "xyes"; then
  RUNPIKE="DEFAULT_RUNPIKE"
  RUNTPIKE="USE_PIKE"
else
  RUNPIKE="DEFAULT_RUNPIKE"
  RUNTPIKE="USE_TPIKE"
fi
AC_SUBST(RUNPIKE)
AC_SUBST(RUNTPIKE)
])


define([AC_MODULE_INIT],
[
AC_LOW_MODULE_INIT()
PIKE_FEATURE_CLEAR()

ifdef([PIKE_INCLUDE_PATH],
[
dynamic_module_makefile=PIKE_INCLUDE_PATH/dynamic_module_makefile
static_module_makefile=PIKE_INCLUDE_PATH/dynamic_module_makefile
],[
  AC_MSG_CHECKING([for the Pike module base directory])

  counter=.

  uplevels=../
  while test ! -f "${uplevels}dynamic_module_makefile"
  do
    counter=.$counter
    if test $counter = .......... ; then
      AC_MSG_RESULT(failed)
      exit 1
    else
      :
    fi
    uplevels=../$uplevels
  done

  dynamic_module_makefile=${uplevels}dynamic_module_makefile
  static_module_makefile=${uplevels}static_module_makefile
  AC_MSG_RESULT(${uplevels}.)
])

])

pushdef([AC_OUTPUT],
[
  AC_SET_MAKE

  AC_SUBST(prefix)
  export prefix
  AC_SUBST(exec_prefix)
  export exec_prefix
  AC_SUBST(CC)
  export CC
  AC_SUBST(CPP)
  export CPP
  AC_SUBST(BINDIR)
  export BINDIR
  AC_SUBST(BUILDDIR)
  export BUILDDIR
  AC_SUBST(TMP_LIBDIR)
  export TMP_BUILDDIR
  AC_SUBST(TMP_BUILDDIR)
  export INSTALL
  AC_SUBST(INSTALL)
  export AR
  AC_SUBST(AR)
  export CFLAGS
  AC_SUBST(CFLAGS)
  export CPPFLAGS
  AC_SUBST(CPPFLAGS)
  export OPTIMIZE
  AC_SUBST(OPTIMIZE)
  export WARN
  AC_SUBST(WARN)
  export CCSHARED
  AC_SUBST(CCSHARED)

  PMOD_TARGETS=
  for f in $srcdir/*.cmod; do
    PMOD_TARGETS="$PMOD_TARGETS $f"
  done
  PMOD_TARGETS=`echo $srcdir/*.cmod | sed -e "s/\.cmod/\.c/g" | sed -e "s|$srcdir/|\\$(SRCDIR)/|g"`
  AC_SUBST(PMOD_TARGETS)

ifdef([PIKE_INCLUDE_PATH],
[
  make_variables_in=PIKE_INCLUDE_PATH/make_variables_in
],[
  AC_MSG_CHECKING([for the Pike base directory])

  make_variables_in=make_variables.in

  counter=.

  uplevels=
  while test ! -f "$srcdir/$uplevels$make_variables_in"
  do
    counter=.$counter
    if test $counter = .......... ; then
      AC_MSG_RESULT(failed)
      exit 1
    else
      :
    fi
    uplevels=../$uplevels
  done

  make_variables_in=$uplevels$make_variables_in
  AC_MSG_RESULT(${uplevels}.)
])

  AC_SUBST_FILE(make_variables)
  make_variables=make_variables

popdef([AC_OUTPUT])
AC_OUTPUT(make_variables:$make_variables_in $][1,$][2,$][3)
])
dnl
dnl
dnl

define(MY_CHECK_FUNCTION,[
  AC_MSG_CHECKING(for working $1)
  AC_CACHE_VAL(pike_cv_func_$1,[
    AC_TRY_RUN([
$2
int main() {
$3;
return 0;
}
], pike_cv_func_$1=yes, pike_cv_func_$1=no, [
      echo $ac_n "crosscompiling... $ac_c" 1>&6
      AC_TRY_LINK([$2], [$3], pike_cv_func_$1=yes, pike_cv_func_$1=no)
    ])
  ])
  AC_MSG_RESULT([$]pike_cv_func_$1)
  if test [$]pike_cv_func_$1 = yes; then
    AC_DEFINE(translit(HAVE_$1,[a-z],[A-Z]))
  else :; fi
])

dnl These are like AC_PATH_PROG etc, but gives a path to
dnl nobinary_dummy when --disable-binary is used. That program will
dnl always return true and have ' ' as output.
define(MY_AC_CHECK_PROG,[
  if test "x$enable_binary" = "xno"; then
    AC_CHECK_PROG($1,nobinary_dummy,$3,$4,$BINDIR)
  else
    AC_CHECK_PROG($1,$2,$3,$4,$5,$6)
  fi
])
define(MY_AC_CHECK_PROGS,[
  if test "x$enable_binary" = "xno"; then
    AC_CHECK_PROGS($1,nobinary_dummy,$3,$BINDIR)
  else
    AC_CHECK_PROGS($1,$2,$3,$4)
  fi
])
define(MY_AC_PATH_PROG,[
  if test "x$enable_binary" = "xno"; then
    AC_PATH_PROG($1,nobinary_dummy,$3,$BINDIR)
  else
    AC_PATH_PROG($1,$2,$3,$4)
  fi
])
define(MY_AC_PATH_PROGS,[
  if test "x$enable_binary" = "xno"; then
    AC_PATH_PROGS($1,nobinary_dummy,$3,$BINDIR)
  else
    AC_PATH_PROGS($1,$2,$3,$4)
  fi
])
