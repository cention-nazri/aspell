
AC_INIT
AM_CONFIG_HEADER(common/settings.h)
AM_INIT_AUTOMAKE(aspell, 0.50-20020815)

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

AC_DISABLE_STATIC
AC_PROG_LIBTOOL

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
#                     Terminal function tests                     #
#                                                                 #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

AC_SUBST(CURSES_LIB)
AC_SUBST(CURSES_INCLUDE)

if test "$enable_curses" != "no"
then
  use_curses=t
  case "$enable_curses" in
    yes | ""                     )                             ;;
    /* | *lib* | *.a | -l* | -L* ) CURSES_LIB="$enable_curses" ;;
    *                            ) CURSES_LIB=-l$enable_curses ;;
  esac
  case "$enable_curses_include" in
    yes | no | "")                                         ;;
    -I*          ) CURSES_INCLUDE="$enable_curses_include" ;;
    *            ) CURSES_INCLUDE=-I$enable_curses_include ;;
  esac
fi

if test "$use_curses"
then
 
  if test -z "$CURSES_LIB"
  then

    AC_CHECK_LIB(curses, initscr,
      [CURSES_LIB=-lcurses],
      [

    dnl else if
    AC_CHECK_LIB(ncurses, initscr,
      [CURSES_LIB=-lncurses])
    ] )
    
  fi

  if test -n "$CURSES_LIB"
  then
     AC_MSG_CHECKING(if standard curses include sequence will work)
     ORIG_LIBS="$LIBS"
     ORIG_CPPFLAGS="$CPPFLAGS"
     CPPFLAGS="$CURSES_INCLUDE $CPPFLAGS"
     LIBS="$CURSES_LIB $LIBS"
     AC_TRY_LINK(
       [#include <termios.h>
	#include <unistd.h>
	#include <curses.h>
	#include <term.h>],
       [tigetstr(const_cast<char *>("cup"));],
       [AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_LIBCURSES, 1, 
                  [Defined if the curses library is available])
	posix_termios=t
	AC_DEFINE(CURSES_INCLUDE_STANDARD, 1, 
                  [Defined if no special Workarounds are needed for Curses headers])],
       [AC_MSG_RESULT(no)

     dnl else if
     AC_MSG_CHECKING(if curses workaround I will work)
     AC_TRY_LINK(
       [#include <termios.h>
	#include <unistd.h>
	#include <curses.h>
	extern "C" {char * tigetstr(char * capname);}],
       [tigetstr(const_cast<char *>("cup"));],
       [AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_LIBCURSES, 1, [])
        posix_termios=t
	AC_DEFINE(CURSES_INCLUDE_WORKAROUND_1, 1, 
                  [Defined if special Wordaround I is need for Curses headers])],
       [AC_MSG_RESULT(no)

     dnl else if
     AC_MSG_CHECKING(if curses without Unix stuff will work)
     AC_TRY_LINK(
       [#include <curses.h>],
       [initscr();],
       [AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_LIBCURSES, 1, [])
	AC_DEFINE(CURSES_ONLY, 1, 
                  [Defined if curses like POSIX Functions should be used])
        curses_only=t],
       [AC_MSG_RESULT(no)
        

     dnl else if
     use_curses=false
     CURSES_LIBS=""
     CURSES_INCLUDE=""

     ]) ]) ])

     CPPFLAGS="$ORIG_CPPFLAGS"
     LIBS="$ORIG_LIBS"
  fi
fi

if test -z "$posix_termios" -a -z "$curses_only"
then
  AC_MSG_CHECKING(if posix termios is supported)
  AC_TRY_LINK(
    [#include <termios.h>
     #include <unistd.h>
     #include <stdlib.h>],
    [isatty (STDIN_FILENO);
     atexit(0);
     termios attrib;
     tcgetattr (STDIN_FILENO, &attrib);
     tcsetattr (STDIN_FILENO, TCSAFLUSH, &attrib);],
    [AC_MSG_RESULT(yes)
     posix_termios=t],
    [AC_MSG_RESULT(no)]
  )
fi

if test -z "$posix_termios" -a -z "$use_curses"
then
  dnl else if
  AC_MSG_CHECKING(if getch supported)
  AC_TRY_LINK(
    [extern "C" {int getch();}],
    [char c = getch();],
    [AC_MSG_RESULT(yes)
     AC_DEFINE(HAVE_GETCH, 1, [Defined if msdos getch is supported])
    ],
    [AC_MSG_RESULT(no)]
  )
fi

if test "$posix_termios"
then
  AC_DEFINE(POSIX_TERMIOS, 1, [Defined if Posix Termios is Supported])
fi

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
                 auto/Makefile modules/Makefile 
                 modules/filter/Makefile modules/tokenizer/Makefile
                 modules/speller/Makefile modules/speller/default/Makefile
		 interfaces/Makefile interfaces/cc/Makefile scripts/Makefile
                 examples/Makefile prog/Makefile manual/Makefile
		 manual/man-html/Makefile manual/man-text/Makefile
		 manual/dev-html/Makefile manual/dev-text/Makefile
		 ])
AC_OUTPUT
