noinst_DATA = auto

auto: @MAINTAINER_MODE_TRUE@ mk-src.in mk-src.pl
	cd ../; perl auto/mk-src.pl
	touch auto

EXTRA_DIST = auto mk-src.in mk-src.pl
