/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id: acconfig.h,v 1.8 2003/04/30 18:03:14 grubba Exp $
*/

#ifndef GMP_MACHINE_H
#define GMP_MACHINE_H

@TOP@

/* Define if your <jerror.h> defines JERR_BAD_CROP_SPEC */
#undef HAVE_JERR_BAD_CROP_SPEC

@BOTTOM@

/* Define this if you have -ljpeg */
#undef HAVE_LIBJPEG

/* Define this if you don't have image transformation capabilities in jpeglib*/
#undef TRANSFORMS_NOT_SUPPORTED

/* Define this if your <jconfig.h> sets HAVE_BOOLEAN */
#undef HAVE_JCONFIG_H_HAVE_BOOLEAN

#endif
