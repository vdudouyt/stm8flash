#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([stm8flash], [?], [https://github.com/vdudouyt/stm8flash/issues])
AC_CONFIG_SRCDIR([src/main.c])
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_HEADERS([config.h])

AC_OUTPUT(Makefile src/Makefile)

PKG_PROG_PKG_CONFIG([0.25]) # check and set $PKG_CONFIG
# Checks for programs.
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_INSTALL
# Checks for libraries.

PKG_CHECK_MODULES([libusb], [libusb-1.0],
[ac_libusb_link=`$PKG_CONFIG --libs libusb-1.0`],
  [ac_libusb_link=;]
)

if test "x$ac_libusb_link" = x ; then
  AC_MSG_ERROR(["Can't Locate libusb-1.0"])
fi

AC_SUBST(LIBUSB_LINK, $ac_libusb_link)
# Checks for header files.
AC_CHECK_HEADERS([fcntl.h malloc.h stddef.h stdint.h stdlib.h string.h strings.h sys/param.h termios.h unistd.h])

PKG_CHECK_MODULES([libusb_headers], [libusb-1.0],
[ac_libusb_include=`$PKG_CONFIG --cflags libusb-1.0`],
  [ac_libusb_include=;]
)

if test "x$ac_libusb_include" = x ; then
  AC_MSG_ERROR(["Can't Locate libusb.h"])
fi

AC_SUBST(LIBUSB_INCLUDE, $ac_libusb_include)


# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset strcasecmp strdup strerror strrchr])

AC_OUTPUT
