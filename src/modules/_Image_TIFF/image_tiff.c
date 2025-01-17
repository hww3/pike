/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id$
*/

#include "global.h"
#include "module.h"
#include "config.h"

/*! @module Image
 */

/*! @module TIFF
 */

#ifdef HAVE_WORKING_LIBTIFF

#include "global.h"
#include "machine.h"
#include "pike_macros.h"
#include "object.h"
#include "constants.h"
#include "interpret.h"
#include "builtin_functions.h"
#include "svalue.h"
#include "threads.h"
#include "array.h"
#include "mapping.h"
#include "module_support.h"
#include "pike_error.h"
#include "stralloc.h"
#include "operators.h"
#include "../Image/image.h"

#ifdef INLINE
#undef INLINE
#endif
#include <tiff.h>
#ifdef HAVE_TIFFIOP_H
#include <tiffiop.h>
#endif
#include <tiffio.h>


#define sp Pike_sp

#ifdef DYNAMIC_MODULE
static struct program *image_program=NULL;
static struct program *image_colortable_program=NULL;
#else
/* The image module is probably linked static too... */
extern struct program *image_program; 
extern struct program *image_colortable_program;
#endif

struct buffer 
{
  char *str;
  ptrdiff_t len;	/* Buffer length. */
  ptrdiff_t offset;
  ptrdiff_t real_len;	/* File length. */
  int extendable;
} *buffer_handle; 

struct imagealpha
{
  struct object *img;
  struct object *alpha;
};

#define INITIAL_WRITE_BUFFER_SIZE 8192

#if 0
# define TRACE(X,Y,Z,Q) fprintf(stderr, X,Y,Z,Q)
#else
# define TRACE(X,Y,Z,Q)
#endif

/* Buffer for holding errors generated by TIFFError().
 *
 * NOTE: This will need to be changed if this module ever
 *       releases the interpreter lock.
 */
#define TIFF_ERROR_BUF_SIZE	4096
static char last_tiff_error[TIFF_ERROR_BUF_SIZE];

static void increase_buffer_size( struct buffer * buffer )
{
  char *new_d;
  TRACE("increase_buffer_size(%p<%d,%d>)\n", buffer,buffer->len,
        buffer->offset);
  if(!buffer->extendable)
    Pike_error("Extending non-extendable buffer!\n");
  if(buffer->len > 1024*1024*400)
    Pike_error("Too large buffer (temprary error..)\n");
  if(!buffer->len) buffer->len = INITIAL_WRITE_BUFFER_SIZE;

  /* FIXME: According to DMALLOC this leaks.
   *
   * NOTE: buffer->str seems to point into Pike strings sometimes,
   * in which case realloc() is wrong.
   */
  new_d = realloc( buffer->str, buffer->len*2 );
  if(!new_d) Pike_error("Realloc (%ld->%ld) failed!\n",
		   DO_NOT_WARN((long)buffer->len),
		   DO_NOT_WARN((long)buffer->len*2));
  MEMSET(new_d+buffer->len, 0, buffer->len);
  buffer->str = new_d;
  buffer->len *= 2;
}

/* Complies with the TIFFReadWriteProc API */
static tsize_t read_buffer( thandle_t bh, tdata_t d, tsize_t len )
{
  struct buffer *buffer_handle = (struct buffer *)bh;
  char *data = (char *)d;
  tsize_t avail;

  TRACE("read_buffer(%p,%p,%d)\n",
	buffer_handle, data, len);
  avail = buffer_handle->real_len-buffer_handle->offset;
  TRACE("\toffset:%d real_len:%d avail:%d\n",
	buffer_handle->offset, buffer_handle->real_len, avail);
  if(!avail) return -1;
  len = MINIMUM(avail, len);
  MEMCPY(data, buffer_handle->str+buffer_handle->offset, len);
  buffer_handle->offset += len;
  return len;
}

/* Complies with the TIFFReadWriteProc API */
static tsize_t write_buffer(thandle_t bh, tdata_t d, tsize_t len)
{
  struct buffer *buffer_handle = (struct buffer *)bh;
  char *data = (char *)d;

  TRACE("write_buffer(%p,%p,%d)", buffer_handle, data, len);
  TRACE(" offset:%d(%d)\n", buffer_handle->offset, buffer_handle->len, 0);
  while((buffer_handle->len-buffer_handle->offset) < len)
  {
    TRACE("Too small buffer: %d/%d vs %d\n",
          buffer_handle->len, buffer_handle->offset,
          len);
    increase_buffer_size( buffer_handle );
  }
  MEMCPY( buffer_handle->str+buffer_handle->offset, data, len );
  buffer_handle->offset += len;
  if(buffer_handle->offset > buffer_handle->real_len)
    buffer_handle->real_len = buffer_handle->offset;
  return len;
}

