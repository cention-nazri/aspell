
AC_INIT
AM_CONFIG_HEADER(common/settings.h)
AM_INIT_AUTOMAKE(aspell, 0.50)

AM_MAINTAINER_MODE

AC_ARG_ENABLE(docdir, 
[  --enable-docdir=DIR      documentation files in DIR [PREFIX/doc/aspell] ], 
pkgdocdir=$enable_docdir, pkgdocdir=\${prefix}/share/doc/aspell )
AC_SUBST(pkgdocdir)

AC_ARG_ENABLE(pkgdatadir, 
[  --enable-pkgdatadir=DIR  device independent data files [DATADIR/aspell] ], 
pkgdatadir=$enable_pkgdatadir, pkgdatadir=\${datadir}/aspell )
AC_SUBST(pkgdatadir)

AC_ARG_ENABLE(pkglibdir, 
[  --enable-pkglibdir=DIR   device dependent data files [LIBDIR/aspell] ], 
pkglibdir=$enable_dictdir, pkglibdir=${libdir}/aspell )
AC_SUBST(pkglibdir)

AC_PROG_CXX
AC_LANG([C++])

AM_PROG_LIBTOOL

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#                                                                 #
#                           Posix tests                           #
#                                                                 #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

AC_MSG_CHECKING(if file locking and truncating is supported)
AC_TRY_LINK( 
  [#include <fcntl.h>
   #include <unistd.h>],
  [int fd;	
   struct flock fl;
   fcntl(fd, F_SETLKW, &fl);
   ftruncate(fd,0);],
  [AC_MSG_RESULT(yes)
   AC_DEFINE(USE_FILE_LOCKS, 1,
             [Defined if file locking and truncating is supported])
  ],
  [AC_MSG_RESULT(no)] )

AC_MSG_CHECKING(if mmap and friends is supported)
AC_TRY_LINK(
  [#include <fcntl.h>
   #include <unistd.h>
   #include <sys/mman.h>],
  [char * p = (char *)mmap(NULL, 10, PROT_READ, MAP_SHARED, -1, 2);
   munmap(p,10);],
  [AC_MSG_RESULT(yes)
   AC_DEFINE(HAVE_MMAP, 1, [Defined if mmap and friends is supported])],
  [AC_MSG_RESULT(no)] )

AC_MSG_CHECKING(if file ino is supported)
touch conftest-f1
touch conftest-f2
AC_TRY_RUN(
  [#include <sys/stat.h>
   int main() {
     struct stat s1,s2;
     if (stat("conftest-f1",&s1) != 0) exit(2);
     if (stat("conftest-f2",&s2) != 0) exit(2);
     exit (s1.st_ino != s2.st_ino ? 0 : 1);
   }
  ],
  [AC_MSG_RESULT(yes)
   AC_DEFINE(USE_FILE_INO, 1, [Defined if file ino is supported])
  ],
  [AC_MSG_RESULT(no)],
  [if test "$MINGW32" = yes
   then
     AC_MSG_RESULT(cant run test!, assuming no)
   else
     AC_MSG_RESULT(cant run test!, assuming yes)
     AC_DEFINE(USE_FILE_INO, 1, [Defined if file ino is supported])
   fi
  ]
)

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#                                                                 #
#                    Compiler Quirks Tests                        #
#                                                                 #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

AC_MSG_CHECKING(for STL rel_ops pollution)
AC_TRY_COMPILE(
  [#include <utility>

   template <typename T>
   class C {};

   template <typename T>
   bool operator== (C<T>, C<T>) {return true;}

   template <typename T>
   bool operator!= (C<T>, C<T>) {return false;}],
  [C<int> c1, c2;
   bool v = c1 != c2;],
  [AC_MSG_RESULT(no)],
  [AC_MSG_RESULT(yes)
   AC_DEFINE(REL_OPS_POLLUTION, 1, 
             [Defined if STL rel_ops polute the global namespace])
  ]
)

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#                                                                 #
#                           Output                                #
#                                                                 #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

AC_CONFIG_FILES([Makefile common/Makefile lib/Makefile data/Makefile
                 modules/Makefile 
                 modules/tokenizer/Makefile
                 modules/speller/Makefile modules/speller/default/Makefile
		 interfaces/Makefile interfaces/cc/Makefile scripts/Makefile
                 examples/Makefile prog/Makefile])
AC_OUTPUT
