noinst_DATA = auto

mksrc_modules = \
 MkSrc/CcHelper.pm  MkSrc/Methods.pm  MkSrc/ProcImpl.pm        MkSrc/Read.pm\
 MkSrc/Create.pm    MkSrc/ProcCc.pm   MkSrc/ProcNativeImpl.pm  MkSrc/Type.pm\
 MkSrc/Info.pm      MkSrc/ProcCxx.pm  MkSrc/ProcNative.pm      MkSrc/Util.pm

auto: @MAINTAINER_MODE_TRUE@ mk-src.in mk-src.pl mk-doc.pl ${mksrc_modules}
	perl mk-src.pl
	perl mk-doc.pl
	touch auto

EXTRA_DIST = auto mk-src.in mk-src.pl mk-src.txt mk-doc.pl ${mksrc_modules}