/* Complies with the TIFFSeekProc API. */
static toff_t seek_buffer(thandle_t bh, toff_t seek, int seek_type )
{
  struct buffer *buffer_handle = (struct buffer *)bh;

  TRACE("seek_buffer(%p,%d,%s)\n", buffer_handle,seek,
        (seek_type == SEEK_CUR?"SEEK_CUR":
         (seek_type==SEEK_SET?"SEEK_SET":"SEEK_END")));
  switch(seek_type)
  {
   case SEEK_CUR:
     while(buffer_handle->len < (ptrdiff_t)(seek+buffer_handle->offset))
       increase_buffer_size( buffer_handle );
     buffer_handle->offset += seek;
     if(buffer_handle->offset > buffer_handle->real_len)
       buffer_handle->real_len = buffer_handle->offset;
     break;
   case SEEK_SET:
     while(buffer_handle->len < (ptrdiff_t)seek)
       increase_buffer_size( buffer_handle );
     buffer_handle->offset = seek;
     if(buffer_handle->offset > buffer_handle->real_len)
       buffer_handle->real_len = buffer_handle->offset;
     break;
   case SEEK_END:
     if (seek > 0) {
       while (buffer_handle->real_len + ((ptrdiff_t)seek) >=
	      buffer_handle->len) {
	 increase_buffer_size(buffer_handle);
       }
     }
     buffer_handle->offset = buffer_handle->real_len + seek;
     if (buffer_handle->offset < 0) {
       buffer_handle->offset = 0;
     }
     break;
  }
  TRACE("Current offset: %ld\n", (long)buffer_handle->offset, 0, 0);
  return buffer_handle->offset;
}

/* Complies with the TIFFCloseProc API. */
static int close_buffer(thandle_t bh)
{
  struct buffer *buffer_handle = (struct buffer *)bh;

  TRACE("close_buffer(%p)\n",buffer_handle,0,0);
  return 0;
}

/* Complies with the TIFFSizeProc API. */
static toff_t size_buffer(thandle_t bh)
{
  struct buffer *buffer_handle = (struct buffer *)bh;

  TRACE("size_buffer(%p)\n",buffer_handle,0,0);
  return buffer_handle->len;
}

/* Complies with the TIFFMapFileProc API. */
static int map_buffer(thandle_t bh, tdata_t *r, toff_t *len )
{
  struct buffer *buffer_handle = (struct buffer *)bh;
  void **res = (void **)r;

  TRACE("map_buffer(%p,%p,%p)\n", buffer_handle, res, len);
  *res = buffer_handle->str;
  *len = buffer_handle->len;
  return 0;
}

/* Complies with the TIFFUnmapFileProc API. */
static void unmap_buffer(thandle_t bh, tdata_t p, toff_t len)
{
  struct buffer *buffer_handle = (struct buffer *)bh;
  void *ptr = (void *)p;

  TRACE("unmap_buffer(%p, %p, %d)\n", buffer_handle, ptr, len);
}

struct options
{
  int compression;
  char *name;
  char *comment;
  float xdpy;
  float ydpy;
};

static int default_tiff_compression = 0;	/* Undefined value */
static const int default_tiff_compressions[] = {
#ifdef COMPRESSION_LZW
  COMPRESSION_LZW,
#endif
#ifdef COMPRESSION_DEFLATE
  COMPRESSION_DEFLATE,
#endif
#ifdef COMPRESSION_ADOBE_DEFLATE
  COMPRESSION_ADOBE_DEFLATE,
#endif
#ifdef COMPRESSION_PACKBITS
  COMPRESSION_PACKBITS,
#endif
#ifdef COMPRESSION_NEXT
  COMPRESSION_NEXT,
#endif
#ifdef COMPRESSION_NEXT
  COMPRESSION_CCITTRLE,
#endif
  COMPRESSION_NONE,
};

void low_image_tiff_encode( struct buffer *buf, 
                            struct imagealpha *img,
                            struct options *opts)
{
  struct image *i, *a=0;
  int spp = 3;
  char *buffer;
  int n;
  ONERROR tmp;
  TIFF *tif;
  rgb_group *is, *as = NULL;
  int x, y;
  char *b;

  i = ((struct image *)get_storage(img->img,image_program));

  if(!i)
    Pike_error("Image is not an image object.\n");

