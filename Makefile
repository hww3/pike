#
# $Id: Makefile,v 1.46 2001/12/02 17:44:42 mast Exp $
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
	  if test -f .configureargs; then \
	    oldconfigureargs="`cat .configureargs`"; \
	  else :; fi; \
	  if test "x$(CONFIGUREARGS)" = x; then \
	    configureargs="$$oldconfigureargs"; \
	  else \
	    configureargs="$(CONFIGUREARGS)"; \
	  fi; \
	  echo; \
	  MAKE=$(MAKE) ; export MAKE ;\
	  echo Configure arguments: $$configureargs; \
	  echo; \
	  if test -f Makefile -a -f config.status -a -f .configureargs -a \
		  "x$$oldconfigureargs" = "x$$configureargs"; then :; \
	  else \
	    echo Running $$srcdir/configure $$configureargs in $$builddir; \
	    if [ -f /bin/bash  ] ; then CONFIG_SHELL=/bin/bash ;  fi ;\
	    (CONFIG_SITE=x $${CONFIG_SHELL-/bin/sh} \
	      "$$srcdir"/configure $$configureargs || \
	      exit $$?) && \
	      echo "$$configureargs" > .configureargs; \
	    if test "x$$oldconfigureargs" = "x$$configureargs"; then :; \
	    else \
	      echo Configure arguments have changed - doing make clean; \
	      $(MAKE) "MAKE=$(MAKE)" clean; \
	    fi; \
	  fi; \
	}

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
	}

documentation:
	@test -f "$(BUILDDIR)/pike" || $(MAKE) $(MAKE_FLAGS) compile
	@cd "$(BUILDDIR)" && $(MAKE) $(MAKE_FLAGS) documentation

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

run_hilfe:
	@$(MAKE) $(MAKE_FLAGS) "METATARGET=run_hilfe"

source:
	@$(MAKE) "MAKE=$(MAKE)" "CONFIGUREARGS=--disable-binary" "OS=source" \
	  "RUNPIKE=$(RUNPIKE)" "LIMITED_TARGETS=yes" "METATARGET=source" compile

export:
	@$(MAKE) "MAKE=$(MAKE)" "CONFIGUREARGS=--disable-binary" "OS=source" \
	  "RUNPIKE=$(RUNPIKE)" "LIMITED_TARGETS=yes" "METATARGET=export" compile

bin_export:
	@$(MAKE) $(MAKE_FLAGS) "METATARGET=bin_export"

feature_list:
	@$(MAKE) $(MAKE_FLAGS) "METATARGET=feature_list"

clean:
	-cd "$(BUILDDIR)" && test -f Makefile && $(MAKE) "MAKE=$(MAKE)" clean || { \
	  res=$$?; \
	  if test -f remake; then $(MAKE) "MAKE=$(MAKE)" clean; \
	  else exit $$res; fi; \
	}

spotless:
	-cd "$(BUILDDIR)" && test -f Makefile && $(MAKE) "MAKE=$(MAKE)" spotless || { \
	  res=$$?; \
	  if test -f remake; then $(MAKE) "MAKE=$(MAKE)" spotless; \
	  else exit $$res; fi; \
	}

delete_builddir:
	-rm -rf "$(BUILDDIR)"

distclean: delete_builddir
	$(MAKE) "OS=source" delete_builddir
	-rm -rf bin/pike

cvsclean: distclean
	for d in `find src -type d -print`; do \
	  if test -f "$$d/.cvsignore"; then \
	    (cd "$$d" && rm -f `cat ".cvsignore"`); \
	  else :; fi; \
	done

depend: configure
	-@cd "$(BUILDDIR)" && \
	$(MAKE) "MAKE=$(MAKE)" "MAKE_PARALLEL=$(MAKE_PARALLEL)" depend || { \
	  res=$$?; \
	  if test -f remake; then $(MAKE) "MAKE=$(MAKE)" "MAKE_PARALLEL=$(MAKE_PARALLEL)" depend; \
	  else exit $$res; fi; \
	}

pikefun_TAGS:
	cd src && etags -l none -r \
	'/[ 	]*\(PMOD_PROTO \|PMOD_EXPORT \|static \|extern \)*void[ 	]\{1,\}f_\([a-zA-Z0-9_]*\)[ 	]*([ 	]*INT32/\2/' \
	`find . -type f -name '*.[ch]' -print`
	cd lib/modules && etags -l none -r \
	'/[ \t]*\(\<\(public\|inline\|final\|static\|protected\|local\|optional\|private\|nomask\|variant\)\>[ \t]\{1,\}\)*\(\<class\>\)[ \t]\{1,\}\<\([a-zA-Z�-�_][a-zA-Z�-�_0-9]*\)\>/\4/' \
	-r '/[ \t]*\(\<\(mixed\|float\|int\|program\|string\|function\|function(.*)\|array\|array(.*)\|mapping\|mapping(.*)\|multiset\|multiset(.*)\|object\|object(.*)\|void\|constant\|class\)\>)*\|\<\([A-Z�-��-�][a-zA-Z�-�_0-9]*\)\>\)[ \t]\{1,\}\(\<\([_a-zA-Z�-�][_a-zA-Z�-�0-9]*\)\>\|``?\(!=\|->=?\|<[<=]\|==\|>[=>]\|\[\]=?\|()\|[%-!^&+*<>|~\/]\)\)[ \t]*(/\4/' \
	-r '/#[ \t]*define[ \t]+\([_a-zA-Z]+\)(?/\1/' \
	`find . -type f '(' -name '*.pmod' -o -name '*.pike' ')' -print`
