AUTOMAKE_OPTIONS = foreign subdir-objects
SUBDIRS = . po manual
DIST_SUBDIRS = ${SUBDIRS} examples myspell

# These are needed due to a bug in Automake
pkgdatadir = @pkgdatadir@
pkglibdir = @pkglibdir@

filterdir = ${pkglibdir}
optdir = ${pkgdatadir}
localedir = ${datadir}/locale

export PERLPROG=@PERLPROG@

AM_CPPFLAGS = -I${srcdir}/gen -I${srcdir}/common \
              -I${srcdir}/interfaces/cc/ -I${srcdir}/modules/speller/default/

SUFFIXES = .opt

noinst_DATA = 
EXTRA_DIST = 

########################################################################
#
# libaspell and friends
#

lib_LTLIBRARIES = libaspell.la libpspell.la

libaspell_la_SOURCES =\
  common/cache.cpp\
  common/string.cpp\
  common/getdata.cpp\
  common/itemize.cpp\
  common/file_util.cpp\
  common/string_map.cpp\
  common/string_list.cpp\
  common/config.cpp\
  common/posib_err.cpp\
  common/errors.cpp\
  common/error.cpp\
  common/fstream.cpp\
  common/iostream.cpp\
  common/info.cpp\
  common/can_have_error.cpp\
  common/convert.cpp\
  common/tokenizer.cpp\
  common/speller.cpp\
  common/document_checker.cpp\
  common/filter.cpp\
  common/objstack.cpp \
  common/strtonum.cpp\
  common/directory.cpp\
  common/gettext_init.cpp\
  modules/speller/default/readonly_ws.cpp\
  modules/speller/default/suggest.cpp\
  modules/speller/default/data.cpp\
  modules/speller/default/multi_ws.cpp\
  modules/speller/default/phonetic.cpp\
  modules/speller/default/writable.cpp\
  modules/speller/default/file_data_util.cpp\
  modules/speller/default/speller_impl.cpp\
  modules/speller/default/phonet.cpp\
  modules/speller/default/typo_editdist.cpp\
  modules/speller/default/editdist.cpp\
  modules/speller/default/primes.cpp\
  modules/speller/default/language.cpp\
  modules/speller/default/leditdist.cpp\
  modules/speller/default/affix.cpp\
  modules/tokenizer/basic.cpp\
  lib/filter-c.cpp\
  lib/word_list-c.cpp\
  lib/info-c.cpp\
  lib/mutable_container-c.cpp\
  lib/error-c.cpp\
  lib/document_checker-c.cpp\
  lib/string_map-c.cpp\
  lib/new_config.cpp\
  lib/config-c.cpp\
  lib/string_enumeration-c.cpp\
  lib/can_have_error-c.cpp\
  lib/dummy.cpp\
  lib/new_filter.cpp\
  lib/string_list-c.cpp\
  lib/find_speller.cpp\
  lib/speller-c.cpp\
  lib/string_pair_enumeration-c.cpp\
  lib/new_checker.cpp

libaspell_la_LIBADD =  $(LIBINTL)

libaspell_la_LDFLAGS = ${extra_ldflags} -version-info 16:0:1

libpspell_la_SOURCES = lib/dummy.cpp

libpspell_la_LIBADD = libaspell.la

libpspell_la_LDFLAGS = ${extra_ldflags} -version-info 16:0:1

