/* $Id: x.c,v 1.20 1999/05/03 21:17:32 mirar Exp $ */

/*
**! module Image
**! note
**!	$Id: x.c,v 1.20 1999/05/03 21:17:32 mirar Exp $
**! submodule X
**!
**!	This submodule handles encoding and decoding of
**!	the binary formats of X11.
**!
**!	
**!
**! see also: Image, Image.Image, Image.Colortable
*/
#include "global.h"

#include <math.h>
#include <ctype.h>

#include "stralloc.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif

RCSID("$Id: x.c,v 1.20 1999/05/03 21:17:32 mirar Exp $");
#include "pike_macros.h"
#include "object.h"
#include "constants.h"
#include "interpret.h"
#include "svalue.h"
#include "threads.h"
#include "array.h"
#include "error.h"




#include "image.h"
#include "colortable.h"
#include "builtin_functions.h"

extern struct program *image_colortable_program;
extern struct program *image_program;

/*
**! method string encode_truecolor(object image,int bpp,int alignbits,int swapbytes,int rbits,int rshift,int gbits,int gshift,int bbits,int bshift)
**! method string encode_truecolor_masks(object image,int bpp,int alignbits,int swapbytes,int rmask,int gmask,int bmask)
**! method string encode_truecolor(object image,int bpp,int alignbits,int swapbytes,int rbits,int rshift,int gbits,int gshift,int bbits,int bshift,object ct)
**! method string encode_truecolor_masks(object image,int bpp,int alignbits,int swapbytes,int rmask,int gmask,int bmask,object ct)
**!	Pack an image into a truecolor string. You will get a string
**!	of packed red, green and blue bits; 
**!	ie: 
**!
**!	<tt>encode_truecolor(img, 12,32, 0, 3,5, 4,0, 3,8)</tt>
**!	will give (aligned to even 32 bits for each row):<br>
**!	<tt>0bbbrrr0 gggg0bbb rrr0gggg 0bbb</tt>...<br>
**!	<tt>&lt;--pixel 1--&gt;&lt;--pixel 2--&gt; &lt;--3--&gt;</tt><br>
**!	<tt>10987654 32101098 76543210 1098</tt>... &lt;- bit position
**!	<tt> &lt;-&gt;&lt;-&gt;  &lt;--&gt;</tt>
**!	<tt>  |  |    +--- 4,0</tt>: 4 bits green shifted 0 bits
**!	<tt>  |  +-------- 3,5</tt>: 3 bits red shifted 5 bits
**!	<tt>  +----------- 3,8</tt>: 3 bits blue shifted 8 bits
**!
**!	The above call is equal to 
**!	<br><tt>encode_truecolor_masks(img, 12,32, 0, 224, 15, 768)</tt>
**!	and
**!	<br><tt>encode_truecolor(img, 12,32, 0, 3,5,4,0,3,8, colortable(1&lt;&lt;3,1&lt;&lt;4,1&lt;&lt;3))</tt>. 
**!	<br>The latter gives possibility to use dither algorithms, 
**!	but is slightly slower.
**!
**! arg object image
**!	the image object to encode
**! arg int bpp
**!	bits per pixel, how many bits each pixel should take
**! arg int alignbits
**!	the number of even bits each line should be padded to
**! arg int rbits
**! arg int gbits
**! arg int bbits
**!	bits for each basecolor
**! arg int rshift
**! arg int gshift
**! arg int bshift
**!	leftshifts for each basecolor
**! arg int rmask
**! arg int gmask
**! arg int bmask
**! 	masks for each basecolor (xbits and gbits are calculated from this),
**!	needs to be massive (no zeroes among the ones in the mask).
**! arg object ct
**!	colortable object (for dithering, or whatever)
**! arg int swapbytes
**!	swap bytes for bpp==16,24,32,
**!	swaps bits in the bytes if bpp==1,
**!	for change of byte/bitorder between client and server.
**! 
*/

