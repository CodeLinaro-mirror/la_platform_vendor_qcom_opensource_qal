#                                               -*- Autoconf -*-
# configure.ac -- Autoconf script for pal
#

# Process this file with autoconf to produce a configure script

# Requires autoconf tool later than 2.61
AC_PREREQ(2.69)
# Initialize the pal package version 1.0.0
AC_INIT([pal],1.0.1)
# Does not strictly follow GNU Coding standards
AM_INIT_AUTOMAKE([foreign subdir-objects])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AC_PROG_LIBTOOL
AC_PROG_AWK
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
PKG_PROG_PKG_CONFIG

AC_ARG_WITH([openwrt],
    AS_HELP_STRING([use openwrt (default is no)]),
    [with_openwrt=$withval],
    [with_openwrt=no])
AM_CONDITIONAL([BUILDSYSTEM_OPENWRT], [test "x${with_openwrt}" = "xyes"])

AC_ARG_WITH([glib],
      AC_HELP_STRING([--with-glib],
         [enable glib, Build against glib. Use this when building for HLOS systems which use glib]))

if (test "x${with_glib}" = "xyes"); then
        PKG_CHECK_MODULES(GTHREAD, gthread-2.0 >= 2.16, dummy=yes,
                                AC_MSG_ERROR(GThread >= 2.16 is required))
        PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.16, dummy=yes,
                                AC_MSG_ERROR(GLib >= 2.16 is required))
        GLIB_CFLAGS="$GLIB_CFLAGS $GTHREAD_CFLAGS"
        GLIB_LIBS="$GLIB_LIBS $GTHREAD_LIBS"

        AC_SUBST(GLIB_CFLAGS)
        AC_SUBST(GLIB_LIBS)
fi

AM_CONDITIONAL(USE_GLIB, test "x${with_glib}" = "xyes")
AM_CONDITIONAL([IS_SDXLEMUR], [test x$IS_SDXLEMUR_TARGET = xtrue])
AM_CONDITIONAL([IS_SA525M], [test x$IS_SA525M_TARGET = xtrue])

AC_ARG_WITH([spf],
      AC_HELP_STRING([--with-spf],
         [enable spf, Build against spf. Use this when building for HLOS systems which use spf]))

SPF_CFLAGS=
AS_IF([test "x$with_spf" != "xno"], [SPF_CFLAGS="-I$with_spf"])

AC_SUBST(SPF_CFLAGS)

AC_ARG_WITH([acdbdata],
      AC_HELP_STRING([--with-acdbdata],
         [enable acdbdata, Build against acdbdata. Use this when building for HLOS systems which use acdbdata]))

ACDBDATA_CFLAGS=
AS_IF([test "x$with_acdbdata" != "xno"], [ACDBDATA_CFLAGS="-I$with_acdbdata"])

AC_SUBST(ACDBDATA_CFLAGS)

AC_ARG_WITH([compress],
    AS_HELP_STRING([compile compress (default is no)]),
    [with_compress=$withval],
    [with_compress=no])
AM_CONDITIONAL([COMPILE_COMPRESS], [test "x${with_compress}" = "xyes"])

AC_CHECK_FUNCS([strlcpy])
AM_CONDITIONAL(USE_G_STRLCPY, [test  "$ac_cv_func_strlcpy" = "no"])

AC_ARG_WITH([dltlogging],
    AC_HELP_STRING([--with-dltlogging], [Enable DLT logging support]),
    [with_dltlogging="$withval"],
    [with_dltlogging=no])

AM_CONDITIONAL(USE_DLT, test "x${with_dltlogging}" = "xyes")

if test "x${with_dltlogging}" = "xyes"; then
    PKG_CHECK_MODULES([DLTLOG], [automotive-dlt])
    AC_DEFINE([USE_DLT], [1], [Define if using DLT logging])
    AC_SUBST([DLTLOG_CFLAGS])
    AC_SUBST([DLTLOG_LIBS])
fi

AC_CONFIG_FILES([ Makefile pal.pc ])
AC_OUTPUT