  if(img->alpha)
  {
    spp++;
    a = ((struct image *)get_storage(img->alpha,image_program));
    if(!a)
      Pike_error("Alpha is not an image object.\n");
    if(i->xsize != a->xsize ||
       i->ysize != a->ysize)
      Pike_error("Image and alpha objects are not equally sized!\n");
  }

  buffer = xalloc( spp * i->xsize  );

  /* Workaround for the patently stupid way the crippling of
   * the LZW has been done.
   */
  n = 0;

 retry:
  tif = TIFFClientOpen( "memoryfile", "w", (thandle_t) buf,
			read_buffer, write_buffer,
			seek_buffer, close_buffer,
			size_buffer, map_buffer,
			unmap_buffer );
  if(!tif) {
    free(buffer);
    Pike_error("\"open\" of TIF file failed: %s\n", last_tiff_error);
  }

  SET_ONERROR(tmp, TIFFClose, tif);
  
  TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, (uint32)i->xsize);
  TIFFSetField(tif, TIFFTAG_IMAGELENGTH, (uint32)i->ysize);
  TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, (uint16)8);
  TIFFSetField(tif, TIFFTAG_ORIENTATION, (uint16)ORIENTATION_TOPLEFT);
  if(img->alpha)
  {
    uint16 val[1];
    val[0] = EXTRASAMPLE_ASSOCALPHA;
    TIFFSetField (tif, TIFFTAG_EXTRASAMPLES, (uint16)1, val);
    as = a->img;
  }
  TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, (uint16)PHOTOMETRIC_RGB);
  TIFFSetField(tif, TIFFTAG_FILLORDER, (uint16)FILLORDER_MSB2LSB);
  if(opts->name)
    TIFFSetField(tif, TIFFTAG_DOCUMENTNAME, (char *)opts->name);
  TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, (uint16)spp);
  TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP,
	       (uint32)MAXIMUM(8192/i->xsize/spp,1));
  TIFFSetField(tif, TIFFTAG_PLANARCONFIG, (uint16)PLANARCONFIG_CONTIG);
  TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, (uint16)RESUNIT_INCH);
  TIFFSetField(tif, TIFFTAG_XRESOLUTION, (float)opts->xdpy);
  TIFFSetField(tif, TIFFTAG_YRESOLUTION, (float)opts->ydpy);
  if(opts->comment)
    TIFFSetField(tif, TIFFTAG_IMAGEDESCRIPTION, (char *)opts->comment);

  /* Not a defined value.
   * i.e. the caller hasn't specified what compression to use.
   *
   * And we haven't checked what the library supports yet.
   */
  if (!opts->compression &&
      !(opts->compression = default_tiff_compression)) {
    TIFFSetField(tif, TIFFTAG_COMPRESSION, default_tiff_compressions[n]);
#ifdef COMPRESSION_LZW
    if(default_tiff_compressions[n] == COMPRESSION_LZW)
      TIFFSetField (tif, TIFFTAG_PREDICTOR, (uint16)2);
#endif
  } else {
    TIFFSetField(tif, TIFFTAG_COMPRESSION, opts->compression);
#ifdef COMPRESSION_LZW
    if(opts->compression == COMPRESSION_LZW)
      TIFFSetField (tif, TIFFTAG_PREDICTOR, (uint16)2);
#endif
  }
      
  b = buffer;
  is = i->img;

  for (y = 0; y < i->ysize; y++)
  {
    char *b = buffer;
    for(x=0; x<i->xsize; x++)
    {
      *(b++)=is->r;
      *(b++)=is->g;
      *(b++)=(is++)->b;
      if(as)
      {
	*(b++)=(as->r + as->g*2 + as->b)/4;
	as++;
      }
    }
    if(TIFFWriteScanline(tif, buffer, y, 0) < 0)
    {
      TRACE("TIFFWriteScanLine(%p, %p, %d, 0) failed:\n",
	    tif, buffer, y);
      TRACE("\terror:\"%s\", n:%d, nelem:%d\n",
	    last_tiff_error, n, NELEM(default_tiff_compressions));

      if (!y && (!opts->compression) &&
	  (n != (NELEM(default_tiff_compressions)-1))) {
	/* Probably a crippled libtiff.
	 *
	 * Try again with the next codec.
	 */
	CALL_AND_UNSET_ONERROR(tmp);

	seek_buffer((thandle_t) buf, 0, SEEK_SET);
	buf->real_len = 0;	/* Truncate file. */
	n++;
	goto retry;
      }
      free(buffer);
      Pike_error("TIFFWriteScanline returned error on line %d: %s(0x%04x)\n",
		 y, last_tiff_error,
		 opts->compression?default_tiff_compressions[n]:
		 opts->compression);
    }
  }

  TIFFFlushData (tif);
  CALL_AND_UNSET_ONERROR(tmp);

  free(buffer);
  if (!opts->compression) {
    default_tiff_compression = default_tiff_compressions[n];
  }
}