EXTRA_DIST += common/*.hpp common/*.h\
              modules/speller/default/*.hpp\
              lib/*.hpp

########################################################################
#
# Aspell Program
#

AM_CPPFLAGS += -DLOCALEDIR=\"$(localedir)\"

bin_PROGRAMS = word-list-compress aspell

word_list_compress_SOURCES = prog/compress.c

aspell_SOURCES = prog/aspell.cpp prog/check_funs.cpp prog/checker_string.cpp

aspell_LDADD = libaspell.la $(CURSES_LIB)

EXTRA_DIST += prog/*.hpp

########################################################################
#
# Filter Modules
#

# This is for filters which are ALWAYS static.  Currently only the
# URL filter

libaspell_la_SOURCES += modules/filter/url.cpp
static_optfiles = modules/filter/url-filter.opt

dynamic_optfiles = 

###
### To add a new filter follow the instruction that begin with '###'
###

### Add the .opt file your filter comes with
optfiles = \
  modules/filter/email-filter.opt\
  modules/filter/tex-filter.opt\
  modules/filter/sgml-filter.opt\
  modules/filter/context-filter.opt

### If your filter sources contain ALIAS line add .flt file
### corresponding to entire alias name
fltfiles = \
  modules/filter/latex.flt

if COMPILE_IN_FILTERS

static_optfiles += ${optfiles}

### Add your filter sources here,
### starting with file containing filter class definition followed by
### file containing filter member implementation.
libaspell_la_SOURCES +=\
  modules/filter/email.cpp\
  modules/filter/tex.cpp\
  modules/filter/sgml.cpp\
  modules/filter/context.hpp\
  modules/filter/context.cpp

else # not COMPILE_IN_FILTERS

dynamic_optfiles += ${optfiles}

### Add name of filter library containing your filter. Name always
### must look like lib<filtername>-filter.la see development manual
filter_LTLIBRARIES = libemail-filter.la libtex-filter.la\
	             libsgml-filter.la libcontext-filter.la

libemail_filter_la_SOURCES = modules/filter/email.cpp
libemail_filter_la_LIBADD = libaspell.la 
libemail_filter_la_LDFLAGS  = -avoid-version

libtex_filter_la_SOURCES = modules/filter/tex.cpp
libtex_filter_la_LIBADD = libaspell.la
libtex_filter_la_LDFLAGS  = -avoid-version

libsgml_filter_la_SOURCES  = modules/filter/sgml.cpp
libsgml_filter_la_LIBADD = libaspell.la
libsgml_filter_la_LDFLAGS  = -avoid-version

libcontext_filter_la_SOURCES  = modules/filter/context.hpp modules/filter/context.cpp
libcontext_filter_la_LIBADD = libaspell.la
libcontext_filter_la_LDFLAGS  = -avoid-version

### Before this line add the corresponding <yourfilterlibrary>_SOURCES and
### <yourfilterlibrary>_LIBADD lines.  The later at least has to look
### like <yourfilterlibrary>_LIBADD = ${top_builddir}/lib/libaspell.la
### in order to make your filter build properly

endif # COMPILE_IN_FILTERS

noinst_DATA += $(static_optfiles) gen/filter.pot
opt_DATA = $(dynamic_optfiles)
filter_DATA = $(fltfiles)

# settings.h added as a dependency so it will get recreated if
#   the COMPILE_IN_FILTERS option changes
gen/static_filters.src.cpp: ${static_optfiles} gen/mk-static-filter.pl gen/settings.h
	${PERLPROG} gen/mk-static-filter.pl ${static_optfiles}

lib/new_filter.cpp: gen/static_filters.src.cpp

gen/filter.pot: gen/mk-filter-pot.pl ${static_optfiles} ${dynamic_optfiles}
	${PERLPROG} gen/mk-filter-pot.pl

EXTRA_DIST += ${static_optfiles} ${dynamic_optfiles} ${fltfiles} \
              gen/mk-static-filter.pl gen/mk-filter-pot.pl\
              gen/filter.pot

CLEANFILES = gen/static_filters.src.cpp

########################################################################
#
# Mk Dirs Target 
#

common/config.cpp: gen/dirs.h

noinst_PROGRAMS = gen/mk-dirs_h
noinst_DATA += gen/dirs.h

gen/dirs.h: gen/mk-dirs_h
	cd gen; echo '#define PREFIX "${prefix}"'            >  dirs.h
	cd gen; ./mk-dirs_h ${prefix} DICT_DIR ${pkglibdir}  >> dirs.h
	cd gen; ./mk-dirs_h ${prefix} DATA_DIR ${pkgdatadir} >> dirs.h
	cd gen; ./mk-dirs_h ${prefix} CONF_DIR ${sysconfdir} >> dirs.h
	cd gen; ./mk-dirs_h ${prefix} FILTER_DIR ${pkglibdir} >> dirs.h
	cd gen; ./mk-dirs_h ${prefix} FILTER_OPT_DIR ${pkgdatadir} >> dirs.h

gen_mk_dirs_h_SOURCES = gen/mk-dirs_h.cpp

CLEANFILES += gen/dirs.h


########################################################################
#
# Scripts
#

bin_SCRIPTS = scripts/pspell-config scripts/run-with-aspell scripts/aspell-import
pkgdata_SCRIPTS = scripts/spell scripts/ispell

scripts/run-with-aspell: scripts/run-with-aspell.create
	sh ${srcdir}/scripts/run-with-aspell.create ${pkgdatadir} > scripts/run-with-aspell
	chmod 755 scripts/run-with-aspell

scripts/pspell-config: scripts/mkconfig
	sh ${srcdir}/scripts/mkconfig ${VERSION} ${datadir} ${pkgdatadir}

CLEANFILES += scripts/pspell-config scripts/run-with-aspell

EXTRA_DIST += scripts/mkconfig scripts/spell scripts/ispell \
              scripts/run-with-aspell.create scripts/aspell-import


########################################################################
#
# auto
#

# FIXME: add dependences

noinst_DATA += auto

mksrc = \
 auto/mk-src.in auto/mk-src.pl auto/mk-doc.pl\
 auto/MkSrc/CcHelper.pm  auto/MkSrc/Methods.pm\
 auto/MkSrc/ProcImpl.pm  auto/MkSrc/Read.pm\
 auto/MkSrc/Create.pm    auto/MkSrc/ProcCc.pm\
 auto/MkSrc/ProcNativeImpl.pm  auto/MkSrc/Type.pm\
 auto/MkSrc/Info.pm            auto/MkSrc/ProcCxx.pm\
 auto/MkSrc/ProcNative.pm      auto/MkSrc/Util.pm

#auto: @MAINTAINER_MODE_TRUE@ ${mksrc}
#	cd auto; perl mk-src.pl
#	cd auto; perl auto/mk-doc.pl
#	touch auto/auto

EXTRA_DIST += auto/auto auto/mk-src.txt ${mksrc}


########################################################################
#
# interfaces
#

include_HEADERS = interfaces/cc/aspell.h

pspell_includedir = ${includedir}/pspell

pspell_include_HEADERS = interfaces/cc/pspell.h


########################################################################
#
# Misc Top level
#

pkgdata_DATA = ${top_srcdir}/data/*.dat ${top_srcdir}/data/*.kbd

EXTRA_DIST += config.rpath  README ${pkgdata_DATA} m4/*.m4 \
              misc/po-filter.c

README: manual/readme.texi
	makeinfo --no-validate --no-headers $< > README

maintainer-clean-local:
	@find . \( -name '*.?pp' -o -name '*.h' -o -name '*.opt' \) -print |\
	    xargs grep -l "Automatically generated file." |\
	    xargs rm

dist-hook:
	mkdir $(distdir)/maintainer
	cp autogen config-opt config-debug TODO FIXMEs README-CVS \
           $(distdir)/maintainer

ACLOCAL_AMFLAGS = -I m4

.PHONY: .manual fake-manual

.manual:
	$(MAKE) -C manual manual
manual: .manual
	$(MAKE) -C manual manual

fake-manual: 
	$(MAKE) -C manual fake-manual