static unsigned char swap_bits[256] = 
{ 0,128,64,192,32,160,96,224,16,144,80,208,48,176,112,
  240,8,136,72,200,40,168,104,232,24,152,88,216,56,184,
  120,248,4,132,68,196,36,164,100,228,20,148,84,212,52,
  180,116,244,12,140,76,204,44,172,108,236,28,156,92,220,
  60,188,124,252,2,130,66,194,34,162,98,226,18,146,82,
  210,50,178,114,242,10,138,74,202,42,170,106,234,26,154,
  90,218,58,186,122,250,6,134,70,198,38,166,102,230,22,
  150,86,214,54,182,118,246,14,142,78,206,46,174,110,238,
  30,158,94,222,62,190,126,254,1,129,65,193,33,161,97,
  225,17,145,81,209,49,177,113,241,9,137,73,201,41,169,
  105,233,25,153,89,217,57,185,121,249,5,133,69,197,37,
  165,101,229,21,149,85,213,53,181,117,245,13,141,77,205,
  45,173,109,237,29,157,93,221,61,189,125,253,3,131,67,
  195,35,163,99,227,19,147,83,211,51,179,115,243,11,139,
  75,203,43,171,107,235,27,155,91,219,59,187,123,251,7,
  135,71,199,39,167,103,231,23,151,87,215,55,183,119,247,
  15,143,79,207,47,175,111,239,31,159,95,223,63,191,127,255};