static const char *photoNames[] = {
    "min-is-white",				/* PHOTOMETRIC_MINISWHITE */
    "min-is-black",				/* PHOTOMETRIC_MINISBLACK */
    "RGB color",				/* PHOTOMETRIC_RGB */
    "palette color (RGB from colormap)",	/* PHOTOMETRIC_PALETTE */
    "transparency mask",			/* PHOTOMETRIC_MASK */
    "separated",				/* PHOTOMETRIC_SEPARATED */
    "YCbCr",					/* PHOTOMETRIC_YCBCR */
    "7 (0x7)",
    "CIE L*a*b*",				/* PHOTOMETRIC_CIELAB */
};

void low_image_tiff_decode( struct buffer *buf, 
                            struct imagealpha *res,
                            int image_only)
{
  TIFF *tif;
  unsigned int i;
  uint32 w, h, *raster,  *s;
  rgb_group *di, *da=NULL;
  tif = TIFFClientOpen("memoryfile", "r", (thandle_t) buf,
		       read_buffer, write_buffer,
		       seek_buffer, close_buffer,
		       size_buffer, map_buffer,
		       unmap_buffer);
  if(!tif)
    Pike_error("Failed to 'open' tiff image: %s\n", last_tiff_error);

  TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
  TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
  s = raster = (uint32 *)_TIFFmalloc(w*h*sizeof(uint32));
  if (raster == NULL) {
    TIFFClose (tif);
    Pike_error("Malloc failed to allocate buffer for %ldx%ld image\n",
	  (long)w, (long)h);
  }

  if(!TIFFReadRGBAImage(tif, w, h, raster, 0)) {
    TIFFClose (tif);
    _TIFFfree (raster);
    Pike_error("Failed to read TIFF data: %s\n", last_tiff_error);
  }


  push_int(w);
  push_int(h);
  res->img = clone_object(image_program, 2);
  if(!image_only)
  {
    push_int(w);
    push_int(h);
    res->alpha = clone_object(image_program, 2);
    da = ((struct image *)get_storage(res->alpha,image_program))->img;
  }
  di = ((struct image *)get_storage(res->img,image_program))->img;
  
  for(i=0; i<h*w; i++)
  {
    uint32 p = *s;
    di->r = DO_NOT_WARN((unsigned char)(p & 255));
    di->g = DO_NOT_WARN((unsigned char)((p>>8) & 255));
    (di++)->b = DO_NOT_WARN((unsigned char)((p>>16) & 255));
    if(!image_only) 
    {
      da->r = da->g = da->b = DO_NOT_WARN((unsigned char)((p>>24) & 255));
      da++;
    }
    s++;
  }
  _TIFFfree(raster);
  if(!image_only)
  {
    apply( res->alpha, "mirrory", 0);
    free_object(res->alpha);
    res->alpha = sp[-1].u.object;
    sp--;
    dmalloc_touch_svalue(sp);
  }
  apply( res->img, "mirrory", 0);
  free_object(res->img);
  res->img = sp[-1].u.object;
  sp--;
  dmalloc_touch_svalue(sp);
  if(!image_only)
  {
#ifdef HAVE_TIFFIOP_H
    char *tmp;
    TIFFDirectory *td = &tif->tif_dir;
    if (TIFFFieldSet(tif,FIELD_RESOLUTION)) 
    {
      push_constant_text( "xres" );   push_float( td->td_xresolution );
      push_constant_text( "yres" );   push_float( td->td_yresolution );
      push_constant_text( "unit" );  
      if (TIFFFieldSet(tif,FIELD_RESOLUTIONUNIT)) 
      {
        switch(td->td_resolutionunit)
        {
         case RESUNIT_NONE:
           push_constant_text("unitless");
           break;
         case RESUNIT_INCH:
           push_constant_text("pixels/inch");
           push_constant_text( "xdpy" );   push_float( td->td_xresolution );
           push_constant_text( "ydpy" );   push_float( td->td_yresolution );
           break;
         case RESUNIT_CENTIMETER:
           push_constant_text("pixels/cm");
           push_constant_text( "xdpy" );   push_float( td->td_xresolution/2.5 );
           push_constant_text( "ydpy" );   push_float( td->td_yresolution/2.5 );
           break;
        } 
      } else
        push_constant_text( "unitless" );  
    }
    if (TIFFFieldSet(tif,FIELD_POSITION))
    {
      push_constant_text("xposition"); push_int(td->td_xposition);
      push_constant_text("yposition"); push_int(td->td_yposition);
    }
    if (TIFFFieldSet(tif,FIELD_PHOTOMETRIC)) 
    {
      push_text("photometric");
      if (td->td_photometric < (sizeof (photoNames) / sizeof (photoNames[0])))
        push_text( photoNames[td->td_photometric] );
      else 
      {
        switch (td->td_photometric) {
#ifdef PHOTOMETRIC_LOGL
         case PHOTOMETRIC_LOGL:
           push_text("CIE Log2(L)");
           break;
#endif /* PHOTOMETRIC_LOGL */
#ifdef PHOTOMETRIC_LOGLUV
         case PHOTOMETRIC_LOGLUV:
           push_text("CIE Log2(L) (u',v')");
           break;
#endif /* PHOTOMETRIC_LOGLUV */
         default:
           push_text("unkown");
           break;
        }
      }
    }

    if (TIFFFieldSet(tif,FIELD_EXTRASAMPLES) && td->td_extrasamples) 
    {
      push_text( "extra_samples" );
      for (i = 0; i < td->td_extrasamples; i++) 
      {
        switch (td->td_sampleinfo[i]) 
        {
         case EXTRASAMPLE_UNSPECIFIED:
           push_text("unspecified");
           break;
         case EXTRASAMPLE_ASSOCALPHA:
           push_text("assoc-alpha");
           break;
         case EXTRASAMPLE_UNASSALPHA:
           push_text("unassoc-alpha");
           break;
         default:
           push_int( td->td_sampleinfo[i] );
           break;
        }
      }
      f_aggregate( td->td_extrasamples );
    }

    if (TIFFFieldSet(tif,FIELD_THRESHHOLDING)) 
    {
      push_text( "threshholding" );
      switch (td->td_threshholding) {
       case THRESHHOLD_BILEVEL:
         push_text( "bilevel art scan" );
         break;
       case THRESHHOLD_HALFTONE:
         push_text( "halftone or dithered scan" );
         break;
       case THRESHHOLD_ERRORDIFFUSE:
         push_text( "error diffused" );
         break;
       default:
         push_text( "unknown" );
         break;
      }
    }
    if (TIFFFieldSet(tif,FIELD_HALFTONEHINTS))
    {
      push_text( "halftone_hints" );
      push_int(td->td_halftonehints[0]);
      push_int(td->td_halftonehints[1]);
      f_aggregate(2);
    }

    if(td->td_artist)
    {
      push_text("artist");
      push_text(td->td_artist);
    }
    if(td->td_datetime)
    {
      push_text("datetime");
      push_text(td->td_datetime);
    }
    if(td->td_hostcomputer)
    {
      push_text("hostcomputer");
      push_text(td->td_hostcomputer);
    }
    if(td->td_software)
    {
      push_text("software");
      push_text(td->td_software);
    }
    if(td->td_documentname)
    {
      push_text("name");
      push_text(td->td_documentname);
    }

    if(td->td_imagedescription)
    {
      push_text("comment");
      push_text(td->td_imagedescription);
    }

    if(td->td_make)
    {
      push_text("make");
      push_text(td->td_make);
    }

    if(td->td_model)
    {
      push_text("model");
      push_text(td->td_model);
    }

    if(td->td_pagename)
    {
      push_text("page_name");
      push_text(td->td_pagename);
    }

    if(TIFFFieldSet(tif,FIELD_PAGENUMBER))
    {
      push_text("page_number");
      push_int(td->td_pagenumber[0]);
      push_int(td->td_pagenumber[1]);
      f_aggregate(2);
    }

    if (TIFFFieldSet(tif,FIELD_COLORMAP)) 
    {
      int l,n = 1L<<td->td_bitspersample;
      push_text("colormap");
      for (l = 0; l < n; l++)
      {
        push_int( td->td_colormap[0][l] );
        push_int( td->td_colormap[1][l] );
        push_int( td->td_colormap[2][l] );
        f_aggregate(3);
      }
      f_aggregate(1L<<td->td_bitspersample);
      push_object(clone_object(image_colortable_program, 1 ));
    }
#ifdef COLORIMETRY_SUPPORT
    if (TIFFFieldSet(tif,FIELD_WHITEPOINT))
    {
      push_text("whitepoint");
      push_float(td->td_whitepoint[0]);
      push_float(td->td_whitepoint[1]);
      f_aggregate(2);
    }
    if (TIFFFieldSet(tif,FIELD_PRIMARYCHROMAS))
    {
      push_text("primary_chromaticities");
      for(i=0;i<6;i++)
        push_float(td->td_primarychromas[i]);
      f_aggregate(6);
    }
    if (TIFFFieldSet(tif,FIELD_REFBLACKWHITE)) 
    {
      push_text("reference_black_white");
      for (i = 0; i < td->td_samplesperpixel; i++)
      {
        push_float(td->td_refblackwhite[2*i+0]);
        push_float(td->td_refblackwhite[2*i+1]);
        f_aggregate(2);
      }
      f_aggregate(td->td_samplesperpixel);
    }
#endif
#endif
  }
  TIFFClose(tif);
}


