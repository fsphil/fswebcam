
AC_INIT(fswebcam, 20110717, phil@sanslogic.co.uk)
AC_PROG_CC

AC_ARG_ENABLE(32bit-buffer,
	[  --enable-32bit-buffer   allow capturing of 2^32 frames],
	[BUFFER_BITS="32"
	AC_DEFINE([USE_32BIT_BUFFER], [1], [Allow capture of 2^32 frames])],
	[BUFFER_BITS="16"])

SRC_OBJS="src_test.o src_raw.o src_file.o"

dnl --- Test if V4L1 should be disabled. ---
AC_ARG_ENABLE(v4l1,
	[  --disable-v4l1          disable V4L1 support],
	[HAVE_V4L1="no"],
	[AC_DEFINE([HAVE_V4L1], [1], [V4L1 source support.])
	SRC_OBJS="$SRC_OBJS src_v4l1.o"
	HAVE_V4L1="yes"]
)

dnl --- Test if V4L2 should be disabled. ---

AC_ARG_ENABLE(v4l2,
	[  --disable-v4l2          disable V4L2 support],
	[HAVE_V4L2="no"],
	[AC_DEFINE([HAVE_V4L2], [1], [V4L2 source support.])
	SRC_OBJS="$SRC_OBJS src_v4l2.o"
	HAVE_V4L2="yes"]
)

AC_SUBST(SRC_OBJS)

AC_FUNC_MMAP

AC_CHECK_LIB(gd, gdImageCreateTrueColor, HAVE_GD="yes",,)
if test "$HAVE_GD" != "yes"; then
	AC_MSG_ERROR([GD graphics library not found])
else
	LDFLAGS="-lgd $LDFLAGS"
fi

AC_CHECK_LIB(gd, gdImageStringFT, HAVE_FT2="yes",,)
if test "$HAVE_FT2" != "yes"; then
	AC_MSG_ERROR([GD does not have FreeType2 font support!])
fi

AC_CHECK_LIB(gd, gdImageJpeg, HAVE_JPEG="yes",,)
if test "$HAVE_JPEG" != "yes"; then
	AC_MSG_ERROR([GD does not have JPEG support!])
fi

AC_CHECK_LIB(gd, gdImagePngEx, HAVE_PNG="yes",,)
if test "$HAVE_PNG" != "yes"; then
	AC_MSG_ERROR([GD does not have PNG support!])
fi

# The V4Lx headers are now included along with the source.

#AC_CHECK_HEADER(linux/videodev.h, HAVE_V4L1="yes",,)
#if test "$HAVE_V4L1" == "yes"; then
#	AC_DEFINE([HAVE_V4L1], [1], [V4L1 source support.])
#fi

#AC_CHECK_HEADERS([linux/videodev2.h], HAVE_V4L2="yes",,[
##include <sys/types.h>
##include <sys/time.h>
##include <linux/videodev.h>
#])
#if test "$HAVE_V4L2" == "yes"; then
#	AC_DEFINE([HAVE_V4L2], [1], [V4L2 source support.])
#fi

AC_MSG_RESULT([
   Buffer type ........... $BUFFER_BITS bit
   PNG support ........... $HAVE_PNG
   JPEG support .......... $HAVE_JPEG
   Freetype 2.x support .. $HAVE_FT2
   V4L1 support .......... $HAVE_V4L1
   V4L2 support .......... $HAVE_V4L2
])

AC_CONFIG_FILES(Makefile)
AC_CONFIG_HEADER(config.h)
AC_OUTPUT

