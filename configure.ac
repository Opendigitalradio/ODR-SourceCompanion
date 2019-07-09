dnl                                               -*- Autoconf -*-
dnl Process this file with autoconf to produce a configure script.

AC_INIT([ODR-SourceCompanion], [0.4.0], [http://opendigitalradio.org/])
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

AX_CHECK_COMPILE_FLAG([-Wduplicated-cond], [CFLAGS="$CFLAGS -Wduplicated-cond"], [], ["-Werror"])
AX_CHECK_COMPILE_FLAG([-Wduplicated-branches], [CFLAGS="$CFLAGS -Wduplicated-branches"], [], ["-Werror"])
AX_CHECK_COMPILE_FLAG([-Wlogical-op], [CFLAGS="$CFLAGS -Wlogical-op"], [], ["-Werror"])
AX_CHECK_COMPILE_FLAG([-Wrestrict], [CFLAGS="$CFLAGS -Wrestrict"], [], ["-Werror"])
AX_CHECK_COMPILE_FLAG([-Wdouble-promotion], [CFLAGS="$CFLAGS -Wdouble-promotion"], [], ["-Werror"])
AX_CHECK_COMPILE_FLAG(["-Wformat=2"], [CFLAGS="$CFLAGS -Wformat=2"], [], ["-Werror"])

AC_LANG_PUSH([C++])
AX_CHECK_COMPILE_FLAG([-Wduplicated-cond], [CXXFLAGS="$CXXFLAGS -Wduplicated-cond"], [], ["-Werror"])
AX_CHECK_COMPILE_FLAG([-Wduplicated-branches], [CXXFLAGS="$CXXFLAGS -Wduplicated-branches"], [], ["-Werror"])
AX_CHECK_COMPILE_FLAG([-Wlogical-op], [CXXFLAGS="$CXXFLAGS -Wlogical-op"], [], ["-Werror"])
AX_CHECK_COMPILE_FLAG([-Wrestrict], [CXXFLAGS="$CXXFLAGS -Wrestrict"], [], ["-Werror"])
AX_CHECK_COMPILE_FLAG([-Wdouble-promotion], [CXXFLAGS="$CXXFLAGS -Wdouble-promotion"], [], ["-Werror"])
AX_CHECK_COMPILE_FLAG(["-Wformat=2"], [CXXFLAGS="$CXXFLAGS -Wformat=2"], [], ["-Werror"])
AC_LANG_POP([C++])


AC_CHECK_LIB(zmq, zmq_init, , AC_MSG_ERROR(ZeroMQ libzmq is required))
AC_CHECK_LIB(fdk-aac, aacEncOpen, , AC_MSG_ERROR(The FDK-AAC library is required))
# We need to have the ODR fdk-aac, the upstream one doesn't support DAB+
AC_MSG_CHECKING([for DAB+ support in FDK-AAC])
AC_COMPILE_IFELSE( [AC_LANG_PROGRAM([[#include <fdk-aac/aacenc_lib.h>]],
                                    [[char dummy[TT_DABPLUS];]])],
                   [
                      AC_MSG_RESULT([yes])
                    ],
                    [
                      AC_MSG_RESULT([no])
                      AC_MSG_ERROR(["Your FDK-AAC does not support DAB+, make sure you have installed the ODR version!"])
                    ]
                 )


AM_CONDITIONAL([IS_GIT_REPO], [test -d '.git'])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

echo
echo "***********************************************"
echo "ODR-SourceCompanion configured"