static void image_tiff_decode( INT32 args )
{
  struct buffer buffer;
  struct imagealpha res;
  if(!args) 
    Pike_error("Too few arguments to Image.TIFF.decode()\n");

  if(sp[-args].type != T_STRING)
    Pike_error("Invalid argument 1 to Image.TIFF.decode()\n");

  buffer.str = sp[-args].u.string->str;
  buffer.len = buffer.real_len =sp[-args].u.string->len;
  buffer.extendable = 0;
  buffer.offset = 0;

  low_image_tiff_decode( &buffer, &res, 1 );
  pop_n_elems(args);
  push_object( res.img );
}

/*! @decl object decode(string data)
 *!   Decodes a TIFF image.
 *!
 *! @throws
 *!   Throws upon error in data.
 */

/*! @decl mapping(string:mixed) _decode(string data)
 *! Decodes a TIFF image to a mapping with at least the members image
 *! and alpha.
 *!
 *! @mapping
 *!   @member Image.Image "image"
 *!     The actual image.
 *!   @member Image.Image "alpha"
 *!     Image alpha channel.
 *!   @member float "xres"
 *!   @member float "yres"
 *!     Resolution
 *!   @member string "unit"
 *!     @string
 *!       @value "unitless"
 *!       @value "pixels/inch"
 *!       @value "pixels/cm"
 *!     @endstring
 *!   @member float "xdpy"
 *!   @member float "ydpy"
 *!     Resolution
 *!   @member int "xposition"
 *!   @member int "yposition"
 *!
 *!   @member string "photometric"
 *!     Type of photometric. Can be @expr{"unknown"@}.
 *!   @member array(int|string) "extra_samples"
 *!     Array elements are either integers or any of
 *!     @string
 *!       @value "unspecified"
 *!       @value "assoc-alpha"
 *!       @value "unassoc-alpha"
 *!     @endstring
 *!   @member string "threshholding"
 *!
 *!   @member array(int) "halftone_hints"
 *!     Array is always two elements.
 *!   @member string "artist"
 *!   @member string "datetime"
 *!   @member string "hostcomputer"
 *!   @member string "software"
 *!   @member string "name"
 *!   @member string "comment"
 *!   @member string "make"
 *!   @member string "model"
 *!   @member string "page_name"
 *!
 *!   @member array(int) "page_number"
 *!     Array is always two elements.
 *!   @member array(array(int)) "colormap"
 *!     Array of array of RGB values.
 *!   @member array(int) "whitepoint"
 *!     Array is always two elements.
 *!   @member array(float) "primary_chromaticities"
 *!     Array is always six elements.
 *!   @member array(array(float)) "reference_black_white"
 *!     Array of array of two elements.
 *! @endmapping
 *!
 *! @throws
 *!   Throws upon error in data.
 */

