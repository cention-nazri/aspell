
AUTOMAKE_OPTIONS = foreign

SUBDIRS = m4 auto common modules lib examples prog data scripts interfaces manual po

EXTRA_DIST = config.rpath  README

README: manual/man-text/2_Getting.txt
	cp manual/man-text/2_Getting.txt README

maintainer-clean-local:
	@find . \( -name '*.?pp' -o -name '*.h' \) -print |\
	    xargs grep -l "Automatically generated file." |\
	    xargs rm

ACLOCAL_AMFLAGS = -I m4