static void image_x_encode_truecolor(INT32 args)
{
   struct image *img;
   struct neo_colortable *nct=NULL;
   int rbits,rshift,gbits,gshift,bbits,bshift;
   int bpp,alignbits;
   unsigned long rfmask,gfmask,bfmask;
   unsigned char *d;
   struct pike_string *dest;
   INT32 x,y;
   rgb_group *s,*tmp=NULL;
   int swap_bytes;

   if (args<10)
      error("Image.X.encode_truecolor: too few arguments (expected 10 arguments)\n");
   
   if (sp[-args].type!=T_OBJECT ||
       !(img=(struct image*)get_storage(sp[-args].u.object,image_program)))
      error("Image.X.encode_truecolor: illegal argument 1 (expected image object)\n");
   if (args>10)
      if (sp[10-args].type!=T_OBJECT ||
	  !(nct=(struct neo_colortable*)
	    get_storage(sp[10-args].u.object,image_colortable_program)))
	 error("Image.X.encode_truecolor: illegal argument 10 (expected colortable object)\n");
	 
   if (sp[1-args].type!=T_INT)
      error("Image.X.encode_truecolor: illegal argument 2 (expected integer)\n");
   else
      bpp=sp[1-args].u.integer;

   if (sp[2-args].type!=T_INT)
      error("Image.X.encode_truecolor: illegal argument 3 (expected integer)\n");
   else
      alignbits=sp[2-args].u.integer;

   if (!alignbits) alignbits=1;

   if (sp[3-args].type!=T_INT)
      error("Image.X.encode_truecolor: illegal argument 4 (expected integer)\n");
   else
      swap_bytes=sp[3-args].u.integer;

   if (sp[4-args].type!=T_INT)
      error("Image.X.encode_truecolor: illegal argument 5 (expected integer)\n");
   else
      rbits=sp[4-args].u.integer;

   if (sp[5-args].type!=T_INT)
      error("Image.X.encode_truecolor: illegal argument 6 (expected integer)\n");
   else
      rshift=sp[5-args].u.integer;

   if (sp[6-args].type!=T_INT)
      error("Image.X.encode_truecolor: illegal argument 7 (expected integer)\n");
   else
      gbits=sp[6-args].u.integer;

   if (sp[7-args].type!=T_INT)
      error("Image.X.encode_truecolor: illegal argument 8 (expected integer)\n");
   else
      gshift=sp[7-args].u.integer;

   if (sp[8-args].type!=T_INT)
      error("Image.X.encode_truecolor: illegal argument 9 (expected integer)\n");
   else
      bbits=sp[8-args].u.integer;

   if (sp[9-args].type!=T_INT)
      error("Image.X.encode_truecolor: illegal argument 10 (expected integer)\n");
   else
      bshift=sp[9-args].u.integer;

   if (nct) 
   {
      tmp=(rgb_group*)xalloc(sizeof(rgb_group)*img->xsize*img->ysize +1);
      if (!image_colortable_map_image(nct,img->img,tmp,
				      img->xsize*img->ysize,img->xsize))
      {
	 free(tmp);
	 error("Image.X.encode_truecolor(): called colortable is not initiated\n");
      }
      s=tmp;
   }
   else
      s=img->img;


   dest=begin_shared_string(( ( (img->xsize*bpp+alignbits-1) / alignbits )
			      * alignbits*img->ysize  +7 ) / 8);
   d=(unsigned char*)(dest->str);
   *d=0;

   y=img->ysize;

   THREADS_ALLOW();
   
   if (!(rshift&7) && !(gshift&7) && !(bshift&7) 
       && sizeof(COLORTYPE)==1
       && rbits==8 && gbits==8 && bbits==8 && !(bpp&7) 
       && (!(alignbits&7) || !(bpp%alignbits)) )
   {
      INT32 Bpp=bpp>>3;
      INT32 rpos=-(rshift>>3)-1;
      INT32 gpos=-(gshift>>3)-1;
      INT32 bpos=-(bshift>>3)-1;
      INT32 linemod=(alignbits-((img->xsize*bpp+alignbits-1)%alignbits)-1)>>3;

      if (!linemod && Bpp==4 && rpos!=gpos && gpos!=bpos) 
      {
	 INT32 zpos=-4;
	 while (rpos==zpos||gpos==zpos||bpos==zpos) zpos++;
	 while (y--)
	 {
	    x=img->xsize;
	    while (x--) 
	       d+=4,d[rpos]=s->r,d[gpos]=s->g,d[bpos]=s->b,
		  d[zpos]=0,s++; 
	 }
      }
      else if (!linemod && Bpp==3 && rpos!=gpos && gpos!=bpos) 
      {
	 while (y--)
	 {
	    x=img->xsize;
	    while (x--) 
	       d+=3,d[rpos]=s->r,d[gpos]=s->g,d[bpos]=s->b,
		  s++; 
	 }
      }
      else
      {
	 MEMSET(d,0,( ( (img->xsize*bpp+alignbits-1) / alignbits )
		      * alignbits*img->ysize  +7 ) / 8);
	 while (y--)
	 {
	    x=img->xsize;
	    while (x--) 
	       d+=Bpp,d[rpos]=s->r,d[gpos]=s->g,d[bpos]=s->b,
		  s++; 
	    d+=linemod;
	 }
      }
   }
   else
   {
      INT32 rfshift,gfshift,bfshift,rzshift,gzshift,bzshift;
      INT32 bpshift,blinemod,bit;

      rfshift=rshift-(sizeof(COLORTYPE)*8-rbits);
      gfshift=gshift-(sizeof(COLORTYPE)*8-gbits);
      bfshift=bshift-(sizeof(COLORTYPE)*8-bbits);
      if (rfshift<0) rzshift=-rfshift,rfshift=0; else rzshift=0;
      if (gfshift<0) gzshift=-gfshift,gfshift=0; else gzshift=0;
      if (bfshift<0) bzshift=-bfshift,bfshift=0; else bzshift=0;

      rfmask=(((1<<rbits)-1)<<(sizeof(COLORTYPE)*8-rbits));
      gfmask=(((1<<gbits)-1)<<(sizeof(COLORTYPE)*8-gbits));
      bfmask=(((1<<bbits)-1)<<(sizeof(COLORTYPE)*8-bbits));
      bpshift=sizeof(unsigned long)*8-bpp;
      blinemod=alignbits-( (img->xsize*bpp+alignbits-1) % alignbits)-1;

      bit=0;

      while (y--)
      {
	 INT32 bp;

	 x=img->xsize;
	 while (x--) /* write bits from this line */
	 {
	    register unsigned long b =
	       ((((s->r&rfmask)>>rzshift)<<rfshift)|
		(((s->g&gfmask)>>gzshift)<<gfshift)|
		(((s->b&bfmask)>>bzshift)<<bfshift))<<bpshift;
	    bp = bpp;
	    while (bp>8-bit)
	    {
	       *d|=(unsigned char)(b>>(24+bit));
	       b<<=8-bit;
	       bp-=8-bit;
	       *(++d)=0; bit=0;
	    }
	    *d|=b>>(24+bit); 
	    bit+=bp;
	    if (bit==8) *(++d)=0,bit=0;
	    s++;
	 }
	 bp=blinemod;
	 while (bp>8-bit) *(++d)=0,bp-=8-bit,bit=0;
	 bit+=bp;
	 if (bit==8) *(++d)=0,bit=0;
      }
   }


   if (swap_bytes)
   {
      d=(unsigned char*)dest->str;
      x=dest->len;
      switch (bpp)
      {
         case 32:
	    while (x>=4)
	    {
	       d[0]^=d[3],d[3]^=d[0],d[0]^=d[3];
	       d[1]^=d[2],d[2]^=d[1],d[1]^=d[2];
	       d+=4;
	       x-=4;
	    }
	    break;
         case 24:
	    while (x>=3)
	    {
	       d[0]^=d[2],d[2]^=d[0],d[0]^=d[2];
	       d+=3;
	       x-=3;
	    }
	    break;
         case 16:
	    while (x>=3)
	    {
	       d[0]^=d[1],d[1]^=d[0],d[0]^=d[1];
	       d+=2;
	       x-=2;
	    }
	    break;
         case 1:
	    while (x--) *(d++)=swap_bits[*d];
	    break;
      }
   }

   THREADS_DISALLOW();

   if (nct) free(tmp);

   pop_n_elems(args);
   push_string(end_shared_string(dest));
}