static void image_tiff__decode( INT32 args )
{
  struct buffer buffer;
  struct imagealpha res;
  struct svalue *osp=sp;
  if(!args) 
    Pike_error("Too few arguments to Image.TIFF.decode()\n");
  if(sp[-args].type != T_STRING)
    Pike_error("Invalid argument 1 to Image.TIFF.decode()\n");

  MEMSET(&res, 0, sizeof(res));
  buffer.str = sp[-args].u.string->str;
  buffer.len = buffer.real_len = sp[-args].u.string->len;
  buffer.extendable = 0;
  buffer.offset = 0;

  low_image_tiff_decode( &buffer, &res, 0 );
  push_constant_text( "image" );
  push_object( res.img );
  push_constant_text( "alpha" );
  push_object( res.alpha );
  f_aggregate_mapping( sp-osp );
  {
    struct mapping *tmp = sp[-1].u.mapping;
    sp--;
    dmalloc_touch_svalue(sp);
    pop_n_elems(args);
    push_mapping( tmp );
  }
}

static struct pike_string *opt_compression, *opt_alpha, *opt_dpy, *opt_xdpy;
static struct pike_string *opt_ydpy, *opt_name, *opt_comment;

static int parameter_int(struct svalue *map,struct pike_string *what,
                         INT32 *p)
{
   struct svalue *v;
   v=low_mapping_string_lookup(map->u.mapping,what);
   if (!v || v->type!=T_INT) return 0;
   *p=v->u.integer;
   return 1;
}

