
INCLUDES = -I../../common

bin_PROGRAMS = aspell-util

aspell_util_SOURCES = aspell-util.cpp

aspell_util_LDADD = ../../common/libpspell-common.la libpspell-aspell.la

lib_LTLIBRARIES = libpspell-aspell.la

libpspell_aspell_la_SOURCES = \
 data.cpp            leditdist.cpp     primes.cpp         writable_base.cpp\
 editdist.cpp        manager_impl.cpp  readonly_ws.cpp    writable_repl.cpp\
 file_data_util.cpp  multi_ws.cpp      split.cpp          writable_ws.cpp\
 l2editdist.cpp      phonet.cpp        suggest.cpp\
 language.cpp        phonetic.cpp      typo_editdist.cpp


libpspell_aspell_la_LDFLAGS = -release 0.15.0