static INLINE void image_x_examine_mask(struct svalue *mask,
					const char *what,
					int *bits,int *shift)
{
   unsigned long x;
   if (mask->type!=T_INT)
      error("Image.X.encode_truecolor_masks: illegal %s (expected integer)\n",what);

   x=(unsigned long)mask->u.integer;
   x&=(unsigned long)((INT32)-1); /* i hope this works... */
   /* what i _really_ want to do is cast INT32 to unsigned INT32... */

   *bits=0; 
   *shift=0; 
   if (!x) return; 
   while (!(x&1)) x>>=1,(*shift)++;
   while (x&1) x>>=1,(*bits)++;

   if (x)
      error("Image.X.encode_truecolor_masks: illegal %s (nonmassive bitfield)\n",what);
}

static void image_x_call_examine_mask(INT32 args)
{
   int bits,shift;
   if (args<1 || sp[-args].type!=T_INT)
      error("Image.X.examine_mask: illegal argument(s)\n");

   image_x_examine_mask(sp-args,"argument 1",&bits,&shift);
   pop_n_elems(args);
    
   push_int(bits);
   push_int(shift);
   f_aggregate(2);
}

static void image_x_encode_truecolor_masks(INT32 args)
{
   struct object *ct=NULL;
   int rbits,rshift,gbits,gshift,bbits,bshift;

   if (args<7) 
      error("Image.X.encode_truecolor_masks: too few arguments (expected 7 arguments)\n");
   if (sp[-args].type!=T_OBJECT ||
       !get_storage(sp[-args].u.object,image_program))
      error("Image.X.encode_truecolor_masks: illegal argument 1 (expected image object)\n");

   if (args>7)
      if (sp[7-args].type!=T_OBJECT ||
	  !get_storage(ct=sp[7-args].u.object,image_colortable_program))
	 error("Image.X.encode_truecolor_masks: illegal argument 8 (expected colortable object)\n");
 
   if (sp[1-args].type!=T_INT)
      error("Image.X.encode_truecolor_masks: illegal argument 2 (expected integer)\n");
   if (sp[2-args].type!=T_INT)
      error("Image.X.encode_truecolor_masks: illegal argument 3 (expected integer)\n");

   if (sp[3-args].type!=T_INT)
      error("Image.X.encode_truecolor_masks: illegal argument 4 (expected integer)\n");

   image_x_examine_mask(sp+4-args,"argument 3 (red mask)",&rbits,&rshift);
   image_x_examine_mask(sp+5-args,"argument 4 (blue mask)",&gbits,&gshift);
   image_x_examine_mask(sp+6-args,"argument 5 (green mask)",&bbits,&bshift);

   if (ct) add_ref(ct);
   pop_n_elems(args-4);
   push_int(rbits);
   push_int(rshift);
   push_int(gbits);
   push_int(gshift);
   push_int(bbits);
   push_int(bshift);
   if (ct)
   {
      push_object(ct);
      image_x_encode_truecolor(11);
   }
   else
      image_x_encode_truecolor(10);
}

/*
**! method string encode_pseudocolor(object image,int bpp,int alignbits,int vbpp,object colortable)
**! method string encode_pseudocolor(object image,int bpp,int alignbits,int vbpp,object colortable,string translate)
**!
**! arg object image
**!	the image object to encode
**! arg int bpp
**!	bits per pixel, how many bits each pixel should take
**! arg int vbpp
**!	value bits per pixel; how many bits per pixel that really 
**!	contains information
**! arg int alignbits
**!	the number of even bits each line should be padded to
**! arg object colortable
**!	colortable to get indices for pseudocolor
**! arg string translate
**!	translate table for colors. Length of this string
**!	should be 1&lt;&lt;vbpp (or 2&lt;&lt;vbpp if vbpp are greater than 8).
**!
**! note
**!	currently, only upto 16 bits pseudocolor are supported.
*/

