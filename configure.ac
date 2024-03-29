m4_define([MAJOR], 1)
m4_define([MINOR], 0)
m4_define([MICRO], 2)

dnl Process this file with autoconf to produce a configure script.
AC_INIT([xbmc-pvr-livestreams], [MAJOR.MINOR.MICRO], [Zoltan Csizmadia])

AC_CONFIG_AUX_DIR(autotools)
PREFIX_DEFAULT="/usr"
AC_PREFIX_DEFAULT($PREFIX_DEFAULT)

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

dnl Setup for automake
AM_INIT_AUTOMAKE

dnl Check for tools

AC_LIBTOOL_WIN32_DLL
AM_PROG_LIBTOOL
AC_PROG_MAKE_SET
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_INSTALL
AC_EXEEXT

dnl Check for zlib
zlib_found=yes
ZLIB_LIBS=
AC_CHECK_HEADER(zlib.h,,[zlib_found="no"])
if test x$zlib_found = xyes; then
        AC_SEARCH_LIBS(compress, z, [AC_DEFINE([HAVE_ZLIB], 1, [have zlib compression library installed])])
        ZLIB_LIBS="-lz"
        AC_SUBST(ZLIB_LIBS)
fi

AC_SUBST(VERSIONEXT)

ISMINGW32=false
ADDONNAME=XBMC_LiveStreams_PVR.pvr
SOEXT=so
ISAPPLE=false

case $host in
  *-*-cygwin* | *-*-mingw32*)
    LDFLAGS+="-no-undefined -no-cygwin"
    ISMINGW32=true
    ADDONNAME=XBMC_LiveStreams_PVR_win32.dll
    SOEXT=dll
    ;;
  arm*-*-linux-gnu*)
    AC_SUBST(ARCH_DEFINES, "-D_ARMEL")
    ;;
  powerpc-*-linux-gnu*)
    AC_SUBST(ARCH_DEFINES, "-D_POWERPC")
    ;;
  powerpc64-*-linux-gnu*)
    AC_SUBST(ARCH_DEFINES, "-D_POWERPC64")
    ;;
  *-apple-*)
    SOEXT=dylib
    ISAPPLE=true
    ;;
  *-androideabi)
    AC_SUBST(ARCH_DEFINES, "-DANDROID")
    ADDONNAME=libXBMC_LiveStreams_PVR.so
    ;;
esac

if test "x${libdir}" != 'x${exec_prefix}/lib'; then
  LIBDIR=${libdir}
elif test "$prefix" = "NONE"; then
  LIBDIR=$PREFIX_DEFAULT/lib/xbmc/addons
else
  LIBDIR=$prefix/lib/xbmc/addons
fi
AC_SUBST(LIBDIR)

if test "x${datadir}" != 'x${datarootdir}' && test "x${datarootdir}" = 'x${prefix}/share'; then
  DATADIR=${datadir}
elif test "$prefix" = "NONE"; then
  DATADIR=$PREFIX_DEFAULT/share/xbmc/addons
else
  DATADIR=$prefix/share/xbmc/addons 
fi
AC_SUBST(DATADIR)

AM_CONDITIONAL([MINGW32], $ISMINGW32)
AM_CONDITIONAL([APPLE], $ISAPPLE)
AC_SUBST(ADDONNAME)
AC_SUBST(SOEXT)

# Finally create all the generated files
AC_OUTPUT([Makefile
           addons/Makefile
           addons/pvr.livestreams/Makefile
           addons/pvr.livestreams/addon.xml
           src/Makefile
           src/livestreamspvr/Makefile])
