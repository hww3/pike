#! /bin/sh

# This file scripts the xenofarm actions and creates a result package
# to send back.

log() {
  echo $1 | tee -a build/xenofarm/mainlog.txt
}

log_start() {
  log "BEGIN $1"
  date >> build/xenofarm/mainlog.txt
}

log_end() {
  LASTERR=$1
  if [ "$1" = "0" ] ; then
    log "PASS"
  else
    log "FAIL"
  fi
  date >> build/xenofarm/mainlog.txt
}

xenofarm_build() {
  log_start compile
  $MAKE $MAKE_FLAGS > build/xenofarm/compilelog.txt 2>&1
  log_end $?
  [ $LASTERR = 0 ] || return 1
}

xenofarm_post_build() {
  log_start verify
  $MAKE $MAKE_FLAGS METATARGET=verify TESTARGS="-a -T" > \
    build/xenofarm/verifylog.txt 2>&1
  log_end $?
  [ $LASTERR = 0 ] || return 1
  
  log_start export
  $MAKE $MAKE_FLAGS bin_export > build/xenofarm/exportlog.txt 2>&1
  log_end $?
  [ $LASTERR = 0 ] || return 1
}


# main code

LC_CTYPE=C
export LC_CTYPE
log "FORMAT 2"

log_start build
xenofarm_build
log_end $?

if [ $LASTERR = 0 ]; then
  log_start post_build
  xenofarm_post_build
  log_end $?
  else :
fi

log_start response_assembly
  cp "$BUILDDIR/config.info" build/xenofarm/configinfo.txt
  if test ! -f "build/xenofarm/verifylog.txt"; then
    cp "$BUILDDIR/config.cache" build/xenofarm/configcache.txt; \
    for f in `find $BUILDDIR -name config.log -print`; do
      cp $f build/xenofarm/configlog`echo $f|tr '[/]' '[_]'`.txt;\
    done;
  fi
  if test ! -f "build/xenofarm/exportlog.txt"; then
    cp "$BUILDDIR/testsuite" build/xenofarm/testsuite.txt;
  fi
  find . -name "core" -exec \
    gdb --batch --nx --command=bin/xenofarm_gdb_cmd "$BUILDDIR/pike" {} >> \
       build/xenofarm/_core.txt ";"
  find . -name "*.core" -exec \
    gdb --batch --nx --command=bin/xenofarm_gdb_cmd "$BUILDDIR/pike" {} >> \
      build/xenofarm/_core.txt ";"
  find . -name "core.*" -exec \
    gdb --batch --nx --command=bin/xenofarm_gdb_cmd "$BUILDDIR/pike" {} >> \
      build/xenofarm/_core.txt ";"
  cp "$BUILDDIR/dumpmodule.log" build/xenofarm/dumplog.txt
  cp buildid.txt build/xenofarm/
log_end $?

log "END"
