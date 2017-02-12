dnl                                               -*- Autoconf -*-
dnl Process this file with autoconf to produce a configure script.

AC_INIT([ODR-SourceCompanion], [0.0.1], [http://opendigitalradio.org/])
AC_CONFIG_AUX_DIR(.)
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([tar-ustar foreign])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_CONFIG_HEADER([config.h])

dnl Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL
LT_INIT

AX_CXX_COMPILE_STDCXX_11(noext,mandatory)

# std::thread requires pthread
AX_PTHREAD( [
            AC_DEFINE(HAVE_PTHREAD,1,[Define if you have POSIX threads libraries and header files.])
            CLIBS="$PTHREAD_LIBS $LIBS"
            CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
            LDFLAGS="$LDFLAGS $PTHREAD_CFLAGS"
            CC="$PTHREAD_CC"], [AC_MSG_ERROR([requires pthread])] )

AC_CHECK_LIB([m], [sin])

AC_CHECK_LIB([rt], [clock_gettime], [], [AC_MSG_ERROR([library rt is missing])])


AC_CHECK_LIB(zmq, zmq_init, , AC_MSG_ERROR(ZeroMQ libzmq is required))


AM_CONDITIONAL([IS_GIT_REPO], [test -d '.git'])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

echo
echo "***********************************************"
echo "ODR-SourceCompanion configured"
