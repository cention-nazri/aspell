
INCLUDES = -I${top_srcdir}/common/\
           -I${top_srcdir}/interfaces/cc/ \
           -I${top_srcdir}/modules/speller/default/ \
           $(CURSES_INCLUDE)

bin_PROGRAMS = word-list-compress aspell

word_list_compress_SOURCES = compress.c

aspell_SOURCES = aspell.cpp check_funs.cpp checker_string.cpp

aspell_LDADD = ${top_builddir}/lib/libaspell.la $(CURSES_LIB)

EXTRA_DIST = *.hpp





