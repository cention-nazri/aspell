
INCLUDES = -I${top_srcdir}/common

noinst_LTLIBRARIES = libaspell-speller-default.la

libaspell_speller_default_la_SOURCES = \
 data.cpp            leditdist.cpp     primes.cpp         writable_base.cpp\
 editdist.cpp        speller_impl.cpp  readonly_ws.cpp    writable_repl.cpp\
 file_data_util.cpp  multi_ws.cpp      split.cpp          writable_ws.cpp\
 l2editdist.cpp      phonet.cpp        suggest.cpp\
 language.cpp        phonetic.cpp      typo_editdist.cpp


libaspell_speller_default_la_LDFLAGS = -release 0.50

EXTRA_DIST = *.hpp

