#include "global.h"
#include "config.h"
/*
**! module Image
**! submodule TIFF
**!
*/







#ifdef HAVE_LIBTIFF
RCSID("$Id: image_tiff.c,v 1.9 1999/05/26 17:06:48 grubba Exp $");

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
#include "error.h"
#include "stralloc.h"
#include "../Image/image.h"

#ifdef INLINE
#undef INLINE
#endif
#include <tiff.h>
#ifdef HAVE_TIFFIOP_H
#include <tiffiop.h>
#endif
#include <tiffio.h>

#ifdef DYNAMIC_MODULE
static struct program *image_program=NULL;
static struct program *image_colortable_program=NULL;
#else
/* The image module is probably linked static too... */
extern struct program *image_program; 
extern struct program *image_colortable_program=NULL;
#endif

#ifndef MIN
#define MIN(X,Y) ((X)<(Y)?(X):(Y))
#endif

struct buffer 
{
  char *str;
  int len;
  int offset;
  int real_len;
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

static void increase_buffer_size( struct buffer * buffer )
{
  char *new_d;
  TRACE("increase_buffer_size(%p<%d,%d>)\n", buffer,buffer->len,
        buffer->offset);
  if(!buffer->extendable)
    error("Extending non-extendable buffer!\n");
  if(buffer->len > 1024*1024*400)
    error("Too large buffer (temprary error..)\n");
  if(!buffer->len) buffer->len = INITIAL_WRITE_BUFFER_SIZE;
  new_d = realloc( buffer->str, buffer->len*2 );
  if(!new_d) error("Realloc (%d->%d) failed!\n", buffer->len,buffer->len*2);
  MEMSET(new_d+buffer->len, 0, buffer->len);
  buffer->str = new_d;
  buffer->len *= 2;
}

static int read_buffer( struct buffer *buffer_handle, 
                        char *data, int len )
{
  int avail;
  TRACE("read_buffer(%p,%p,%d)\n", buffer_handle,data,len);
  avail = buffer_handle->len-buffer_handle->offset;
  if(!avail) return -1;
  MEMCPY( data, buffer_handle->str+buffer_handle->offset, MIN(avail,len) );
  buffer_handle->offset += MIN(avail,len);
  if(buffer_handle->offset > buffer_handle->real_len)
    buffer_handle->real_len = buffer_handle->offset;
  return MIN(avail,len);
}

static int write_buffer( struct buffer *buffer_handle, 
                         char *data, int len )
{
  TRACE("write_buffer(%p,%p,%d)\n", buffer_handle, data,len);
  while((buffer_handle->len-buffer_handle->offset) < len)
  {
    TRACE("Too small buffer: %d/%d vs %d\n",
          buffer_handle->len,buffer_handle->offset,
          len);
    increase_buffer_size( buffer_handle );
  }
  MEMCPY( buffer_handle->str+buffer_handle->offset, data, len );
  buffer_handle->offset += len;
  if(buffer_handle->offset > buffer_handle->real_len)
    buffer_handle->real_len = buffer_handle->offset;
  return len;
}

static int seek_buffer( struct buffer *buffer_handle, 
                        int seek, int seek_type )
{
  TRACE("seek_buffer(%p,%d,%s)\n", buffer_handle,seek,
        (seek_type == SEEK_CUR?"SEEK_CUR":
         (seek_type==SEEK_SET?"SEEK_SET":"SEEK_END")));
  switch(seek_type)
  {
   case SEEK_CUR:
     while(buffer_handle->len < seek+buffer_handle->offset)
       increase_buffer_size( buffer_handle );
     buffer_handle->offset += seek;
     if(buffer_handle->offset > buffer_handle->real_len)
       buffer_handle->real_len = buffer_handle->offset;
     break;
   case SEEK_SET:
     while(buffer_handle->len < seek)
       increase_buffer_size( buffer_handle );
     buffer_handle->offset = seek;
     if(buffer_handle->offset > buffer_handle->real_len)
       buffer_handle->real_len = buffer_handle->offset;
     break;
   case SEEK_END:
     buffer_handle->offset = buffer_handle->real_len-seek;
     break;
  }
  return buffer_handle->offset;
}

static int close_buffer( struct buffer *buffer_handle )
{
  TRACE("close_buffer(%p)\n",buffer_handle,0,0);
  return 0;
}

static int size_buffer( struct buffer *buffer_handle )
{
  TRACE("size_buffer(%p)\n",buffer_handle,0,0);
  return buffer_handle->len;
}


static int map_buffer( struct buffer *buffer_handle, void **res, int *len )
{
  TRACE("map_buffer(%p,%p,%p)\n",buffer_handle,res,len);
  *res = buffer_handle->str;
  *len = buffer_handle->len;
  return 0;
}

static int unmap_buffer( struct buffer *buffer_handle, void *p, int len)
{
  TRACE("unmap_buffer(%p,%p,%d)\n",buffer_handle,p,len);
  return 0;
}

struct options
{
  int compression;
  char *name;
  char *comment;
  float xdpy;
  float ydpy;
};

#define MAX(X,Y) ((X)>(Y)?(X):(Y))

void low_image_tiff_encode( struct buffer *buf, 
                            struct imagealpha *img,
                            struct options *opts)
{
  TIFF *tif;
  struct image *i, *a;
  int spp = 3;
  int x, y;
  char *buffer;
  rgb_group *is, *as = NULL;
  tif = TIFFClientOpen( "memoryfile", "w", buf,
                        (TIFFReadWriteProc)read_buffer,
			(TIFFReadWriteProc)write_buffer,
                        (TIFFSeekProc)seek_buffer,
			(TIFFCloseProc)close_buffer,
                        (TIFFSizeProc)size_buffer,
			(TIFFMapFileProc)map_buffer,
                        (TIFFUnmapFileProc)unmap_buffer );
  if(!tif)
    error("\"open\" of TIF file failed!\n");
  
