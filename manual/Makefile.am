pkgdoc_DATA = manual.dvi manual.tex manual2.lyx manual.aux\
              devel.dvi  devel.tex devel2.lyx devel.aux
noinst_DATA = manual-html manual-text devel-html devel-text

MANUALCLEANFILES = manual2.tex manual2.aux manual2.log \
                   devel2.tex devel2.aux devel2.log \
                   manual.* manual-html manual-text\
                   devel.* devel-html devel-text \
                   man-html/* man-text/* dev-html/* dev-text/*

MAINTAINERCLEANFILES = ${MANUALCLEANFILES}

TOUCH_LIST = manual2.lyx manual2.tex man-proc.pl manual.tex manual.dvi \
             manual-html html2txt.pl manual-text \
             devel2.lyx devel2.tex man-proc.pl mk-src.tex devel.tex devel.dvi \
             devel-html devel-text

BOOTSTRAP_LIST = man-html/manual.html man-text/manual.txt\
                 dev-html/devel.html dev-text/devel.txt

# FIXME
export TEXT_DUMP=@TEXT_DUMP@
export DUMP_WIDTH=@DUMP_WIDTH@

.PHONY: manual fake-manual manual-clean

manual: manual2.tex manual.tex manual.dvi manual-html manual-text\
        devel2.tex devel.tex devel.dvi devel-html devel-text

fake-manual:
	touch $(TOUCH_LIST) ${BOOTSTRAP_LIST}

manual-clean:
	-rm -f ${MANUALCLEANFILES}
	touch ${BOOTSTRAP_LIST}

maintainer-clean-hook:
	touch ${BOOTSTRAP_LIST}

#
# Main Manual
#

manual2.tex: @MAINTAINER_MODE_TRUE@ manual2.lyx
	lyx --export latex manual2.lyx

manual.tex: @MAINTAINER_MODE_TRUE@ man-proc.pl manual2.tex
	perl man-proc.pl manual

manual.dvi: @MAINTAINER_MODE_TRUE@ manual.tex
	latex manual.tex
	latex manual.tex
	latex manual.tex

manual-html: @MAINTAINER_MODE_TRUE@ manual.dvi
	-rm man-html/*
	latex2html \
	  -split 3 \
          -toc_depth 3 \
	  -dir man-html \
	  -long_titles 2 \
	  -local_icons \
	  -show_section_numbers \
	  manual.tex
	chmod -R ugo+rX man-html/
	touch manual-html

manual-text: @MAINTAINER_MODE_TRUE@ manual-html html2txt.pl
	-rm man-text/*
	perl html2txt.pl man
	touch manual-text

manual.ps: @MAINTAINER_MODE_TRUE@ manual.dvi
	dvips -o manual.ps -t letter manual.dvi

#
# Devel Docs
#

devel2.tex: @MAINTAINER_MODE_TRUE@ devel2.lyx
	lyx --export latex devel2.lyx

devel.tex: @MAINTAINER_MODE_TRUE@ man-proc.pl devel2.tex mk-src.tex
	perl man-proc.pl devel

devel.dvi: @MAINTAINER_MODE_TRUE@ devel.tex
	latex devel.tex
	latex devel.tex
	latex devel.tex

devel-html: @MAINTAINER_MODE_TRUE@ devel.dvi
	-rm dev-html/*
	latex2html \
	  -split 3 \
          -toc_depth 3 \
	  -dir dev-html \
	  -long_titles 2 \
	  -local_icons \
	  -show_section_numbers \
	  devel.tex
	chmod -R ugo+rX dev-html/
	touch devel-html

devel-text: @MAINTAINER_MODE_TRUE@ devel-html html2txt.pl
	-rm dev-text/*
	perl html2txt.pl dev
	touch devel-text

devel.ps: @MAINTAINER_MODE_TRUE@ devel.dvi
	dvips -o devel.ps -t letter devel.dvi

nobase_pkgdoc_DATA = man-html/* man-text/* dev-html/* dev-text/*

MAINTAINERCLEANFILES += ${nobase_pkgdoc_DATA} WARNINGS

EXTRA_DIST = ${pkgdoc_DATA} ${nobase_pkgdoc_DATA}\
             html.sty html2txt.pl edit-man manual2.per man-proc.pl mk-src.tex\
	     manual-text manual-html devel-html devel-text

dist-hook:
	cd $(distdir); touch $(TOUCH_LIST)
