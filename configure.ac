AC_INIT(restream,0.01)
AM_INIT_AUTOMAKE([subdir-objects foreign])
AM_MAINTAINER_MODE([enable])
AC_PROG_CC
AC_PROG_CXX
AC_GNU_SOURCE
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([restream.c])
AC_CANONICAL_HOST
AC_CONFIG_MACRO_DIR([m4])

##############################################################################
###  Check pkg-config  - Required.  Needed to get lib paths/info
##############################################################################
AC_CHECK_PROG([PKGCONFIG],[pkg-config],[yes],[no])
AS_IF([test "${PKGCONFIG}" = "no" ],[
    AC_MSG_ERROR([Required package 'pkg-config' not found.])
  ]
)

##############################################################################
###  Check pthread
##############################################################################
AC_CHECK_HEADERS(pthread.h,,AC_MSG_ERROR([pthread is required.]))
TEMP_CFLAGS="-D_THREAD_SAFE"
LIBS="$LIBS -pthread "

##############################################################################
###  Check for ffmpeg
##############################################################################
FFMPEG_VER="--"
AC_ARG_WITH([ffmpeg],
  AS_HELP_STRING([--with-ffmpeg[=DIR]],[Build with FFMPEG support]),
  [FFMPEG=$withval],
  [FFMPEG="yes"]
)

AS_IF([test "${FFMPEG}" = "no"], [
    AC_MSG_CHECKING(for FFmpeg)
    AC_MSG_ERROR([Required ffmpeg packages were not found.])
  ],[
    AC_MSG_CHECKING(FFmpeg pkg-config path)
    TEMP_PATH=$PKG_CONFIG_PATH
    AS_IF([test "${FFMPEG}" != "yes"], [
        PKG_CONFIG_PATH=${FFMPEG}/lib/pkgconfig:$PKG_CONFIG_PATH
        FFMPEG="yes"
      ]
    )
    export PKG_CONFIG_PATH
    AC_MSG_RESULT($PKG_CONFIG_PATH)

    FFMPEG_DEPS="libavdevice libavformat libavcodec libswresample libswscale libavutil"
    AC_MSG_CHECKING(for FFmpeg)
    AS_IF([pkg-config $FFMPEG_DEPS], [
        FFMPEG_VER=`pkg-config --modversion libavformat`
        TEMP_CFLAGS="$TEMP_CFLAGS "`pkg-config --cflags $FFMPEG_DEPS`
        TEMP_LIBS="$TEMP_LIBS "`pkg-config --libs $FFMPEG_DEPS`
        AC_DEFINE([HAVE_FFMPEG], [1], [Define to 1 if FFMPEG is around])
        AC_MSG_RESULT(yes)
      ],[
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([Required ffmpeg packages were not found.])
      ]
    )
    PKG_CONFIG_PATH=$TEMP_PATH
    export PKG_CONFIG_PATH
  ]
)

AC_SUBST(AM_CFLAGS, "$TEMP_CFLAGS")
LIBS="$LIBS $TEMP_LIBS"
AC_SUBST(AM_LDFLAGS, "$TEMP_LDFLAGS")

AC_CONFIG_FILES([Makefile])

AC_OUTPUT

##############################################################################
###  Report results to user
##############################################################################
echo ""
echo "   **************************"
echo "      Configure status       "
echo "      ${PACKAGE_NAME} ${PACKAGE_VERSION}"
echo "   **************************"
echo
echo "CFLAGS: $TEMP_CFLAGS $CFLAGS"
echo
echo "LIBS: $LIBS"
echo
echo "LDFLAGS: $TEMP_LDFLAGS $LDFLAGS"
echo
echo  "Install prefix:       $prefix"
echo