static void image_x_encode_pseudocolor_1byte_exact(INT32 args,
						   struct image *img,
						   struct neo_colortable *nct,
						   int bpp,int vbpp,int alignbits,
						   unsigned char* translate)
{
   struct pike_string *dest;
   INT32 linemod=(alignbits-((img->xsize*bpp+alignbits-1)%alignbits)-1)>>3;
   INT32 mask=((1<<vbpp)-1);

   dest=begin_shared_string(img->xsize*img->ysize);

   if (!image_colortable_index_8bit_image(nct,img->img,
					  (unsigned char*)(dest->str),
					  img->xsize*img->ysize,img->xsize))
   {
      free_string(end_shared_string(dest));
      error("Image.x.encode_pseudocolor: colortable not initialised");
   }

   if (!translate && !linemod)
   {
      pop_n_elems(args);
      push_string(end_shared_string(dest));
      return;
   }
		
   if (!linemod)
   {
      unsigned char *d;
      INT32 n;
      d=(unsigned char*)dest->str;
      n=img->xsize*img->ysize;
      while (n--) *d=translate[(*d)&mask],d++;
      pop_n_elems(args);
      push_string(end_shared_string(dest));
      return;
   }

   do
   {
      unsigned char *d;
      unsigned char *s;
      INT32 y,x,m;

      struct pike_string *dest2;
      dest2=begin_shared_string((img->xsize+linemod)*img->ysize);

      s=(unsigned char*)(dest->str);
      d=(unsigned char*)(dest2->str);

      y=img->ysize;
      while (y--)
      {
	 if (translate) 
	    { x=img->xsize; while (x--) *(d++)=translate[(*(s++))&mask]; }
	 else MEMCPY(d,s,img->xsize),d+=img->xsize,s+=img->xsize;
	 m=linemod;
	 while (m--) *(d++)=0;
      }

      free_string(end_shared_string(dest));
      pop_n_elems(args);
      push_string(end_shared_string(dest2));
   }
   while (0);
}
					     
static void image_x_encode_pseudocolor_1byte(INT32 args,
					     struct image *img,
					     struct neo_colortable *nct,
					     int bpp,int vbpp,int alignbits,
					     unsigned char* translate)
{
   struct pike_string *dest;
   INT32 blinemod=(alignbits-((img->xsize*bpp+alignbits-1)%alignbits)-1);
   unsigned char *d;
   unsigned char *s;
   INT32 y,x,bit,bp;
   unsigned long b;
   struct pike_string *dest2;

   dest=begin_shared_string(img->xsize*img->ysize);

   if (!image_colortable_index_8bit_image(nct,img->img,
					  (unsigned char*)(dest->str),
					  img->xsize*img->ysize,img->xsize))
   {
      free_string(end_shared_string(dest));
      error("Image.x.encode_pseudocolor: colortable not initialised");
   }

   dest2=begin_shared_string(((img->xsize*bpp+blinemod)*img->ysize+7)/8);

   s=(unsigned char*)(dest->str);
   d=(unsigned char*)(dest2->str);
   bit=0;
   *d=0;

   y=img->ysize;
   while (y--)
   {
      if (translate) 
      { 
	 x=img->xsize; 
	 while (x--) 
	 {
	    b=translate[*(s++)]<<(32-vbpp); 
	    bp = bpp;
	    while (bp>8-bit)
	    {
	       *d|=(unsigned char)(b>>(24+bit));
	       b<<=8-bit;
	       bp-=8-bit;
	       *(++d)=0; bit=0;
	    }
	    *d|=b>>24; 
	    bit+=bp;
	    if (bit==8) *(++d)=0,bit=0;
	 }
      }
      else 
      {
	 x=img->xsize; 
	 while (x--) 
	 {
	    b=(*(s++))<<(32-bpp); 
	    bp = bpp;
	    while (bp>8-bit)
	    {
#ifdef BITDEBUG
	       fprintf(stderr,"   b=%08x *d=%02x bp=%d bit=%d\n",b,*d,bp,bit);
#endif
	       *d|=(unsigned char)(b>>(24+bit));
	       b<<=8-bit;
	       bp-=8-bit;
#ifdef BITDEBUG
	       fprintf(stderr,">  b=%08x *d=%02x bp=%d bit=%d\n",b,*d,bp,bit);
#endif
	       *(++d)=0; bit=0;
	    }
#ifdef BITDEBUG
	    fprintf(stderr," - b=%08x *d=%02x bp=%d bit=%d\n",b,*d,bp,bit);
#endif
	    *d|=(unsigned char)(b>>(24+bit));
	    bit+=bp;
	    if (bit==8) *(++d)=0,bit=0;
#ifdef BITDEBUG
	    fprintf(stderr,"^- b=%08x *d=%02x bp=%d bit=%d\n",b,*d,bp,bit);
#endif
	 }
      }
      bp=blinemod;
      while (bp>8-bit) *(++d)=0,bp-=8-bit,bit=0;
      bit+=bp;
      if (bit==8) *(++d)=0,bit=0;
   }

   free_string(end_shared_string(dest));
   pop_n_elems(args);
   push_string(end_shared_string(dest2));
}
					     
