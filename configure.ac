AC_INIT([jsonrpc-c], [0.1], [hmng@farol.pt])
AC_CONFIG_SRCDIR([src/jsonrpc-c.c])

AM_PROG_AR
LT_INIT
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CC
AM_PROG_CC_C_O

#AC_CHECK_LIB([ev], [ev_io_init],[], [AC_MSG_ERROR([libev was not found])])
#AC_CHECK_LIB([ev],[ev_sleep],[],[AC_MSG_ERROR([libev was not found])])

# stollen from lighttpd's configure.ac
AC_MSG_CHECKING([for libev support])
AC_ARG_WITH([libev],
    [AS_HELP_STRING([--with-libev@<:@=PATH@:>@],[Search for libev in PATH/include and PATH/lib])],
    [WITH_LIBEV=$withval],[WITH_LIBEV=yes])

LIBEV_CFLAGS=""
LIBEV_LIBS=""

PKG_CHECK_MODULES([LIBEV], [libev], [], [
	# no pkg-config for libev, searching manually:
	if test "$WITH_LIBEV" != "yes"; then
		LIBEV_CFLAGS="-I$WITH_LIBEV/include"
		LIBEV_LIBS="-L$WITH_LIBEV/lib -lev"
	else
		AC_CHECK_HEADERS([ev.h],[
			AC_CHECK_LIB([ev], [ev_time], [
					LIBEV_LIBS="-lev"
				],[
					AC_MSG_ERROR([libev not found])
				]
			)],[
				AC_MSG_ERROR([libev not found])
			]
		)
	fi
])

AC_SUBST([LIBEV_CFLAGS])
AC_SUBST([LIBEV_LIBS])

PTHREAD_LIBS=error
AC_CHECK_LIB(pthread, pthread_attr_init, PTHREAD_LIBS="-lpthread")
if test "x$PTHREAD_LIBS" = xerror; then
    AC_CHECK_LIB(pthreads, pthread_attr_init, PTHREAD_LIBS="-lpthreads")
fi
if test "x$PTHREAD_LIBS" = xerror; then
    AC_CHECK_LIB(c_r, pthread_attr_init, PTHREAD_LIBS="-lc_r")
fi
if test "x$PTHREAD_LIBS" = xerror; then
    AC_CHECK_FUNC(pthread_attr_init, PTHREAD_LIBS="")
fi
AC_SUBST([PTHREAD_LIBS])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 include/Makefile
 src/Makefile
 example/Makefile
])
AC_OUTPUT