static int parameter_string(struct svalue *map,struct pike_string *what,
                            char **p)
{
   struct svalue *v;
   v=low_mapping_string_lookup(map->u.mapping,what);
   if (!v || v->type!=T_STRING) return 0;
   if(v->u.string->size_shift) return 0;
   *p=(char *)v->u.string->str;
   return 1;
}

static int parameter_float(struct svalue *map,struct pike_string *what,
                           float *p)
{
   struct svalue *v;
   v=low_mapping_string_lookup(map->u.mapping,what);
   if (!v || v->type!=T_FLOAT) return 0;
   *p=v->u.float_number;
   return 1;
}

static int parameter_object(struct svalue *map,struct pike_string *what,
                            struct object **p)
{
   struct svalue *v;
   v=low_mapping_string_lookup(map->u.mapping,what);
   if (!v || v->type!=T_OBJECT) return 0;
   *p=v->u.object;
   return 1;
}


/*! @decl string encode(object image)
 *! @decl string encode(object image, mapping options)
 *! @decl string _encode(object image)
 *! @decl string _encode(object image, mapping options)
 *! Encode an image object into a TIFF file. [encode] and @[_encode]
 *! are identical.
 *!
 *! The @[options] argument may be a mapping containing zero or more
 *! encoding options. See @[_decode].
 *!
 *! @example
 *!   Image.TIFF.encode(img, ([
 *!	    "compression":Image.TIFF.COMPRESSION_LZW,
 *!	    "name":"an image name",
 *!	    "comment":"an image comment",
 *!	    "alpha":An alpha channel,
 *!	    "dpy":Dots per inch (as a float),
 *!	    "xdpy":Horizontal dots per inch (as a float),
 *!	    "ydpy":Vertical dots per inch (as a float),
 *!   ]));
 */
static void image_tiff_encode( INT32 args )
{
  struct imagealpha a;
  struct buffer b;
  struct options c;
  ONERROR onerr;

  a.alpha = 0;
  get_all_args( "Image.TIFF.encode", args, "%o", &a.img );


  MEMSET(&c, 0, sizeof(c));
  c.xdpy = 150.0;
  c.ydpy = 150.0;
  c.compression = 0;	/* Not a defined value. */

  if(args > 1)
  {
    float dpy;
    if(sp[-args+1].type != T_MAPPING)
      Pike_error("Invalid argument 2 to Image.TIFF.encode. Expected mapping.\n");
    parameter_int( sp-args+1, opt_compression, &c.compression );
    if(parameter_float( sp-args+1, opt_dpy, &dpy ))
      c.xdpy = c.ydpy = dpy;
    parameter_float( sp-args+1, opt_xdpy, &c.xdpy );
    parameter_float( sp-args+1, opt_ydpy, &c.ydpy );
    parameter_string( sp-args+1, opt_name, &c.name );
    parameter_string( sp-args+1, opt_comment, &c.comment );
    parameter_object( sp-args+1, opt_alpha, &a.alpha );
  }

  b.str = xalloc( b.len = INITIAL_WRITE_BUFFER_SIZE );
  b.real_len = 0;
  b.offset = 0;
  b.extendable = 1;
  SET_ONERROR( onerr, free, b.str );
  low_image_tiff_encode( &b, &a, &c );
  UNSET_ONERROR( onerr );
  push_string( make_shared_binary_string( b.str, b.real_len ) );
  free( (char *) b.str );
}


/* Complies with the TIFFErrorHandler API */
void my_tiff_warning_handler(const char *module, const char *fmt, va_list x){}
/* Complies with the TIFFErrorHandler API */
void my_tiff_error_handler(const char *module, const char *fmt, va_list x)
{
#ifdef HAVE_VSNPRINTF
  vsnprintf(last_tiff_error, TIFF_ERROR_BUF_SIZE-1, fmt, x);
#else /* !HAVE_VSNPRINTF */
  /* Sentinel that will be overwritten on buffer overflow. */
  last_tiff_error[TIFF_ERROR_BUF_SIZE-1] = '\0';

  VSPRINTF(last_tiff_error, fmt, x);

  if(last_tiff_error[TIFF_ERROR_BUF_SIZE-1])
    Pike_fatal("Buffer overflow in my_tiff_error_handler()\n");
#endif /* HAVE_VSNPRINTF */
}