static void image_x_encode_pseudocolor_2byte(INT32 args,
					     struct image *img,
					     struct neo_colortable *nct,
					     int bpp,int vbpp,int alignbits,
					     unsigned short  *translate)
{
   struct pike_string *dest;
   INT32 blinemod=(alignbits-((img->xsize*bpp+alignbits-1)%alignbits)-1);
   unsigned char *d;
   unsigned char *s;
   unsigned long b;
   INT32 y,x,bit,bp;
   struct pike_string *dest2;

   dest=begin_shared_string(img->xsize*img->ysize);

   if (!image_colortable_index_8bit_image(nct,img->img,
					  (unsigned char*)(dest->str),
					  img->xsize*img->ysize,img->xsize))
   {
      free_string(end_shared_string(dest));
      error("Image.x.encode_pseudocolor: colortable not initialised");
   }

   dest2=begin_shared_string(((img->xsize*bpp+blinemod)*img->ysize+7)/8);

   s=(unsigned char*)(dest->str);
   d=(unsigned char*)(dest2->str);
   *d=0;
   bit=0;

   y=img->ysize;
   while (y--)
   {
      if (translate) 
      { 
	 x=img->xsize; 
	 while (x--) 
	 {
	    b=ntohs(translate[*(s++)])<<(32-vbpp); 
	    bp = bpp;
	    while (bp>8-bit)
	    {
	       *d|=(unsigned char)(b>>(24+bit));
	       b<<=8-bit;
	       bp-=8-bit;
	       *(++d)=0; bit=0;
	    }
	    *d|=b>>24; 
	    bit+=bp;
	    if (bit==8) *(++d)=0,bit=0;
	 }
      }
      else 
      {
	 x=img->xsize; 
	 while (x--) 
	 {
	    b=(*(s++))<<(32-bpp); 
	    bp = bpp;
	    while (bp>8-bit)
	    {
	       *d|=(unsigned char)(b>>(24+bit));
	       b<<=8-bit;
	       bp-=8-bit;
	       *(++d)=0; bit=0;
	    }
	    *d|=b>>24; 
	    bit+=bp;
	    if (bit==8) *(++d)=0,bit=0;
	 }
      }
      bp=blinemod;
      while (bp>8-bit) *(++d)=0,bp-=8-bit,bit=0;
      bit+=bp;
      if (bit==8) *(++d)=0,bit=0;
   }

   free_string(end_shared_string(dest));
   pop_n_elems(args);
   push_string(end_shared_string(dest2));
}
					     
void image_x_encode_pseudocolor(INT32 args)
{
   INT32 bpp,alignbits,vbpp;
   struct image *img;
   struct neo_colortable *nct;
   char *translate=NULL;
   
   if (args<4) 
      error("Image.X.encode_pseudocolor: too few arguments");
   if (sp[1-args].type!=T_INT)
      error("Image.X.encode_pseudocolor: illegal argument 2 (expected integer)\n");
   if (sp[2-args].type!=T_INT)
      error("Image.X.encode_pseudocolor: illegal argument 3 (expected integer)\n");
   if (sp[3-args].type!=T_INT)
      error("Image.X.encode_pseudocolor: illegal argument 4 (expected integer)\n");
   bpp=sp[1-args].u.integer;
   alignbits=sp[2-args].u.integer;
   vbpp=sp[3-args].u.integer;
   if (!alignbits) alignbits=1;

   if (sp[-args].type!=T_OBJECT ||
       !(img=(struct image*)get_storage(sp[-args].u.object,image_program)))
      error("Image.X.encode_pseudocolor: illegal argument 1 (expected image object)\n");
   if (sp[4-args].type!=T_OBJECT ||
       !(nct=(struct neo_colortable*)
	 get_storage(sp[4-args].u.object,image_colortable_program)))
      error("Image.X.encode_pseudocolor: illegal argument 4 (expected colortable object)\n");

   if (args>5) {
      if (sp[5-args].type!=T_STRING)
	 error("Image.X.encode_pseudocolor: illegal argument 6 (expected string)\n");
      else if (sp[5-args].u.string->len!=((vbpp>8)?2:1)<<vbpp)
	 error("Image.X.encode_pseudocolor: illegal argument 6 (expected translate string of length %d, not %d)\n",((vbpp>8)?2:1)<<vbpp,sp[5-args].u.string->len);
      else 
	 translate=sp[5-args].u.string->str;
   } 
   if ( vbpp==8 && bpp==8 && !((bpp*img->xsize)%alignbits) )
      image_x_encode_pseudocolor_1byte_exact(args,img,nct,vbpp,bpp,alignbits,
					     (unsigned char*)translate);
   else if (vbpp<=8) 
      image_x_encode_pseudocolor_1byte(args,img,nct,bpp,vbpp,alignbits,
				       (unsigned char*)translate);
   else if (vbpp<=16) 
      image_x_encode_pseudocolor_2byte(args,img,nct,bpp,vbpp,alignbits,
				       (unsigned short*)translate);
   else error("Image.X.encode_pseudocolor: sorry, too many bits (%d>16)\n",
	      vbpp);
}