 i = ((struct image *)get_storage(img->img,image_program));

  if(!i)
    error("Image is not an image object.\n");

  is = i->img;

  if(img->alpha)
  {
    as = i->img;
    spp++;
    a = ((struct image *)get_storage(img->alpha,image_program));
    if(!a)
      error("Alpha is not an image object.\n");
    if(i->xsize != a->xsize ||
       i->ysize != a->ysize)
      error("Image and alpha objects are not equally sized!\n");
  }



  TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, i->xsize);
  TIFFSetField(tif, TIFFTAG_IMAGELENGTH, i->ysize);
  TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
  TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
  TIFFSetField(tif, TIFFTAG_COMPRESSION, opts->compression);
  if(opts->compression == COMPRESSION_LZW)
    TIFFSetField (tif, TIFFTAG_PREDICTOR, 2);
  if(as)
  {
    unsigned short val[1];
    val[0] = EXTRASAMPLE_ASSOCALPHA;
    TIFFSetField (tif, TIFFTAG_EXTRASAMPLES, 1, val);
  }
  TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
  TIFFSetField(tif, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
  if(opts->name)
    TIFFSetField(tif, TIFFTAG_DOCUMENTNAME, opts->name);
  TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, spp);
  TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, MAX(8192/i->xsize/spp,1));
  TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
  TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
  TIFFSetField(tif, TIFFTAG_XRESOLUTION, opts->xdpy);
  TIFFSetField(tif, TIFFTAG_YRESOLUTION, opts->ydpy);
  if(opts->comment)
    TIFFSetField(tif, TIFFTAG_IMAGEDESCRIPTION, opts->comment);

  buffer = xalloc( spp * i->xsize  );
  for (y = 0; y < i->ysize; y++)
  {
    char *b = buffer;
    for(x=0; x<i->xsize; x++)
    {
      *(b++)=is->r;
      *(b++)=is->g;
      *(b++)=(is++)->b;
      if(as)
        *(b++)=(as->r + as->g*2 + (as++)->b)/4;
    }
    if(TIFFWriteScanline(tif, buffer, y, 0) < 0)
    {
      free(buffer);
      error("TIFFWriteScanline returned error on line %d\n", y );
    }
  }
  free(buffer);
  TIFFFlushData (tif);
  TIFFClose (tif);
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
  unsigned int w, h, i;
  uint32 *raster,  *s;
  rgb_group *di, *da=NULL;
  tif = TIFFClientOpen( "memoryfile", "r", buf,
                        (TIFFReadWriteProc)read_buffer,
			(TIFFReadWriteProc)write_buffer,
                        (TIFFSeekProc)seek_buffer,
			(TIFFCloseProc)close_buffer,
                        (TIFFSizeProc)size_buffer,
			(TIFFMapFileProc)map_buffer,
                        (TIFFUnmapFileProc)unmap_buffer );
  if(!tif)
    error("Failed to 'open' tiff image.\n");

  TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
  TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
  s = raster = (uint32 *)_TIFFmalloc(w*h*sizeof(uint32));
  if (raster == NULL)
    error("Malloc failed to allocate buffer for %dx%d image\n",w,h);

  if(!TIFFReadRGBAImage(tif, w, h, raster, 0))
    error("Failed to read TIFF data\n");


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
    di->r = p&255;
    di->g = (p>>8) & 255;
    (di++)->b = (p>>16) & 255;
    if(!image_only) 
      da->r = da->g = (da++)->b = (p>>24) & 255;
    s++;
  }
  free(raster);
  if(!image_only)
  {
    apply( res->alpha, "mirrory", 0);
    free_object(res->alpha);
    res->alpha = sp[-1].u.object;
    sp--;
  }
  apply( res->img, "mirrory", 0);
  free_object(res->img);
  res->img = sp[-1].u.object;
  sp--;
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
    error("Too few arguments to Image.TIFF.decode()\n");

  if(sp[-args].type != T_STRING)
    error("Invalid argument 1 to Image.TIFF.decode()\n");

  buffer.str = sp[-args].u.string->str;
  buffer.len = buffer.real_len =sp[-args].u.string->len;
  buffer.extendable = 0;
  buffer.offset = 0;

  low_image_tiff_decode( &buffer, &res, 1 );
  pop_n_elems(args);
  push_object( res.img );
}

