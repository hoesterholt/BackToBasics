dnl Process this file with autoconf to produce a configure script.
dnl Created by Anjuta application wizard.

AC_INIT(backtobasics, 0.3)

AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.11])

AM_SILENT_RULES([yes])

AC_PROG_CC




dnl ***************************************************************************
dnl Internationalization
dnl ***************************************************************************
IT_PROG_INTLTOOL([0.35.0])

GETTEXT_PACKAGE=backtobasics
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [GETTEXT package name])

AM_GLIB_GNU_GETTEXT


PKG_CHECK_MODULES(BACKTOBASICS, [gtk+-3.0 >= 2.90.3 ao vorbisfile flac libmpg123 libstreamripper taglib taglib_c gdk-3.0 libpcre elementals libgtklistmodel libconfig libtagcoverart libcurl webkitgtk-3.0])
PKG_CHECK_MODULES(BTB_PLAY, [ao vorbisfile flac libmpg123 libstreamripper taglib_c libpcre elementals libtagcoverart libcurl libconfig])

CHK_APPINDICATOR=`pkg-config --exists appindicator3-0.1;echo $?`
if test x"$CHK_APPINDICATOR" = "x0"; then
PKG_CHECK_MODULES(APPINDICATOR, [appindicator3-0.1])
fi
AM_CONDITIONAL([HAVE_APPINDICATOR], [test x$CHK_APPINDICATOR = x0])

##########
# Figure out what C libraries are required to compile programs
# that use "readline()" library.
#
TARGET_READLINE_LIBS=""
TARGET_READLINE_INC=""
TARGET_HAVE_READLINE=0
AC_ARG_ENABLE([readline],
	[AC_HELP_STRING([--disable-readline],[disable readline support [default=detect]])],
	[with_readline=$enableval],
	[with_readline=auto])

if test x"$with_readline" != xno; then
	found="yes"

	AC_ARG_WITH([readline-lib],
		[AC_HELP_STRING([--with-readline-lib],[specify readline library])],
		[with_readline_lib=$withval],
		[with_readline_lib="auto"])
	if test "x$with_readline_lib" = xauto; then
		save_LIBS="$LIBS"
		LIBS=""
		AC_SEARCH_LIBS(tgetent, [readline ncurses curses termcap], [term_LIBS="$LIBS"], [term_LIBS=""])
		AC_CHECK_LIB([readline], [readline], [TARGET_READLINE_LIBS="-lreadline"], [found="no"])
		TARGET_READLINE_LIBS="$TARGET_READLINE_LIBS $term_LIBS"
		LIBS="$save_LIBS"
	else
		TARGET_READLINE_LIBS="$with_readline_lib"
	fi

	AC_ARG_WITH([readline-inc],
		[AC_HELP_STRING([--with-readline-inc],[specify readline include paths])],
		[with_readline_inc=$withval],
		[with_readline_inc="auto"])
	if test "x$with_readline_inc" = xauto; then
		AC_CHECK_HEADER(readline.h, [found="yes"], [
			found="no"
			if test "$cross_compiling" != yes; then
				for dir in /usr /usr/local /usr/local/readline /usr/contrib /mingw; do
					for subdir in include include/readline; do
						AC_CHECK_FILE($dir/$subdir/readline.h, found=yes)
						if test "$found" = "yes"; then
							TARGET_READLINE_INC="-I$dir/$subdir -DHAVE_READLINE"
							break
						fi
					done
					test "$found" = "yes" && break
				done
			fi
		])
	else
		TARGET_READLINE_INC="$with_readline_inc"
	fi

	if test x"$found" = xno; then
		TARGET_READLINE_LIBS=""
		TARGET_READLINE_INC=""
		TARGET_HAVE_READLINE=0
	else
		TARGET_HAVE_READLINE=1
	fi
fi

AC_SUBST(TARGET_READLINE_LIBS)
AC_SUBST(TARGET_READLINE_INC)
AC_SUBST(TARGET_HAVE_READLINE)

dnl
dnl  Detect pthreads libraries
dnl

PTHREAD_LIBS=error
AC_CHECK_LIB(pthread, pthread_attr_init, PTHREAD_LIBS="-lpthread")
if test "x$PTHREAD_LIBS" = xerror; then
        AC_CHECK_LIB(pthreads, pthread_attr_init, PTHREAD_LIBS="-lpthreads")
fi
if test "x$PTHREAD_LIBS" = xerror; then
        AC_CHECK_LIB(c_r, pthread_attr_init, PTHREAD_LIBS="-lc_r")
fi
if test "x$PTHREAD_LIBS" = xerror; then
	AC_MSG_ERROR([Pthreads are mandatory])
fi
AC_SUBST(PTHREAD_LIBS)


AC_OUTPUT([
Makefile
src/Makefile
po/Makefile.in
])