/*
**! method object decode_truecolor(string data,int width,int height,int bpp,int alignbits,int swapbytes,int rbits,int rshift,int gbits,int gshift,int bbits,int bshift)
**! method object decode_truecolor_masks(string data,int width,int height,int bpp,int alignbits,int swapbytes,int rmask,int gmask,int bmask)
**!    lazy support for truecolor ZPixmaps 
**!
**! note:
**!    currently, only byte-aligned masks are supported
*/

static void image_x_decode_truecolor(INT32 args)
{
   struct pike_string *ps;
   unsigned char *s;
   unsigned long len;
   INT32 width,height,bpp,alignbits,swapbytes;
   INT32 rbits,rshift,gbits,gshift,bbits,bshift;
   int i;
   INT32 n;
   rgb_group *d;

   if (args<12) 
      error("Image.X.decode_truecolor: too few arguments\n");
   if (sp[-args].type!=T_STRING) error("Image.X.decode_truecolor: illegal argument 1\n");
   for (i=1; i<12; i++)
      if (sp[i-args].type!=T_INT) 
	 error("Image.X.decode_truecolor: illegal argument %d\n",i+1);

   add_ref(ps=sp[-args].u.string);
   s=(unsigned char*)ps->str;
   len=ps->len;
   width=sp[1-args].u.integer;
   height=sp[2-args].u.integer;
   bpp=sp[3-args].u.integer;
   alignbits=sp[4-args].u.integer;
   swapbytes=sp[5-args].u.integer;
   rbits=sp[6-args].u.integer;
   rshift=sp[7-args].u.integer;
   gbits=sp[8-args].u.integer;
   gshift=sp[9-args].u.integer;
   bbits=sp[10-args].u.integer;
   bshift=sp[11-args].u.integer;

   pop_n_elems(args);

   if (bbits==8 && rbits==8 && gbits==8  &&
       !((rshift|gshift|bshift|alignbits|bpp)&7))
   {
      int rpos=(rshift>>3);
      int gpos=(gshift>>3);
      int bpos=(bshift>>3);
      int Bpp=(bpp>>3);
      struct object *o;
      struct image *img;

      if (rpos>=Bpp || rpos<0 ||
	  gpos>=Bpp || gpos<0 ||
	  bpos>=Bpp || bpos<0)
	 error("Image.X.decode_truecolor: illegal colorshifts\n");
      
      if (swapbytes)
	 rpos=Bpp-1-rpos,
	    gpos=Bpp-1-gpos,
	    bpos=Bpp-1-bpos;

      push_int(width);
      push_int(height);
      o=clone_object(image_program,2);
      img=(struct image*)get_storage(o,image_program);

      d=img->img;
      n=width*height;
      while (n--)
      {
	 d->r=s[rpos];
	 d->g=s[gpos];
	 d->b=s[bpos];
	 d++;

	 if (n && (unsigned long)Bpp>=len) 
	    break;
	 len-=Bpp;
	 s+=Bpp;
      }
      
      free_string(ps);
      push_object(o);
   }
   else
   {
      free_string(ps);
      error("Image.X.decode_truecolor: currently not supported non-byte ranges\n");
   }
}

