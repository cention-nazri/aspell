
#INCLUDES = 

pkgdatadir = @pkgdatadir@
pkglibdir = @pkglibdir@

noinst_PROGRAMS = mk-dirs_h

dirs.h: mk-dirs_h
	echo '#define PREFIX "${prefix}"'            >  dirs.h
	./mk-dirs_h ${prefix} DICT_DIR ${pkglibdir}  >> dirs.h
	./mk-dirs_h ${prefix} DATA_DIR ${pkgdatadir} >> dirs.h
	./mk-dirs_h ${prefix} CONF_DIR ${sysconfdir} >> dirs.h

mk_dirs_h_SOURCES = mk-dirs_h.cpp

config.cpp: dirs.h

dist-hook:
	-rm dirs.h

CLEANFILES = dirs.h



lib_LTLIBRARIES = libaspell-common.la

libaspell_common_la_SOURCES = \
  string.cpp getdata.cpp itemize.cpp file_util.cpp \
  string_buffer.cpp \
  string_map.cpp string_list.cpp \
  config.cpp\
  posib_err.cpp errors.cpp error.cpp\
  fstream.cpp iostream.cpp info.cpp \
  can_have_error.cpp \
  convert.cpp tokenizer.cpp \
  speller.cpp document_checker.cpp filter.cpp \
  strtonum.cpp

libaspell_common_la_LDFLAGS = ${extra_ldflags} -release 0.50.1

EXTRA_DIST = *.hpp gettext.h