/*
**! method object decode(string data)
**! 	Decodes a TIFF image. 
**!
**! note
**!	Throws upon error in data.
*/

/*
**! method mapping _decode(string data)
**! 	Decodes a TIFF image to a mapping with at least the members 
**!     image and alpha. 
**!
**! note
**!	Throws upon error in data.
*/
static void image_tiff__decode( INT32 args )
{
  struct buffer buffer;
  struct imagealpha res;
  struct svalue *osp=sp;
  if(!args) 
    error("Too few arguments to Image.TIFF.decode()\n");
  if(sp[-args].type != T_STRING)
    error("Invalid argument 1 to Image.TIFF.decode()\n");

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


/*
**! method string encode(object image)
**! method string encode(object image, mapping options)
**! method string _encode(object image)
**! method string _encode(object image, mapping options)
**! 	encode and _encode are identical.
**!
**!      The <tt>options</tt> argument may be a mapping
**!	 containing zero or more encoding options:
**!
**!	<pre>
**!	normal options:
**!	    "compression":Image.TIFF.COMPRESSION_*,
**!	    "name":"an image name",
**!	    "comment":"an image comment",
**!	    "alpha":An alpha channel,
**!	    "dpy":Dots per inch (as a float),
**!	    "xdpy":Horizontal dots per inch (as a float),
**!	    "ydpy":Vertical dots per inch (as a float),
**!	</pre>
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
#ifdef COMPRESSION_LZW
  c.compression = COMPRESSION_LZW;
#else
#ifdef COMPRESSION_PACKBITS
  c.compression = COMPRESSION_PACKBITS;
#else
  c.compression = COMPRESSION_NONE;
#endif
#endif

  if(args > 1)
  {
    float dpy;
    if(sp[-args+1].type != T_MAPPING)
      error("Invalid argument 2 to Image.TIFF.encode. Expected mapping.\n");
    parameter_int( sp-args+1, opt_compression, &c.compression );
    if(parameter_float( sp-args+1, opt_dpy, &dpy ))
      c.xdpy = c.ydpy = dpy;
    parameter_float( sp-args+1, opt_xdpy, &c.xdpy );
    parameter_float( sp-args+1, opt_ydpy, &c.ydpy );
    parameter_string( sp-args+1, opt_name, &c.name );
    parameter_string( sp-args+1, opt_comment, &c.comment );
    parameter_object( sp-args+1, opt_alpha, &a.alpha );
  }

  b.str = malloc( INITIAL_WRITE_BUFFER_SIZE );
  b.len = b.real_len = 0;
  b.offset = 0;
  b.extendable = 1;
  SET_ONERROR( onerr, free, b.str );
  low_image_tiff_encode( &b, &a, &c );
  UNSET_ONERROR( onerr );
  push_string( make_shared_binary_string( b.str, b.real_len ) );
}


void my_tiff_warning_handler(const char* module, const char* fmt, ...){}
void my_tiff_error_handler(const char* module, const char* fmt, ...){}

#endif /* HAVE_LIBTIFF */

void pike_module_init(void)
{
  extern void f_index(INT32);
#ifdef HAVE_LIBTIFF
#ifdef DYNAMIC_MODULE
   push_string(make_shared_string("Image")); push_int(0);
   SAFE_APPLY_MASTER("resolv",2);
   if (sp[-1].type==T_OBJECT) 
   {
     push_string(make_shared_string("image"));
     f_index(2);
     image_program=program_from_svalue(sp-1);
     pop_stack();

     push_string(make_shared_string("Image")); push_int(0);
     SAFE_APPLY_MASTER("resolv",2);
     push_string(make_shared_string("colortable"));
     f_index(2);
     image_colortable_program=program_from_svalue(sp-1);
     pop_stack();
   }
#endif /* DYNAMIC_MODULE */

   TIFFSetWarningHandler( (TIFFErrorHandler)my_tiff_warning_handler );
#if 1
   TIFFSetErrorHandler( (TIFFErrorHandler)my_tiff_error_handler );
#endif

   if (image_program)
   {
     add_function("decode",image_tiff_decode,"function(string:object)",0);
     add_function("_decode",image_tiff__decode,"function(string:mapping)",0);
     add_function("encode",image_tiff_encode,
                  "function(object,mapping|void:string)",0); 
     add_function("_encode",image_tiff_encode,
                  "function(object,mapping|void:string)",0); 
   }
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
#endif /* HAVE_LIBTIFF */
}

void pike_module_exit(void)
{
#ifdef HAVE_LIBTIFF
  free_string(opt_compression);
  free_string(opt_name);
  free_string(opt_comment);
  free_string(opt_alpha);
  free_string(opt_dpy);
  free_string(opt_xdpy);
  free_string(opt_ydpy);
#endif
}
