#
# $Id: Makefile,v 1.59 2002/03/11 17:55:25 mast Exp $
#
# Meta Makefile
#

VPATH=.
MAKE=make
OS=`uname -s -r -m|sed \"s/ /-/g\"|tr \"[A-Z]\" \"[a-z]\"|tr \"/\" \"_\"`
BUILDDIR=build/$(OS)
METATARGET=

# Use this to pass arguments to configure. Leave empty to keep previous args.
CONFIGUREARGS=

# Set to a flag for parallelizing make, e.g. -j2. It's given to make
# at the level where it's most effective.
MAKE_PARALLEL=

# Used to avoid make compatibility problems.
BIN_TRUE=":"

MAKE_FLAGS="MAKE=$(MAKE)" "CONFIGUREARGS=$(CONFIGUREARGS)" "BUILDDIR=$(BUILDDIR)"

all: bin/pike compile
	-@$(BIN_TRUE)

force:
	-@$(BIN_TRUE)

src/configure:
	cd src && ./run_autoconfig . 2>&1 | grep -v warning

force_autoconfig:
	cd src && ./run_autoconfig . 2>&1 | grep -v warning

force_configure:
	-rm -f "$(BUILDDIR)/Makefile"
	@$(MAKE) $(MAKE_FLAGS) configure

builddir:
	@builddir="$(BUILDDIR)"; \
	IFS='/'; \
	dir=`echo "$$builddir" | sed -e 's|[^/].*||'`; \
	for d in $$builddir; do \
	  dir="$$dir$$d"; \
	  test x"$$dir" = x -o -d "$$dir" || mkdir "$$dir" || exit 1; \
	  dir="$$dir/"; \
	done; \
	cd "$$builddir"

configure: src/configure builddir
	@builddir="$(BUILDDIR)"; \
	srcdir=`pwd`/src; \
	cd "$$builddir" && { \
	  if test "x$(CONFIGUREARGS)" = x; then :; else \
	    configureargs="$(CONFIGUREARGS)"; \
	    oldconfigureargs="$(CONFIGUREARGS)"; \
	  fi; \
	  if test -f .configureargs; then \
	    oldconfigureargs="`cat .configureargs`"; \
	  else :; fi; \
	  if test "x$(CONFIGUREARGS)" = x; then \
	    configureargs="$$oldconfigureargs"; \
	  else :; fi; \
	  MAKE=$(MAKE) ; export MAKE ;\
	  echo; \
	  echo Configure arguments: $$configureargs; \
	  echo 'Use `make CONFIGUREARGS="..."' "...'" 'to change them.'; \
	  echo 'They will be retained in the build directory.'; \
	  echo; \
	  if test -f Makefile -a -f config.status -a -f .configureargs -a \
		  "x$$oldconfigureargs" = "x$$configureargs"; then :; \
	  else \
	    echo Running $$srcdir/configure $$configureargs in $$builddir; \
	    if [ -f /bin/bash  ] ; then CONFIG_SHELL=/bin/bash ;  fi ;\
	    CONFIG_SITE=x $${CONFIG_SHELL-/bin/sh} \
	      "$$srcdir"/configure $$configureargs || exit $$?; \
	    echo "$$configureargs" > .configureargs; \
	    if test "x$$oldconfigureargs" = "x$$configureargs"; then :; \
	    else \
	      echo Configure arguments have changed - doing make clean; \
	      $(MAKE) "MAKE=$(MAKE)" clean || exit $$?; \
	      if test "x$(METATARGET)" = "xsource"; then :; \
	      elif test "x$(METATARGET)" = "xexport"; then :; \
	      else \
		echo Configure arguments have changed - doing make depend; \
		$(MAKE) "MAKE=$(MAKE)" depend || exit $$?; \
	      fi; \
	    fi; \
	  fi; \
	} || exit $$?

compile: configure
	@builddir="$(BUILDDIR)"; \
	cd "$$builddir" && { \
	  metatarget="$(METATARGET)"; \
	  if test "x$(LIMITED_TARGETS)" = "x"; then \
	    if test -f master.pike -a -x pike; then :; \
	    elif test "x$$metatarget" = xpike; then :; \
	    else metatarget="all $$metatarget"; fi; \
	    if test "x$$metatarget" = x; then metatarget=all; else :; fi; \
	  else :; fi; \
	  for target in $$metatarget; do \
	    echo Making $$target in "$$builddir"; \
	    rm -f remake; \
	    $(MAKE) "MAKE=$(MAKE)" "MAKE_PARALLEL=$(MAKE_PARALLEL)" $$target || { \
	      res=$$?; \
	      if test -f remake; then \
		$(MAKE) "MAKE=$(MAKE)" "MAKE_PARALLEL=$(MAKE_PARALLEL)" $$target || \
		  exit $$?; \
	      else \
		exit $$res; \
	      fi; \
	    }; \
	  done; \
	} || exit $$?

