
INCLUDES = -I../interfaces/cc/ -I../common

lib_LTLIBRARIES = libaspell.la libpspell.la

libaspell_la_SOURCES =\
  can_have_error-c.cpp  info-c.cpp               string_list-c.cpp\
  config-c.cpp          speller-c.cpp            string_map-c.cpp\
  error-c.cpp           mutable_container-c.cpp  string_pair_enumeration-c.cpp\
  find_speller.cpp      new_checker.cpp new_filter.cpp new_config.cpp\
  string_enumeration-c.cpp   word_list-c.cpp \
  filter-c.cpp          document_checker-c.cpp

libaspell_la_LIBADD = ../common/libaspell-common.la \
                      ../modules/speller/default/libaspell-speller-default.la \
                      ../modules/filter/libaspell-filter-standard.la \
                      ../modules/tokenizer/libaspell-tokenizer-standard.la

libaspell_la_LDFLAGS = -version-info 15:0:0

libpspell_la_SOURCES = dummy.cpp

libpspell_la_LIBADD = libaspell.la

libpspell_la_LDFLAGS = -version-info 15:0:0


