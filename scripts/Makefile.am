
pkgadatadir = ${pkgdatadir}

bin_SCRIPTS = pspell-config run-with-aspell
pkgadata_SCRIPTS = spell ispell

run-with-aspell: run-with-aspell.create
	sh run-with-aspell.create ${pkgadatadir} > run-with-aspell
	chmod 755 run-with-aspell

pspell-config: mkconfig
	sh mkconfig ${VERSION} ${datadir} ${pkgdatadir}

CLEANFILES = pspell-config run-with-aspell

EXTRA_DIST = mkconfig spell ispell run-with-aspell.create