lobotomize_crypto:
	@echo "Removing Pike crypto modules."
	rm -f lib/modules/Crypto/_rsa.pike
	rm -f lib/modules/Crypto/aes.pike
	rm -f lib/modules/Crypto/des3.pike
	rm -f lib/modules/Crypto/des3_cbc.pike
	rm -f lib/modules/Crypto/idea_cbc.pike
	rm -f lib/modules/Crypto/rsa.pike
# Delete crypto dependent modules?
# rm -rf lib/modules/SSL.pmod
# rm -f lib/modules/Standards.pmod/PKCS.pmod/RSA.pmod
# rm -f lib/modules/Tools.pmod/X509.pmod
	(cd src/modules/_Crypto && ./.build_lobotomized_crypto);
	@echo
	@echo "Removing low Crypto module."
	rm -rf src/modules/_Crypto

# FIXME: The refdoc stuff ought to use $(BUILDDIR) too.

documentation:
	@$(MAKE) $(MAKE_FLAGS) "METATARGET=documentation"

doc: documentation

bin/pike: force
	@builddir='$(BUILDDIR)'; \
	case $$builddir in /*) ;; *) builddir="`pwd`/$$builddir";; esac; \
	sed -e "s@\"BUILDDIR\"@$$builddir@" < bin/pike.in > bin/pike
	@chmod a+x bin/pike

# This skips the modules.
pike: bin/pike
	@$(MAKE) $(MAKE_FLAGS) "METATARGET=pike"

install:
	@$(MAKE) $(MAKE_FLAGS) "METATARGET=install"

install_interactive:
	@$(MAKE) $(MAKE_FLAGS) "METATARGET=install_interactive"

tinstall:
	@$(MAKE) $(MAKE_FLAGS) "METATARGET=tinstall"

just_verify:
	@$(MAKE) $(MAKE_FLAGS) "METATARGET=just_verify"

verify:
	@$(MAKE) $(MAKE_FLAGS) "METATARGET=verify"

verify_installed:
	@$(MAKE) $(MAKE_FLAGS) "METATARGET=verify_installed"

check: verify
	-@$(BIN_TRUE)

sure: verify
	-@$(BIN_TRUE)

verbose_verify:
	@$(MAKE) $(MAKE_FLAGS) "METATARGET=verbose_verify"

gdb_verify:
	@$(MAKE) $(MAKE_FLAGS) "METATARGET=gdb_verify"

dump_modules:
	@$(MAKE) $(MAKE_FLAGS) "METATARGET=dump_modules"

force_dump_modules:
	@$(MAKE) $(MAKE_FLAGS) "METATARGET=force_dump_modules"

run_hilfe:
	@$(MAKE) $(MAKE_FLAGS) "METATARGET=run_hilfe"

source:
	@$(MAKE) "MAKE=$(MAKE)" "CONFIGUREARGS=--disable-binary $(CONFIGUREARGS)" \
	  "OS=source" "LIMITED_TARGETS=yes" "METATARGET=source" compile

export:
	@$(MAKE) "MAKE=$(MAKE)" "CONFIGUREARGS=--disable-binary $(CONFIGUREARGS)" \
	  "OS=source" "LIMITED_TARGETS=yes" "METATARGET=export" compile


autobuild_export:
	@$(MAKE) "MAKE=$(MAKE)" "CONFIGUREARGS=--disable-binary $(CONFIGUREARGS)" \
	  "OS=source" "LIMITED_TARGETS=yes" "METATARGET=small_export" compile

bin_export:
	@$(MAKE) $(MAKE_FLAGS) "METATARGET=bin_export"

feature_list:
	@$(MAKE) $(MAKE_FLAGS) "METATARGET=feature_list"

autobuild:
	@test -d build || mkdir build
	@rm -rf build/autobuild || /bin/true
	@mkdir build/autobuild
	@$(MAKE) $(MAKE_FLAGS) autobuild_low || /bin/true
	@echo Begin response assembly | tee -a build/autobuild/autobuildlog.txt
	@date >> build/autobuild/autobuildlog.txt
# AIX (hactar) FIXME: Better recognition needed.
#	@if test "$OS" = "aix-3-005370164c00" ; then \
#	  grep -v "-fpic ignored for AIX" makelog.txt | grep -v "-fPIC ignored for AIX" | grep -v "0711-327" > tmp ; \
#	  mv tmp makelog.txt ; \
#	else : ; \
#	fi;
	@cp "$(BUILDDIR)/config.info" build/autobuild/configinfo.txt || /bin/true
# Add all config.log and config.cache?
	@cp "$(BUILDDIR)/testsuite" build/autobuild/testsuite.txt || /bin/true
	@cp "$(BUILDDIR)/dumpmodule.log" build/autobuild/dumplog.txt || /bin/true
	@tar -c build/autobuild/*.txt > autobuild_result.tar
	@gzip -f9 autobuild_result.tar

autobuild_low:
	@echo Begin build | tee -a build/autobuild/autobuildlog.txt
	@date >> build/autobuild/autobuildlog.txt
	@$(MAKE) $(MAKE_FLAGS) TERM=dumb > build/autobuild/makelog.txt 2>&1
	@echo Begin verify | tee -a build/autobuild/autobuildlog.txt
	@date >> build/autobuild/autobuildlog.txt
	@$(MAKE) $(MAKE_FLAGS) METATARGET=verify TERM=dumb TESTARGS="-a -q" > build/autobuild/verifylog.txt 2>&1
	@echo Begin export | tee -a build/autobuild/autobuildlog.txt
	@date >> build/autobuild/autobuildlog.txt
	@$(MAKE) $(MAKE_FLAGS) TERM=dumb export > build/autobuild/exportlog.txt 2>&1

clean:
	-cd "$(BUILDDIR)" && test -f Makefile && $(MAKE) "MAKE=$(MAKE)" clean || { \
	  res=$$?; \
	  if test -f remake; then $(MAKE) "MAKE=$(MAKE)" clean; \
	  else exit $$res; fi; \
	} || exit $$?
	if test -f "refdoc/Makefile"; then \
	  cd refdoc; $(MAKE) "MAKE=$(MAKE)" clean; \
	else :; fi

spotless:
	-cd "$(BUILDDIR)" && test -f Makefile && $(MAKE) "MAKE=$(MAKE)" spotless || { \
	  res=$$?; \
	  if test -f remake; then $(MAKE) "MAKE=$(MAKE)" spotless; \
	  else exit $$res; fi; \
	} || exit $$?
	if test -f "refdoc/Makefile"; then \
	  cd refdoc; $(MAKE) "MAKE=$(MAKE)" spotless; \
	else :; fi

delete_builddir:
	-rm -rf "$(BUILDDIR)"

distclean: delete_builddir
	$(MAKE) "OS=source" delete_builddir
	-rm -rf bin/pike

srcclean:
	for d in `find src -type d -print`; do \
	  if test -f "$$d/.cvsignore"; then \
	    (cd "$$d" && rm -f `cat ".cvsignore"`); \
	  else :; fi; \
	done

cvsclean: srcclean distclean

depend: configure
	-@cd "$(BUILDDIR)" && \
	$(MAKE) "MAKE=$(MAKE)" "MAKE_PARALLEL=$(MAKE_PARALLEL)" depend || { \
	  res=$$?; \
	  if test -f remake; then $(MAKE) "MAKE=$(MAKE)" "MAKE_PARALLEL=$(MAKE_PARALLEL)" depend; \
	  else exit $$res; fi; \
	} || exit $$?

pikefun_TAGS:
	cd src && etags -l none -r \
	'/[ 	]*\(PMOD_PROTO \|PMOD_EXPORT \|static \|extern \)*void[ 	]\{1,\}f_\([a-zA-Z0-9_]*\)[ 	]*([ 	]*INT32/\2/' \
	`find . -type f -name '*.[ch]' -print`
	cd lib/modules && etags -l none -r \
	'/[ \t]*\(\<\(public\|inline\|final\|static\|protected\|local\|optional\|private\|nomask\|variant\)\>[ \t]\{1,\}\)*\(\<class\>\)[ \t]\{1,\}\<\([a-zA-Z�-�_][a-zA-Z�-�_0-9]*\)\>/\4/' \
	-r '/[ \t]*\(\<\(mixed\|float\|int\|program\|string\|function\|function(.*)\|array\|array(.*)\|mapping\|mapping(.*)\|multiset\|multiset(.*)\|object\|object(.*)\|void\|constant\|class\)\>)*\|\<\([A-Z�-��-�][a-zA-Z�-�_0-9]*\)\>\)[ \t]\{1,\}\(\<\([_a-zA-Z�-�][_a-zA-Z�-�0-9]*\)\>\|``?\(!=\|->=?\|<[<=]\|==\|>[=>]\|\[\]=?\|()\|[%-!^&+*<>|~\/]\)\)[ \t]*(/\4/' \
	-r '/#[ \t]*define[ \t]+\([_a-zA-Z]+\)(?/\1/' \
	`find . -type f '(' -name '*.pmod' -o -name '*.pike' ')' -print`
