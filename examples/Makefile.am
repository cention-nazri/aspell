CCLD=$(CXX)
noinst_PROGRAMS = example-c list-dicts

filterdir = "$(libdir)/aspell"
filter_PROGRAMS = po-filter.la

INCLUDES = -I${top_srcdir}/interfaces/cc/


example_c_SOURCES = example-c.c

example_c_LDADD = ../lib/libaspell.la


list_dicts_SOURCES = list-dicts.c

list_dicts_LDADD = ../lib/libaspell.la

po_filter_la_SOURCES = po-filter.c
po-filter.lo: $(srcdir)/po-filter.c
	$(LIBTOOL) --mode=compile $(CC) -c  \
		$(CFLAGS) $(AM_CFLAGS) $(INCLUDES) $(srcdir)/po-filter.c

po-filter.la$(EXEEXT): po-filter.lo
	$(LIBTOOL) --mode=link $(CC) -module -export-dynamic \
		-o po-filter.la po-filter.lo  \
		-rpath $(filterdir)