#endif /* HAVE_WORKING_LIBTIFF */

/*! @decl constant COMPRESSION_NONE
 */

/*! @decl constant COMPRESSION_CCITTRLE
 */

/*! @decl constant COMPRESSION_CCITTFAX3
 */

/*! @decl constant COMPRESSION_CCITTFAX4
 */

/*! @decl constant COMPRESSION_CCITTRLEW
 */

/*! @decl constant COMPRESSION_LZW
 */

/*! @decl constant COMPRESSION_JPEG
 */

/*! @decl constant COMPRESSION_NEXT
 */

/*! @decl constant COMPRESSION_PACKBITS
 */

/*! @decl constant COMPRESSION_THUNDERSCAN
 */

/*! @endmodule
 */

/*! @endmodule
 */

PIKE_MODULE_INIT
{
#ifdef HAVE_WORKING_LIBTIFF
   opt_compression = 0;
#ifdef DYNAMIC_MODULE
   image_program = PIKE_MODULE_IMPORT(Image, image_program);
   image_colortable_program = PIKE_MODULE_IMPORT(Image,
						image_colortable_program);
   if(!image_program || !image_colortable_program) {
      yyerror("Could not load Image module.");
      return;
   }
#endif /* DYNAMIC_MODULE */

   TIFFSetWarningHandler(my_tiff_warning_handler);
   TIFFSetErrorHandler(my_tiff_error_handler);

   ADD_FUNCTION("decode",image_tiff_decode,tFunc(tStr,tObj),0);
   ADD_FUNCTION("_decode",image_tiff__decode,tFunc(tStr,tMapping),0);
   ADD_FUNCTION("encode",image_tiff_encode,
		tFunc(tObj tOr(tMapping,tVoid),tStr),0);
   ADD_FUNCTION("_encode",image_tiff_encode,
		tFunc(tObj tOr(tMapping,tVoid),tStr), 0);

   add_integer_constant( "COMPRESSION_NONE", COMPRESSION_NONE,0 );
#ifdef CCITT_SUPPORT
   add_integer_constant( "COMPRESSION_CCITTRLE", COMPRESSION_CCITTRLE,0);
   add_integer_constant( "COMPRESSION_CCITTFAX3", COMPRESSION_CCITTFAX3,0);
   add_integer_constant( "COMPRESSION_CCITTFAX4", COMPRESSION_CCITTFAX4,0);
   add_integer_constant( "COMPRESSION_CCITTRLEW", COMPRESSION_CCITTRLEW,0);
#endif
#ifdef COMPRESSION_LZW
   add_integer_constant( "COMPRESSION_LZW", COMPRESSION_LZW,0);
#endif
#ifdef JPEG_SUPPORT
   add_integer_constant( "COMPRESSION_JPEG", COMPRESSION_JPEG,0);
#endif
#ifdef NEXT_SUPPORT
   add_integer_constant( "COMPRESSION_NEXT", COMPRESSION_NEXT,0);
#endif
#ifdef COMPRESSION_PACKBITS
   add_integer_constant( "COMPRESSION_PACKBITS", COMPRESSION_PACKBITS,0);
#endif
#ifdef THUNDERSCAN_SUPPORT
  add_integer_constant( "COMPRESSION_THUNDERSCAN",COMPRESSION_THUNDERSCAN,0);
#endif

   opt_compression = make_shared_string( "compression" );
   opt_name = make_shared_string( "name" );
   opt_comment = make_shared_string( "comment" );
   opt_alpha = make_shared_string( "alpha" );
   opt_dpy = make_shared_string( "dpy" );
   opt_xdpy = make_shared_string( "xdpy" );
   opt_ydpy = make_shared_string( "ydpy" );
#endif /* HAVE_WORKING_LIBTIFF */
}

PIKE_MODULE_EXIT
{
#ifdef HAVE_WORKING_LIBTIFF
  if(!opt_compression) return;
  free_string(opt_compression);
  free_string(opt_name);
  free_string(opt_comment);
  free_string(opt_alpha);
  free_string(opt_dpy);
  free_string(opt_xdpy);
  free_string(opt_ydpy);
#endif /* HAVE_WORKING_LIBTIFF */
}
