
INCLUDES = -I../interfaces/cc/ -I../common

lib_LTLIBRARIES = libpspell.la

libpspell_la_SOURCES =\
  can_have_error-c.cpp  info-c.cpp               string_list-c.cpp\
  config-c.cpp          manager-c.cpp            string_map-c.cpp\
  error-c.cpp           mutable_container-c.cpp  string_pair_enumeration-c.cpp\
  find_manager.cpp      string_enumeration-c.cpp   word_list-c.cpp

libpspell_la_LIBADD = ../common/libpspell-common.la ../modules/aspell/libpspell-aspell.la

libpspell_la_LDFLAGS = -version-info 12:0:0