void image_x_decode_truecolor_masks(INT32 args)
{
   struct object *ct=NULL;
   int rbits,rshift,gbits,gshift,bbits,bshift;

   if (args<9) 
      error("Image.X.decode_truecolor_masks: too few arguments (expected 7 arguments)\n");
   if (sp[-args].type!=T_STRING)
      error("Image.X.decode_truecolor_masks: illegal argument 1 (expected image object)\n");

   if (args>9)
      if (sp[9-args].type!=T_OBJECT ||
	  !get_storage(ct=sp[9-args].u.object,image_colortable_program))
	 error("Image.X.decode_truecolor_masks: illegal argument 8 (expected colortable object)\n");
 
   if (sp[6-args].type!=T_INT)
      error("Image.X.decode_truecolor_masks: illegal argument 7 (expected integer)\n");
   if (sp[7-args].type!=T_INT)
      error("Image.X.decode_truecolor_masks: illegal argument 8 (expected integer)\n");

   if (sp[8-args].type!=T_INT)
      error("Image.X.decode_truecolor_masks: illegal argument 9 (expected integer)\n");

   image_x_examine_mask(sp+6-args,"argument 7 (red mask)",&rbits,&rshift);
   image_x_examine_mask(sp+7-args,"argument 8 (blue mask)",&gbits,&gshift);
   image_x_examine_mask(sp+8-args,"argument 9 (green mask)",&bbits,&bshift);

   if (ct) add_ref(ct);
   pop_n_elems(args-6);
   push_int(rbits);
   push_int(rshift);
   push_int(gbits);
   push_int(gshift);
   push_int(bbits);
   push_int(bshift);
   if (ct)
   {
      push_object(ct);
      image_x_decode_truecolor(13);
   }
   else
      image_x_decode_truecolor(12);
}

/*
**! method object decode_pseudocolor(string data,int width,int height,int bpp,int alignbits,int swapbytes,object colortable)
**!    lazy support for pseudocolor ZPixmaps 
**!
**! note:
**!    currently, only byte-aligned pixmaps are supported
*/

void image_x_decode_pseudocolor(INT32 args)
{
   struct pike_string *ps;
   unsigned char *s;
   unsigned long len;
   INT32 width,height,bpp,alignbits,swapbytes;
   int i;
   INT32 n;
   rgb_group *d;
   struct neo_colortable *nct;
   struct object *ncto;

   if (args<7) 
      error("Image.X.decode_pseudocolor: too few arguments\n");
   if (sp[-args].type!=T_STRING) error("Image.X.decode_pseudocolor: illegal argument 1\n");
   for (i=1; i<6; i++)
      if (sp[i-args].type!=T_INT) 
	 error("Image.X.decode_pseudocolor: illegal argument %d\n",i+1);
   if (sp[6-args].type!=T_OBJECT ||
       !(nct=(struct neo_colortable*)
	 get_storage(ncto=sp[6-args].u.object,image_colortable_program)))
      error("Image.X.decode_pseudocolor: illegal argument 7\n");

   if (nct->type!=NCT_FLAT)
      /* fix me some other day */ 
      error("Image.X.decode_pseudocolor: argument 7, colortable, needs to be a flat colortable\n");

   add_ref(ps=sp[-args].u.string);
   s=(unsigned char*)ps->str;
   len=ps->len;
   width=sp[1-args].u.integer;
   height=sp[2-args].u.integer;
   bpp=sp[3-args].u.integer;
   alignbits=sp[4-args].u.integer;
   swapbytes=sp[5-args].u.integer;

   add_ref(ncto);

   pop_n_elems(args);

   if (bpp==8)
   {
      struct object *o;
      struct image *img;

      push_int(width);
      push_int(height);
      o=clone_object(image_program,2);
      img=(struct image*)get_storage(o,image_program);

      d=img->img;
      n=width*height;
      while (n--)
      {
	 if (*s>=nct->u.flat.numentries)
	    *d=nct->u.flat.entries[0].color;
	 else
	    *d=nct->u.flat.entries[*s].color;
	 d++;

	 if (n && len<=1) 
	    break;
	 len--;
	 s++;
      }
      
      free_string(ps);
      free_object(ncto);
      push_object(o);
   }
   else
   {
      free_object(ncto);
      free_string(ps);
      error("Image.X.decode_pseudocolor: currently not supported non-byte ranges\n");
   }
}

/**** init module ********************************************/

struct program *image_x_module_program=NULL;

void init_image_x(void)
{
   struct pike_string *s;
   start_new_program();
   
   add_function("encode_truecolor",image_x_encode_truecolor,
		"function(object,int,int,int,int,int,int,int,int,int,void|object:string)",0);
   add_function("encode_truecolor_masks",image_x_encode_truecolor_masks,
		"function(object,int,int,int,int,int,int,void|object:string)",0);
   add_function("encode_pseudocolor",image_x_encode_pseudocolor,
		"function(object,int,int,int,object,void|string:string)",0);

   add_function("examine_mask",image_x_call_examine_mask,
		"function(int:array(int))",0);

   image_x_module_program=end_program();
   push_object(clone_object(image_x_module_program,0));
   add_constant(s=make_shared_string("X"),sp-1,0);
   free_string(s);
   pop_stack();
}

void exit_image_x(void)
{
   if(image_x_module_program)
   {
      free_program(image_x_module_program);
      image_x_module_program=0;
   }
}
