#include <config.h>

/* $Id: colortable.c,v 1.8 1997/10/21 18:37:01 mirar Exp $ */

/*
**! module Image
**! note
**!	$Id: colortable.c,v 1.8 1997/10/21 18:37:01 mirar Exp $<br>
**! class colortable
**!
**!	This object keeps colortable information,
**!	mostly for image re-coloring (quantization).
**!
**! see also: Image, Image.image, Image.font
*/

#undef COLORTABLE_DEBUG
#undef COLORTABLE_REDUCE_DEBUG

#include "global.h"
RCSID("$Id: colortable.c,v 1.8 1997/10/21 18:37:01 mirar Exp $");

#include <sys/types.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <errno.h>

#include "config.h"

#include "stralloc.h"
#include "pike_macros.h"
#include "object.h"
#include "constants.h"
#include "interpret.h"
#include "svalue.h"
#include "array.h"
#include "threads.h"
#include "builtin_functions.h"

#include "image.h"

struct program *image_colortable_program;
extern struct program *image_program;

typedef unsigned long nct_weight_t;
#define WEIGHT_NEEDED (nct_weight_t)(0x10000000)
#define WEIGHT_REMOVE (nct_weight_t)(0x10000001)

#define COLORLOOKUPCACHEHASHSIZE 207
#define COLORLOOKUPCACHEHASHR 7
#define COLORLOOKUPCACHEHASHG 17
#define COLORLOOKUPCACHEHASHB 1
#define COLORLOOKUPCACHEHASHVALUE(r,g,b) \
               (((COLORLOOKUPCACHEHASHR*(int)(r))+ \
		 (COLORLOOKUPCACHEHASHG*(int)(g))+ \
		 (COLORLOOKUPCACHEHASHB*(int)(b)))% \
		COLORLOOKUPCACHEHASHSIZE) 

#define SPACEFACTOR_R 2
#define SPACEFACTOR_G 3
#define SPACEFACTOR_B 1

#define CUBICLE_DEFAULT_R 4
#define CUBICLE_DEFAULT_G 5
#define CUBICLE_DEFAULT_B 4
#define CUBICLE_DEFAULT_ACCUR 16

#ifndef MAX
#define MAX(X,Y) ((X)>(Y)?(X):(Y))
#endif

#define SQ(x) ((x)*(x))
static INLINE int sq(int x) { return x*x; }

struct nct_flat_entry /* flat colorentry */
{
   rgb_group color;
   nct_weight_t weight;
   signed long no;
};

struct nct_scale
{
   struct nct_scale *next;
   rgb_group low,high;
   rgbl_group vector; /* high-low */
   float invsqvector; /* |vector|� */
   INT32 realsteps;
   int steps;
   float mqsteps;     /* 1.0/(steps-1) */
   int no[1];  /* actually no[steps] */
};

struct neo_colortable
{
   enum nct_type 
   { 
      NCT_NONE, /* no colors */
      NCT_FLAT, /* flat with weight */
      NCT_CUBE  /* cube with additions */
   } type;
   enum nct_lookup_mode /* see union "lu" below */
   {
      NCT_TREE, /* tree lookup */
      NCT_CUBICLES, /* cubicle lookup */
      NCT_FULL /* scan all values */
   } lookup_mode;

   union
   {
      struct nct_flat
      {
	 int numentries;
	 struct nct_flat_entry *entries;
      } flat;
      struct nct_cube
      {
	 nct_weight_t weight;
	 int r,g,b; /* steps of sides */
	 struct nct_scale *firstscale;
	 INT32 disttrig; /* (sq) distance trigger */
	 int numentries;
      } cube;
   } u;

   rgbl_group spacefactor; 
      /* rgb space factors, normally 2,3,1 */

   struct lookupcache
   {
      rgb_group src;
      rgb_group dest;
      int index;
   } lookupcachehash[COLORLOOKUPCACHEHASHSIZE];

   union /* of pointers!! */
   {
      struct nctlu_cubicles
      {
	 int r,g,b; /* size */
	 int accur; /* accuracy, default 2 */
	 struct nctlu_cubicle
	 {
	    int n; 
	    int *index; /* NULL if not initiated */
	 } *cubicles; /* [r*g*b], index as [ri+(gi+bi*g)*r] */
      } cubicles;
      struct nctlu_tree
      {
	 struct nctlu_treenode
	 {
	    int splitvalue;
	    enum { SPLIT_R,SPLIT_G,SPLIT_B,SPLIT_DONE } split_direction;
	    int less,more;
	 } *nodes; /* shoule be colors�2 */
      } tree;
   } lu;
};

#define THIS ((struct neo_colortable *)(fp->current_storage))
#define THISOBJ (fp->current_object)

/***************** init & exit *********************************/


static void colortable_free_lookup_stuff(struct neo_colortable *nct)
{
   switch (nct->lookup_mode)
   {
      case NCT_TREE: 
         if (nct->lu.tree.nodes) free(nct->lu.tree.nodes);
	 nct->lu.tree.nodes=NULL;
	 break;
      case NCT_CUBICLES: 
         if (nct->lu.cubicles.cubicles)
	 {
	    int i=nct->lu.cubicles.r*nct->lu.cubicles.g*nct->lu.cubicles.b;
	    while (i--) 
	       if (nct->lu.cubicles.cubicles[i].index)
		  free(nct->lu.cubicles.cubicles[i].index);
	    free(nct->lu.cubicles.cubicles);
	 }
	 nct->lu.cubicles.cubicles=NULL;
	 break;
      case NCT_FULL:
         break;
   }
}

static void free_colortable_struct(struct neo_colortable *nct)
{
   struct nct_scale *s;
   colortable_free_lookup_stuff(nct);
   switch (nct->type)
   {
      case NCT_NONE:
         return; /* done */
      case NCT_FLAT:
         free(nct->u.flat.entries);
	 nct->type=NCT_NONE;
	 return; /* done */
      case NCT_CUBE:
         while ((s=nct->u.cube.firstscale))
	 {
	    nct->u.cube.firstscale=s->next;
	    free(s);
	 };
	 nct->type=NCT_NONE;
         return; /* done */
   }
}

static void init_colortable_struct(struct object *o)
{
   int i;
   THIS->type=NCT_NONE;
   THIS->lookup_mode=NCT_CUBICLES;
   THIS->lu.cubicles.cubicles=NULL;

   THIS->spacefactor.r=SPACEFACTOR_R;
   THIS->spacefactor.g=SPACEFACTOR_G;
   THIS->spacefactor.b=SPACEFACTOR_B;
   
   THIS->lu.cubicles.r=CUBICLE_DEFAULT_R;
   THIS->lu.cubicles.g=CUBICLE_DEFAULT_G;
   THIS->lu.cubicles.b=CUBICLE_DEFAULT_B;
   THIS->lu.cubicles.accur=CUBICLE_DEFAULT_ACCUR;

   for (i=0; i<COLORLOOKUPCACHEHASHSIZE; i++)
      THIS->lookupcachehash[i].index=-1;
}

static void exit_colortable_struct(struct object *obj)
{
   free_colortable_struct(THIS);
}

/***************** internal stuff ******************************/

#if 0
#include <sys/resource.h>
#define CHRONO(X) chrono(X);

static void chrono(char *x)
{
   struct rusage r;
   static struct rusage rold;
   getrusage(RUSAGE_SELF,&r);
   fprintf(stderr,"%s: %ld.%06ld - %ld.%06ld\n",x,
	   (long)r.ru_utime.tv_sec,(long)r.ru_utime.tv_usec,
	   
	   (long)(((r.ru_utime.tv_usec-rold.ru_utime.tv_usec<0)?-1:0)
		  +r.ru_utime.tv_sec-rold.ru_utime.tv_sec),
	   (long)(((r.ru_utime.tv_usec-rold.ru_utime.tv_usec<0)?1000000:0)
		  + r.ru_utime.tv_usec-rold.ru_utime.tv_usec)
      );

   rold=r;
}
#else
#define CHRONO(X)
#endif

static void _img_copy_colortable(struct neo_colortable *dest,
				 struct neo_colortable *src)
{
   struct nct_scale *s,*d,**np;
   int i;
   for (i=0; i<COLORLOOKUPCACHEHASHSIZE; i++)
      dest->lookupcachehash[i].index=-1;

   dest->lookup_mode=src->lookup_mode;
   switch (dest->lookup_mode)
   {
      case NCT_TREE:     dest->lu.tree.nodes=NULL; break;
      case NCT_CUBICLES: dest->lu.cubicles.cubicles=NULL; break;
      case NCT_FULL:     break;
   }

   switch (src->type)
   {
      case NCT_NONE:
         dest->type=NCT_NONE;
         return; /* done */
      case NCT_FLAT:
         dest->type=NCT_NONE; /* don't free anything if xalloc throws */
         dest->u.flat.entries=(struct nct_flat_entry*)
	    xalloc(src->u.flat.numentries*sizeof(struct nct_flat_entry));
	 memcpy(dest->u.flat.entries,src->u.flat.entries,
		src->u.flat.numentries*sizeof(struct nct_flat_entry));
	 dest->u.flat.numentries=src->u.flat.numentries;
	 dest->type=NCT_FLAT;
	 return; /* done */
      case NCT_CUBE:
         *dest=*src;
	 dest->u.cube.firstscale=NULL;
	 np=&(dest->u.cube.firstscale);
	 s=src->u.cube.firstscale;
	 while (s)
	 {
	    d=(struct nct_scale*)
	       xalloc(sizeof(struct nct_scale)+s->steps*sizeof(int));
	    memcpy(d,s,sizeof(struct nct_scale)+s->steps*sizeof(int));
	    d->next=NULL;
	    *np=d;
	    np=&(d->next); /* don't change order */
	    s=s->next;
	 }
         return; /* done */
   }
}


#ifdef COLORTABLE_DEBUG
static void stderr_print_entries(struct nct_flat_entry *src,int n)
{
   int i;

   for (i=0; i<n; i++)
    if (src[i].weight==WEIGHT_NEEDED)
       fprintf(stderr,"#%02x%02x%02x  nd %3d ",src[i].color.r,src[i].color.g,src[i].color.b,src[i].no);
    else 
       fprintf(stderr,"#%02x%02x%02x %3d %3d ",src[i].color.r,src[i].color.g,src[i].color.b,src[i].weight,src[i].no);
   fprintf(stderr,"\n");
}
#endif

/*** reduction ***/

#define DIFF_R_MULT 3
#define DIFF_G_MULT 4
#define DIFF_B_MULT 3
#define DIFF_GREY_MULT 3

static int reduce_recurse(struct nct_flat_entry *src,
			  struct nct_flat_entry *dest,
			  int src_size,
			  int target_size,
			  int level,
			  rgbl_group sf)
{
   int n,i,m;
   rgbl_group sum={0,0,0},diff={0,0,0};
   unsigned long mmul,tot=0;
   long gdiff=0,g;
   int left,right;
   enum { SORT_R,SORT_G,SORT_B,SORT_GREY } st;


#ifdef COLORTABLE_REDUCE_DEBUG
   fprintf(stderr,"COLORTABLE%*s reduce_recurse %lx,%lx, %d,%d\n",level,"",(unsigned long)src,(unsigned long)dest,src_size,target_size);
#if 0
   stderr_print_entries(src,src_size);
#endif
#endif

   if (!src_size) return 0; 
      /* can't go figure some good colors just like that... */
   
   if (target_size<=1 || src_size<=1)
   {
      for (i=n=0; i<src_size; i++)
	 if (src[i].weight==WEIGHT_NEEDED)
	 {
	    *dest=src[i];
	    dest++;
	    n++;
	 }

      if (n>=target_size) return n;

      /* get the mean color */

      if ((mmul=src_size)>10240) mmul=10240;
      
      for (i=0; i<src_size; i++)
      {
	 unsigned long mul=src[i].weight;

	 sum.r+=src[i].color.r*mul;
	 sum.g+=src[i].color.g*mul;
	 sum.b+=src[i].color.b*mul;
	 tot+=mul;
      }

      dest->color.r=sum.r/tot;
      dest->color.g=sum.g/tot;
      dest->color.b=sum.b/tot;
      dest->weight=tot;
      dest->no=-1;

#ifdef COLORTABLE_REDUCE_DEBUG
  fprintf(stderr,"COLORTABLE%*s sum=%d,%d,%d tot=%d\n",level,"",sum.r,sum.g,sum.b,tot);
  fprintf(stderr,"COLORTABLE%*s dest=%d,%d,%d weidht=%d no=%d\n",level,"",dest->color.r,dest->color.g,dest->color.b,dest->weight,dest->no);
#endif
      return 1;
   }

   /* figure out what direction to split */
   
   tot=0;
   for (i=0; i<src_size; i++)
      if (src[i].weight!=WEIGHT_NEEDED) tot+=src[i].weight; 
   mmul=tot*10;

   if (!tot) /* all needed */
   {
      memcpy(dest,src,sizeof(struct nct_flat_entry)*src_size);
      return src_size;
   }

   tot=0;
   for (i=0; i<src_size; i++)
   {
      unsigned long mul;
      if ((mul=src[i].weight)==WEIGHT_NEEDED)
	 mul=mmul;
      sum.r+=src[i].color.r*mul;
      sum.g+=src[i].color.g*mul;
      sum.b+=src[i].color.b*mul;
      tot+=mul;
   }

#ifdef COLORTABLE_REDUCE_DEBUG
   fprintf(stderr,"COLORTABLE%*s sum=%d,%d,%d\n",level,"",sum.r,sum.g,sum.b);
#endif

   g=(sf.r*sum.r+sf.g*sum.g+sf.b*sum.b)/tot;
   sum.r/=tot;
   sum.g/=tot;
   sum.b/=tot;

#ifdef COLORTABLE_REDUCE_DEBUG
   fprintf(stderr,"COLORTABLE%*s mean=%d,%d,%d,%ld tot=%ld\n",level,"",sum.r,sum.g,sum.b,g,tot);
#endif

   for (i=0; i<src_size; i++)
   {
      unsigned long mul;
      if ((mul=src[i].weight)==WEIGHT_NEEDED)
	 mul=mmul;
      diff.r+=(sq(src[i].color.r-(INT32)sum.r)/8)*mul;
      diff.g+=(sq(src[i].color.g-(INT32)sum.g)/8)*mul;
      diff.b+=(sq(src[i].color.b-(INT32)sum.b)/8)*mul;
      gdiff+=(sq(sf.r*src[i].color.r+sf.g*src[i].color.g+
		 sf.b*src[i].color.b-g)/8)*mul;
      tot+=mul;
   }

   diff.r*=DIFF_R_MULT;
   diff.g*=DIFF_G_MULT;
   diff.b*=DIFF_B_MULT;
   gdiff=gdiff*DIFF_GREY_MULT/sq(sf.r+sf.g+sf.b);

   if (diff.r > diff.g)
      if (diff.r > diff.b)
	 if (diff.r > gdiff) st=SORT_R; else st=SORT_GREY;
      else 
	 if (diff.b > gdiff) st=SORT_B; else st=SORT_GREY;
   else 
      if (diff.g > diff.b)
	 if (diff.g > gdiff) st=SORT_G; else st=SORT_GREY;
      else 
	 if (diff.b > gdiff) st=SORT_B; else st=SORT_GREY;

#ifdef COLORTABLE_REDUCE_DEBUG
   fprintf(stderr,"COLORTABLE%*s diff=%d,%d,%d,%ld sort=%d\n",level,"",diff.r,diff.g,diff.b,gdiff,st);
#endif

   /* half-sort */

   left=0;
   right=src_size-1;

#define HALFSORT(C) \
      while (left<right) \
      { \
         struct nct_flat_entry tmp; \
         if (src[left].color.C>sum.C)  \
	    tmp=src[left],src[left]=src[right],src[right--]=tmp; \
         else left++; \
      } 

   switch (st)
   {
      case SORT_R: HALFSORT(r); break;
      case SORT_G: HALFSORT(g); break;
      case SORT_B: HALFSORT(b); break;
      case SORT_GREY:
         while (left<right) 
	 { 
	    struct nct_flat_entry tmp; 
	    if (src[left].color.r*sf.r+src[left].color.g*sf.g+
		src[left].color.b*sf.b>g)  
	       tmp=src[left],src[left]=src[right],src[right--]=tmp; 
	    else left++; 
	 } 
         break;
   }

#ifdef COLORTABLE_REDUCE_DEBUG
   fprintf(stderr,"COLORTABLE%*s left=%d right=%d\n",level,"",left,right);
#endif

   if (left==0) left++;

   i=target_size/2;
   if (src_size-left<target_size-i) i+=(target_size-i)-(src_size-left);
   else if (i>left) i=left;

#ifdef COLORTABLE_REDUCE_DEBUG
   fprintf(stderr,"COLORTABLE%*s try i=%d/%d - %d+%d=%d from %d+%d=%d\n",level+1,"",i,target_size,i,target_size-i,target_size,left,src_size-left,src_size);
#endif

   n=reduce_recurse(src,dest,left,i,level+2,sf); 
   m=reduce_recurse(src+left,dest+n,src_size-left,target_size-n,level+2,sf);

#ifdef COLORTABLE_REDUCE_DEBUG
   fprintf(stderr,"COLORTABLE%*s ->%d+%d=%d (tried for %d+%d=%d)\n",level,"",n,m,n+m,i,target_size-i,target_size);
#endif

   if (m>target_size-n && n<=i) /* right is too big, try again */
   {
      int oldn=n;

      i=target_size-m;
      if (src_size-left<target_size-i) i+=(target_size-i)-(src_size-left);
      else if (i>left) i=left;

#ifdef COLORTABLE_REDUCE_DEBUG
      fprintf(stderr,"COLORTABLE%*s try i=%d/%d - %d+%d=%d from %d+%d=%d\n",level+1,"",i,target_size,i,target_size-i,target_size,left,src_size-left,src_size);
#endif

      n=reduce_recurse(src,dest,left,i,level+2,sf);
      if (n!=oldn)
	 if (n<oldn) /* i certainly hope so */
	    memmove(dest+n,dest+oldn,sizeof(struct nct_flat_entry)*m);
	 else /* huh? */
	    /* this is the only case we don't have them already */
	    m=reduce_recurse(src+left,dest+n,src_size-left,target_size-n,level+2,sf);
#ifdef COLORTABLE_REDUCE_DEBUG
      fprintf(stderr,"COLORTABLE%*s ->%d+%d=%d (retried for %d+%d=%d)\n",level,"",n,m,n+m,i,target_size-i,target_size);
#endif
   }

   return n+m;
}

static struct nct_flat _img_reduce_number_of_colors(struct nct_flat flat,
						    unsigned long maxcols,
						    rgbl_group sf)
{
   int i,j;
   struct nct_flat_entry *newe;

   newe=malloc(sizeof(struct nct_flat_entry)*flat.numentries);
   if (!newe) { return flat; }

   i=reduce_recurse(flat.entries,newe,flat.numentries,maxcols,0,sf);

   free(flat.entries);

   flat.entries=realloc(newe,i*sizeof(struct nct_flat_entry));
   flat.numentries=i;
   if (!flat.entries) { free(newe); error("out of memory\n"); }

   for (j=0; j<i; j++)
      flat.entries[j].no=j;
   
   return flat;
}

/*** scanning ***/

#define DEFAULT_COLOR_HASH_SIZE 8192
#define MAX_COLOR_HASH_SIZE 32768

struct color_hash_entry
{
  rgb_group color;
  unsigned long pixels;
  int no;
};

static INLINE struct color_hash_entry *insert_in_hash(rgb_group rgb,
						struct color_hash_entry *hash,
						unsigned long hashsize)
{
   unsigned long j,k;
   j=(rgb.r*127+rgb.b*997+rgb.g*2111)%hashsize;
   k=100;
   if (j+100>=hashsize)
      while (--k && hash[j].pixels && 
	     (hash[j].color.r!=rgb.r || 
	      hash[j].color.g!=rgb.g || 
	      hash[j].color.b!=rgb.b))
	 j=(j+1)%hashsize;
   else
      while (--k && hash[j].pixels && 
	     (hash[j].color.r!=rgb.r || 
	      hash[j].color.g!=rgb.g || 
	      hash[j].color.b!=rgb.b))
	 j++;
   if (!k) return NULL; /* no space */
   hash[j].pixels++;
   hash[j].color=rgb;
   return hash+j;
}

static INLINE struct color_hash_entry *insert_in_hash_nd(rgb_group rgb,
						struct color_hash_entry *hash,
						unsigned long hashsize)
{
   unsigned long j,k;
   j=(rgb.r*127+rgb.b*997+rgb.g*2111)%hashsize;
   k=100;
   if (j+100>=hashsize)
      while (--k && hash[j].pixels && 
	     (hash[j].color.r!=rgb.r || 
	      hash[j].color.g!=rgb.g || 
	      hash[j].color.b!=rgb.b))
	 j=(j+1)%hashsize;
   else
      while (--k && hash[j].pixels && 
	     (hash[j].color.r!=rgb.r || 
	      hash[j].color.g!=rgb.g || 
	      hash[j].color.b!=rgb.b))
	 j++;
   if (!k) return NULL; /* no space */
   hash[j].color=rgb;
   return hash+j;
}

static INLINE struct color_hash_entry *insert_in_hash_mask(rgb_group rgb,
    struct color_hash_entry *hash,unsigned long hashsize,rgb_group mask)
{
   unsigned long j,k;
   rgb.r&=mask.r;
   rgb.g&=mask.g;
   rgb.b&=mask.b;
   j=(rgb.r*127+rgb.b*997+rgb.g*2111)%hashsize;
   k=100;
   if (j+100>=hashsize)
      while (--k && hash[j].pixels && 
	     (hash[j].color.r!=rgb.r || 
	      hash[j].color.g!=rgb.g || 
	      hash[j].color.b!=rgb.b))
	 j=(j+1)%hashsize;
   else
      while (--k && hash[j].pixels && 
	     (hash[j].color.r!=rgb.r || 
	      hash[j].color.g!=rgb.g || 
	      hash[j].color.b!=rgb.b))
	 j++;
   if (!k) return NULL; /* no space */
   hash[j].pixels++;
   hash[j].color=rgb;
   return hash+j;
}

static INLINE rgb_group get_mask_of_level(int level)
{
   static unsigned char strip_r[24]=
   { 0xff, 0xfe, 0xfe, 0xfe, 0xfc, 0xfc, 0xfc, 0xf8, 0xf8, 0xf8, 0xf0,
     0xf0, 0xf0, 0xe0, 0xe0, 0xe0, 0xc0, 0xc0, 0xc0, 0x80, 0x80, 0x80 };
   static unsigned char strip_g[24]=
   { 0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xfc, 0xfc, 0xfc, 0xf8, 0xf8, 0xf8, 
     0xf0, 0xf0, 0xf0, 0xe0, 0xe0, 0xe0, 0xc0, 0xc0, 0xc0, 0x80, 0x80, 0x80 };
   static unsigned char strip_b[24]=
   { 0xfe, 0xfe, 0xfe, 0xfc, 0xfc, 0xfc, 0xf8, 0xf8, 0xf8, 0xf0, 0xf0, 
     0xf0, 0xe0, 0xe0, 0xe0, 0xc0, 0xc0, 0xc0, 0x80, 0x80, 0x80 };

   rgb_group res;
   res.r=strip_r[level];
   res.g=strip_g[level];
   res.b=strip_b[level];

   return res;
}

static struct nct_flat _img_get_flat_from_image(struct image *img,
						unsigned long maxcols)
{
   struct color_hash_entry *hash;
   unsigned long hashsize=DEFAULT_COLOR_HASH_SIZE;
   unsigned long i,j,k;
   rgb_group *s;
   struct nct_flat flat;

   hash=(struct color_hash_entry*)
      xalloc(sizeof(struct color_hash_entry)*hashsize);
   i=hashsize;
   while (i--) hash[i].pixels=0;
   
   i=img->xsize*img->ysize;
   s=img->img;

   while (i)
   {
      if (!insert_in_hash(*s,hash,hashsize))
      {
	 struct color_hash_entry *oldhash=hash,*mark;
	 j=hashsize;

rerun_rehash:

	 hashsize*=2;
	 if (hashsize>MAX_COLOR_HASH_SIZE)
	 {
	    hashsize/=2;
	    break; /* try again, with mask */
	 }
#ifdef COLORTABLE_DEBUG
   fprintf(stderr,"COLORTABLE %ld pixels left; hashsize=%ld\n",i,hashsize);
#endif

	 hash=malloc(sizeof(struct color_hash_entry)*hashsize);
	 if (!hash)
	 {
	    free(oldhash);
	    error("out of memory\n");
	 }
	 k=hashsize;
	 while (k--) hash[k].pixels=0;

	 while (j--) 
	    if (oldhash[j].pixels)
	    {
	       mark=insert_in_hash(oldhash[j].color,hash,hashsize);
	       if (!mark) goto rerun_rehash;
	       mark->pixels=oldhash[j].pixels;
	    }
	 
	 free(oldhash);
	 continue;
      }

      i--;
      s++;
   }

   if (i) /* restart, but with mask */
   {
      int mask_level=0;
      rgb_group rgb_mask;
      struct color_hash_entry *oldhash,*mark;

rerun_mask:

      rgb_mask=get_mask_of_level(mask_level);
      mask_level++;
#ifdef COLORTABLE_DEBUG
      fprintf(stderr,"COLORTABLE mask level=%d mask=%02x%02x%02x\n",
	      mask_level,rgb_mask.r,rgb_mask.g,rgb_mask.b);
#endif
      
      oldhash=hash;
      j=hashsize;
      
      hash=malloc(sizeof(struct color_hash_entry)*hashsize);
      if (!hash)
      {
	 free(oldhash);
	 error("out of memory\n");
      }
      
      k=hashsize;
      while (k--) hash[k].pixels=0;

      while (j--) 
	 if (oldhash[j].pixels)
	 {
	    mark=insert_in_hash_mask(oldhash[j].color,hash,hashsize,rgb_mask);
	    if (!mark) goto rerun_mask; /* increase mask level inst of hash */
	    mark->pixels+=oldhash[j].pixels-1;
	 }

      free(oldhash);
      
      while (i) 
      {
	 if (!insert_in_hash_mask(*s,hash,hashsize,rgb_mask))
	    goto rerun_mask; /* increase mask */

	 i--;
	 s++;
      }
   }

   /* convert to flat */

   i=hashsize;
   j=0;
   while (i--)
      if (hash[i].pixels) j++;
   /* j is now the number of colors */

#ifdef COLORTABLE_DEBUG
   fprintf(stderr,"COLORTABLE %ld colors found in image; hashsize=%ld\n",j,hashsize);
#endif

   flat.numentries=j;
   flat.entries=malloc(sizeof(struct nct_flat_entry)*j);
   if (!flat.entries)
   {
      free(hash);
      error("out of memory\n");
   }
   j=0;
   i=hashsize;
   while (i--)
      if (hash[i].pixels)
      {
	 flat.entries[j].color=hash[i].color;
	 flat.entries[j].no=j;
	 flat.entries[j].weight=hash[i].pixels;
	 j++;
      }

   if (((int)j)!=flat.numentries) abort();
   
   free(hash);

   return flat;
}

static struct nct_flat _img_get_flat_from_array(struct array *arr)
{
   struct svalue s,s2;
   struct nct_flat flat;
   int i;

   flat.numentries=arr->size;
   flat.entries=(struct nct_flat_entry*)
      xalloc(flat.numentries*sizeof(struct nct_flat_entry));

   s2.type=s.type=T_INT;
   for (i=0; i<arr->size; i++)
   {
      array_index(&s,arr,i);
      if (s.type!=T_ARRAY || s.u.array->size<3)
      {
	 free(flat.entries);
	 error("Illegal type in colorlist, element %d\n",i);
      }
      array_index(&s2,s.u.array,0);
      if (s2.type!=T_INT) flat.entries[i].color.r=0; else flat.entries[i].color.r=s2.u.integer;
      array_index(&s2,s.u.array,1);
      if (s2.type!=T_INT) flat.entries[i].color.g=0; else flat.entries[i].color.g=s2.u.integer;
      array_index(&s2,s.u.array,2);
      if (s2.type!=T_INT) flat.entries[i].color.b=0; else flat.entries[i].color.b=s2.u.integer;
      flat.entries[i].weight=1;
      flat.entries[i].no=i;
   }
   free_svalue(&s);
   free_svalue(&s2);

   return flat;
}

static INLINE void _find_cube_dist(struct nct_cube cube,rgb_group rgb,
				   int *dist,int *no,
				   rgbl_group sf)
{
   int mindist;
   struct nct_scale *s;
   int nc;

   *no=-1;

   if (cube.r&&cube.g&&cube.b)
   {
      mindist=sf.r*sq((((rgb.r*(int)cube.r+cube.r/2)>>8)*255)/(cube.r-1)-rgb.r)+
	      sf.g*sq((((rgb.g*(int)cube.g+cube.g/2)>>8)*255)/(cube.g-1)-rgb.g)+
	      sf.b*sq((((rgb.b*(int)cube.b+cube.b/2)>>8)*255)/(cube.b-1)-rgb.b);

      *no=((rgb.r*cube.r+cube.r/2)>>8)+
	  ((rgb.g*cube.g+cube.g/2)>>8)*cube.r+
	  ((rgb.b*cube.b+cube.b/2)>>8)*cube.r*cube.g;

      if (mindist<cube.disttrig)
      {
	 *dist=mindist;
	 return;
      }
   }
   else
      mindist=10000000;

   s=cube.firstscale;

   nc=cube.r*cube.g*cube.b;

   while (s)
   {
      rgbl_group b;
      int n;

      b.r=((INT32)rgb.r)-s->low.r;
      b.g=((INT32)rgb.g)-s->low.g;
      b.b=((INT32)rgb.b)-s->low.b;

      n=(int)((s->steps*(b.r*s->vector.r+
			 b.g*s->vector.g+
			 b.b*s->vector.b))*s->invsqvector);

      if (n<0) n=0; else if (n>=s->steps) n=s->steps-1;

      if (s->no[n]>=nc) 
      {
	 int steps=s->steps;
	 int ldist=sf.r*sq(rgb.r-(INT32)((s->high.r*n+s->low.r*(steps-n-1))/(steps-1)))+
	           sf.g*sq(rgb.g-(INT32)((s->high.g*n+s->low.g*(steps-n-1))/(steps-1)))+
	           sf.b*sq(rgb.b-(INT32)((s->high.b*n+s->low.b*(steps-n-1))/(steps-1)));

	 if (ldist<mindist)
	 {
	    *no=s->no[n];
	    mindist=ldist;
	 }
      }

      nc+=s->realsteps;
      s=s->next;
   }
  
   *dist=mindist;
}

static struct nct_cube _img_get_cube_from_args(INT32 args)
{
   struct nct_cube cube;
   struct nct_scale *s,**np;
   int no,i;
   int osteps,o2steps;
   rgbl_group sf;

   INT32 ap;

   if (args<3 ||
       sp[-args].type!=T_INT ||
       sp[1-args].type!=T_INT ||
       sp[2-args].type!=T_INT)
      error("Illegal argument(s) 1, 2 or 3\n");

   cube.r=sp[-args].u.integer;
   cube.g=sp[1-args].u.integer;
   cube.b=sp[2-args].u.integer;
   if (cube.r<2) cube.r=0;
   if (cube.g<2) cube.g=0;
   if (cube.b<2) cube.b=0;

   sf=THIS->spacefactor;

   if (cube.r && cube.b && cube.g)
      cube.disttrig=(sf.r*SQ(255/cube.r)+
		     sf.g*SQ(255/cube.g)+
		     sf.b*SQ(255/cube.b))/4;
  			 /* 4 is for /2�, closest dot, always */
   else
      cube.disttrig=100000000;
      
   no=cube.r*cube.g*cube.b;

   ap=3;

   cube.firstscale=NULL;
   np=&(cube.firstscale);

   while (args>=ap+3)
   {
      rgb_group low,high;
      int steps;
      int isteps;
      int mdist;
      int c;

      if (sp[ap-args].type!=T_ARRAY ||
	  sp[1+ap-args].type!=T_ARRAY ||
	  sp[2+ap-args].type!=T_INT)
	 error("illegal argument(s) %d, %d or %d\n",ap,ap+1,ap+2);

      if (sp[ap-args].u.array->size==3
	  && sp[ap-args].u.array->item[0].type==T_INT
	  && sp[ap-args].u.array->item[1].type==T_INT
	  && sp[ap-args].u.array->item[2].type==T_INT)
	 low.r=sp[ap-args].u.array->item[0].u.integer,
	 low.g=sp[ap-args].u.array->item[1].u.integer,
	 low.b=sp[ap-args].u.array->item[2].u.integer;
      else
	 low.r=low.g=low.b=0;

      if (sp[1+ap-args].u.array->size==3
	  && sp[1+ap-args].u.array->item[0].type==T_INT
	  && sp[1+ap-args].u.array->item[1].type==T_INT
	  && sp[1+ap-args].u.array->item[2].type==T_INT)
	 high.r=sp[1+ap-args].u.array->item[0].u.integer,
	 high.g=sp[1+ap-args].u.array->item[1].u.integer,
	 high.b=sp[1+ap-args].u.array->item[2].u.integer;
      else
	 high.r=high.g=high.b=0;

      steps=isteps=sp[2+ap-args].u.integer;
      ap+=3;

      if (steps<2) continue; /* no idea */

      c=0;
      o2steps=osteps=-1;
      for (;;)
      {
	 mdist=((SQ(low.r-high.r)+SQ(low.g-high.g)+SQ(low.b-high.b))
		/SQ(steps)) / ( 4 );
  			 /* 4 is for /2�, closest dot, always */
			 /* 6 makes a suitable constant */
	 if (mdist>cube.disttrig) mdist=cube.disttrig;

	 c=0;
	 for (i=0; i<steps; i++)
	 {
	    rgb_group rgb;
	    int dist,dummyno;

	    rgb.r=(unsigned char)((high.r*i+low.r*(steps-i-1))/(steps-1));
	    rgb.g=(unsigned char)((high.g*i+low.g*(steps-i-1))/(steps-1));
	    rgb.b=(unsigned char)((high.b*i+low.b*(steps-i-1))/(steps-1));

	    _find_cube_dist(cube,rgb,&dist,&dummyno,sf);

	    if (dist>mdist) c++; /* important! same test as below */
	 }

	 if (c>=isteps) break;
	 if (c==o2steps) break; /* sanity check */
	 o2steps=osteps;
	 osteps=c;
	 steps++; /* room for more */
      }

      if (isteps<c) { steps--; c=osteps; }
      isteps=c;

      cube.disttrig=mdist;

      s=malloc(sizeof(struct nct_scale)+sizeof(int)*steps);
      if (!s) continue;
      s->low=low;
      s->high=high;
      s->vector.r=high.r-(INT32)low.r;
      s->vector.g=high.g-(INT32)low.g;
      s->vector.b=high.b-(INT32)low.b;
      s->invsqvector=1.0/(SQ(s->vector.r)+SQ(s->vector.g)+SQ(s->vector.b));
      s->steps=steps;
      s->realsteps=isteps;
      s->mqsteps=1.0/(steps-1);

#ifdef COLORTABLE_DEBUG
      fprintf(stderr,"COLORTABLE %d steps, %d live, trig=%d\n",s->steps,s->realsteps,cube.disttrig);
#endif

      for (i=0; i<steps; i++)
      {
	 rgb_group rgb;
	 int dist,dummyno;

	 rgb.r=(unsigned char)((high.r*i+low.r*(steps-i))/(steps-1));
	 rgb.g=(unsigned char)((high.g*i+low.g*(steps-i))/(steps-1));
	 rgb.b=(unsigned char)((high.b*i+low.b*(steps-i))/(steps-1));

	 _find_cube_dist(cube,rgb,&dist,&dummyno,sf);

#ifdef COLORTABLE_DEBUG
	 fprintf(stderr,"COLORTABLE step %d: %02x%02x%02x s_no=%d dist=%d\n",i,rgb.r,rgb.g,rgb.b,dummyno,dist);
#endif

	 if (dist>cube.disttrig)  /* important! same test as above */
	    s->no[i]=no++;
	 else 
	    s->no[i]=dummyno;
      }
      
      *np=s;
      s->next=NULL;
      np=&(s->next);
   }
   cube.numentries=no;

   return cube;
}

static struct nct_flat _img_nct_cube_to_flat(struct nct_cube cube)
{
   struct nct_flat flat;
   int no;
   int r,g,b;
   struct nct_scale *s;

   flat.numentries=cube.numentries;
   flat.entries=(struct nct_flat_entry*)
      xalloc(sizeof(struct nct_flat_entry)*flat.numentries);

   no=0;

   if (cube.b && cube.g && cube.b)
      for (b=0; b<cube.b; b++)
	 for (g=0; g<cube.g; g++)
	    for (r=0; r<cube.r; r++)
	    {
	       flat.entries[no].color.r=(unsigned char)((0xff*r)/(cube.r-1));
	       flat.entries[no].color.g=(unsigned char)((0xff*g)/(cube.g-1));
	       flat.entries[no].color.b=(unsigned char)((0xff*b)/(cube.b-1));
	       flat.entries[no].no=no;
	       flat.entries[no].weight=cube.weight;
	       no++;
	    }

   s=cube.firstscale;
   while (s)
   {
      int i;
      for (i=0; i<s->steps; i++)
	 if (s->steps && s->no[i]>=no)
	 {
	    flat.entries[no].color.r=
	       (unsigned char)((s->high.r*i+s->low.r*(s->steps-i-1))/(s->steps-1));
	    flat.entries[no].color.g=
	       (unsigned char)((s->high.g*i+s->low.g*(s->steps-i-1))/(s->steps-1));
	    flat.entries[no].color.b=
	       (unsigned char)((s->high.b*i+s->low.b*(s->steps-i-1))/(s->steps-1));
	    flat.entries[no].no=no;
	    flat.entries[no].weight=cube.weight;
	    no++;
	 }
      s=s->next;
   }

   if (no!=cube.numentries) abort(); /* simple sanity check */

   return flat;
} 

static void _img_add_colortable(struct neo_colortable *rdest,
				struct neo_colortable *src)
{
   struct neo_colortable tmp1,tmp2;
   struct color_hash_entry *hash,*mark;
   unsigned long i,j,hashsize,k;
   struct nct_flat_entry *en;
   struct nct_flat flat;
   struct neo_colortable *dest=rdest;
   int no;

   tmp1.type=NCT_NONE;
   tmp2.type=NCT_NONE; /* easy free... */

   if (dest->type==NCT_NONE)
   {
      _img_copy_colortable(dest,src);
      return;
   }

   for (i=0; i<COLORLOOKUPCACHEHASHSIZE; i++)
      dest->lookupcachehash[i].index=-1;

   if (src->type==NCT_CUBE)
   {
      tmp1.u.flat=_img_nct_cube_to_flat(src->u.cube);
      tmp1.type=NCT_FLAT;
      src=&tmp1;
   }
   if (dest->type==NCT_CUBE)
   {
      tmp2.u.flat=_img_nct_cube_to_flat(dest->u.cube);
      tmp2.type=NCT_FLAT;
      dest=&tmp2;
   }

   hashsize=(dest->u.flat.numentries+src->u.flat.numentries)*2;
   hash=(struct color_hash_entry*)
      xalloc(sizeof(struct color_hash_entry)*hashsize);

   i=hashsize;
   while (i--) hash[i].pixels=0;

#if 0
   fprintf(stderr,"add\n");
   stderr_print_entries(src->u.flat.entries,src->u.flat.numentries);
   fprintf(stderr,"to\n");
   stderr_print_entries(dest->u.flat.entries,dest->u.flat.numentries);
#endif

   i=dest->u.flat.numentries;
   en=dest->u.flat.entries;

   while (i)
   {
      if (!(mark=insert_in_hash_nd(en->color,hash,hashsize)))
      {
	 struct color_hash_entry *oldhash=hash;
	 j=hashsize;

rerun_rehash_add_1:

	 hashsize*=2;

	 hash=malloc(sizeof(struct color_hash_entry)*hashsize);
	 if (!hash)
	 {
	    free(oldhash);
	    free_colortable_struct(&tmp1);
	    free_colortable_struct(&tmp2);
	    error("out of memory\n");
	 }
	 k=hashsize;
	 while (k--) hash[k].pixels=0;

	 while (j--) 
	    if (oldhash[j].pixels)
	    {
	       mark=insert_in_hash_nd(oldhash[j].color,hash,hashsize);
	       if (!mark) goto rerun_rehash_add_1;
	       mark->no=oldhash[i].no;
	       mark->pixels=oldhash[i].pixels;
	    }
	 
	 free(oldhash);
	 continue;
      }

      mark->no=en->no;
      mark->pixels+=en->weight;

      i--;
      en++;
   }

   i=src->u.flat.numentries;
   en=src->u.flat.entries;
   no=dest->u.flat.numentries;

   while (i)
   {
      if (!(mark=insert_in_hash_nd(en->color,hash,hashsize)))
      {
	 struct color_hash_entry *oldhash=hash;
	 j=hashsize;

rerun_rehash_add_2:

	 hashsize*=2;

	 hash=malloc(sizeof(struct color_hash_entry)*hashsize);
	 if (!hash)
	 {
	    free(oldhash);
	    free_colortable_struct(&tmp1);
	    free_colortable_struct(&tmp2);
	    error("out of memory\n");
	 }
	 i=hashsize;
	 while (i--) hash[i].pixels=0;

	 while (j--) 
	    if (oldhash[j].pixels)
	    {
	       mark=insert_in_hash_nd(oldhash[j].color,hash,hashsize);
	       if (!mark) goto rerun_rehash_add_2;
	       if (mark->pixels!=1)
		  mark->no=oldhash[i].no;
	       mark->pixels=oldhash[i].pixels;
	    }
	 
	 free(oldhash);
	 continue;
      }

      if (mark->pixels==WEIGHT_NEEDED || en->weight==WEIGHT_NEEDED) 
	 mark->pixels=WEIGHT_NEEDED;
      else
      {
	 if (!mark->pixels) mark->no=no++;
	 mark->pixels+=en->weight;
      }

      i--;
      en++;
   }

   /* convert to flat */

   i=hashsize;
   j=0;
   while (i--)
      if (hash[i].pixels) j++;
   /* j is now the number of colors */
   flat.numentries=j;
   flat.entries=malloc(sizeof(struct nct_flat_entry)*j);
   if (!flat.entries)
   {
      free(hash);
      error("out of memory\n");
   }
   j=0;
   i=hashsize;
   while (i--)
      if (hash[i].pixels)
      {
	 flat.entries[j].color=hash[i].color;
	 flat.entries[j].no=j;
	 flat.entries[j].weight=hash[i].pixels;
	 j++;
      }
   free(hash);

   free_colortable_struct(&tmp1);
   free_colortable_struct(&tmp2);

   free_colortable_struct(rdest);

   rdest->type=NCT_FLAT; 
   rdest->u.flat=flat;
}

static void _img_sub_colortable(struct neo_colortable *rdest,
				struct neo_colortable *src)
{
   struct neo_colortable tmp1,tmp2;
   struct color_hash_entry *hash,*mark;
   unsigned long i,j,hashsize,k;
   struct nct_flat_entry *en;
   struct nct_flat flat;
   struct neo_colortable *dest=rdest;
   int no;

   tmp1.type=NCT_NONE;
   tmp2.type=NCT_NONE; /* easy free... */

   if (dest->type==NCT_NONE)
   {
      _img_copy_colortable(dest,src);
      return;
   }

   for (i=0; i<COLORLOOKUPCACHEHASHSIZE; i++)
      dest->lookupcachehash[i].index=-1;

   if (src->type==NCT_CUBE)
   {
      tmp1.u.flat=_img_nct_cube_to_flat(src->u.cube);
      tmp1.type=NCT_FLAT;
      src=&tmp1;
   }
   if (dest->type==NCT_CUBE)
   {
      tmp2.u.flat=_img_nct_cube_to_flat(dest->u.cube);
      tmp2.type=NCT_FLAT;
      dest=&tmp2;
   }

   hashsize=(dest->u.flat.numentries+src->u.flat.numentries)*2;
   hash=(struct color_hash_entry*)
      xalloc(sizeof(struct color_hash_entry)*hashsize);

   i=hashsize;
   while (i--) hash[i].pixels=0;

#if 0
   fprintf(stderr,"add\n");
   stderr_print_entries(src->u.flat.entries,src->u.flat.numentries);
   fprintf(stderr,"to\n");
   stderr_print_entries(dest->u.flat.entries,dest->u.flat.numentries);
#endif

   i=dest->u.flat.numentries;
   en=dest->u.flat.entries;

   while (i)
   {
      if (!(mark=insert_in_hash_nd(en->color,hash,hashsize)))
      {
	 struct color_hash_entry *oldhash=hash;
	 j=hashsize;

rerun_rehash_add_1:

	 hashsize*=2;

	 hash=malloc(sizeof(struct color_hash_entry)*hashsize);
	 if (!hash)
	 {
	    free(oldhash);
	    free_colortable_struct(&tmp1);
	    free_colortable_struct(&tmp2);
	    error("out of memory\n");
	 }
	 k=hashsize;
	 while (k--) hash[k].pixels=0;

	 while (j--) 
	    if (oldhash[j].pixels)
	    {
	       mark=insert_in_hash_nd(oldhash[j].color,hash,hashsize);
	       if (!mark) goto rerun_rehash_add_1;
	       mark->no=oldhash[i].no;
	       mark->pixels=oldhash[i].pixels;
	    }
	 
	 free(oldhash);
	 continue;
      }

      mark->no=en->no;
      mark->pixels+=en->weight;

      i--;
      en++;
   }

   i=src->u.flat.numentries;
   en=src->u.flat.entries;
   no=dest->u.flat.numentries;

   while (i)
   {
      if (!(mark=insert_in_hash_nd(en->color,hash,hashsize)))
      {
	 struct color_hash_entry *oldhash=hash;
	 j=hashsize;

rerun_rehash_add_2:

	 hashsize*=2;

	 hash=malloc(sizeof(struct color_hash_entry)*hashsize);
	 if (!hash)
	 {
	    free(oldhash);
	    free_colortable_struct(&tmp1);
	    free_colortable_struct(&tmp2);
	    error("out of memory\n");
	 }
	 i=hashsize;
	 while (i--) hash[i].pixels=0;

	 while (j--) 
	    if (oldhash[j].pixels)
	    {
	       mark=insert_in_hash_nd(oldhash[j].color,hash,hashsize);
	       if (!mark) goto rerun_rehash_add_2;
	       if (mark->pixels!=1)
		  mark->no=oldhash[i].no;
	       mark->pixels=oldhash[i].pixels;
	    }
	 
	 free(oldhash);
	 continue;
      }

      mark->pixels=WEIGHT_REMOVE;

      i--;
      en++;
   }

   /* convert to flat */

   i=hashsize;
   j=0;
   while (i--)
      if (hash[i].pixels && hash[i].pixels!=WEIGHT_REMOVE) j++;
   /* j is now the number of colors */
   flat.numentries=j;
   flat.entries=malloc(sizeof(struct nct_flat_entry)*j);
   if (!flat.entries)
   {
      free(hash);
      error("out of memory\n");
   }
   j=0;
   i=hashsize;
   while (i--)
      if (hash[i].pixels && hash[i].pixels!=WEIGHT_REMOVE)
      {
	 flat.entries[j].color=hash[i].color;
	 flat.entries[j].no=j;
	 flat.entries[j].weight=hash[i].pixels;
	 j++;
      }
   free(hash);

   free_colortable_struct(&tmp1);
   free_colortable_struct(&tmp2);

   free_colortable_struct(rdest);

   rdest->type=NCT_FLAT; 
   rdest->u.flat=flat;
}

/***************** global methods ******************************/

/*
**! method void create()
**! method void create(array(array(int)) colors)
**! method void create(object(Image.image) image,int number)
**! method void create(object(Image.image) image,int number,array(array(int)) needed)
**! method void create(int r,int g,int b)
**! method void create(int r,int g,int b, array(int) from1,array(int) to1,int steps1, ..., array(int) fromn,array(int) ton,int stepsn)
**! method object add(array(array(int)) colors)
**! method object add(object(Image.image) image,int number)
**! method object add(object(Image.image) image,int number,array(array(int)) needed)
**! method object add(int r,int g,int b)
**! method object add(int r,int g,int b, array(int) from1,array(int) to1,int steps1, ..., array(int) fromn,array(int) ton,int stepsn)
**!
**!	<ref>create</ref> initiates a colortable object. 
**!	Default is that no colors are in the colortable. 
**!
**!	<ref>create</ref> can also take the same arguments
**!	as <ref>add</ref>, thus adding colors to the colortable.
**!
**!	The colortable is mostly a list of colors,
**!	or more advanced, colors and weight.
**!
**!	The colortable could also be a colorcube, with or 
**!	without additional scales. A colorcube is the by-far 
**!	fastest way to find colors. 
**!
**!	Example:
**!	<pre>
**!	ct=colortable(my_image,256); // the best 256 colors
**!	ct=colortable(my_image,255,({0,0,0})); // black and the best other 255
**!
**!	ct=colortable(({({0,0,0}),({255,255,255})})); // black and white
**!
**!	ct=colortable(6,7,6); // a colortable of 252 colors
**!	ct=colortable(7,7,5, ({0,0,0}),({255,255,255}),11); 
**!		// a colorcube of 245 colors, and a greyscale of the rest -> 256
**!	</pre>
**!	
**! arg array(array(int)) colors
**!	list of colors 
**! arg object(Image.image) image
**!	source image 
**!
**!	note: you may not get all colors from image,
**!	max hash size is (probably, set by a <tt>#define</tt>) 
**!	32768 entries, giving
**!	maybe half that number of colors as maximum.
**! arg int number
**!	number of colors to get from the image
**!	
**!	0 (zero) gives all colors in the image.
**!
**!	Default value is 256.
**! arg array(array(int)) needed
**!	needed colors (to optimize selection of others to these given)
**!	
**!	this will add to the total number of colors (see argument 'number')
**! arg int r
**! arg int g
**! arg int b
**!	size of sides in the colorcube, must (of course) be equal
**!	or larger than 2 - if smaller, the cube is ignored (no colors).
**!	This could be used to have only scales (like a greyscale)
**!	in the output.
**! arg array(int) fromi
**! arg array(int) toi
**! arg int stepi
**!	This is to add the possibility of adding a scale
**!	of colors to the colorcube; for instance a grayscale
**!	using the arguments <tt>({0,0,0}),({255,255,255}),17</tt>,
**!	adding a scale from black to white in 17 or more steps.
**!
**!	Colors already in the cube is used again to add the number
**!	of steps, if possible. 
**!
**!	The total number of colors in the table is therefore
**!	<tt>r*b*g+step1+...+stepn</tt>.
*/

static void image_colortable_add(INT32 args)
{
   if (!args) 
   {
      pop_n_elems(args);
      push_object(THISOBJ); THISOBJ->refs++;
      return;
   }
   
   if (THIS->type!=NCT_NONE)
   {
      struct object *o;

      if (sp[-args].type==T_OBJECT)
      {
	 struct neo_colortable *ct2;
	 ct2=(struct neo_colortable*)
	    get_storage(sp[-args].u.object,image_colortable_program);
	 if (ct2)
	 {
#ifdef COLORTABLE_DEBUG
	    fprintf(stderr,"COLORTABLE added %lx and %lx to %lx (args=%d)\n",
		           THIS,ct2,THIS,args);
#endif
	    _img_add_colortable(THIS,ct2);
	    pop_n_elems(args);
	    push_object(THISOBJ); THISOBJ->refs++;
	    return;
	 }
      }

#ifdef COLORTABLE_DEBUG
      fprintf(stderr,
	      "COLORTABLE %lx isn't empty; create new and add (args=%d)\n",
	      THIS,args);
#endif
      o=clone_object(image_colortable_program,args);
      push_object(o);
      image_colortable_add(1);

#ifdef COLORTABLE_DEBUG
      fprintf(stderr,
	      "COLORTABLE done (%lx isn't empty...)\n",
	      THIS,args);
#endif
      return;
   }

   /* initiate */
#ifdef COLORTABLE_DEBUG
   fprintf(stderr,"COLORTABLE %lx created with %d args, sp-args=%lx\n",THIS,args,sp-args);
#endif

   if (sp[-args].type==T_OBJECT)
   {
      struct neo_colortable *ct2;
      struct image *img;

      if ((ct2=(struct neo_colortable*)
	   get_storage(sp[-args].u.object,image_colortable_program)))
      {
	 /* just copy that colortable */
	 _img_copy_colortable(THIS,ct2);
      }
      else if ((img=(struct image*)
		get_storage(sp[-args].u.object,image_program)))
      {
	 /* get colors from image */
	 if (args>=2)
	    if (sp[1-args].type==T_INT)
	    {
	       THIS->u.flat=_img_get_flat_from_image(img,
						     sp[1-args].u.integer);
	       THIS->type=NCT_FLAT;
	       if (args>2)
	       {
		  struct object *o;
		  struct neo_colortable *nct;

		  o=clone_object(image_colortable_program,args-2);

		  nct=(struct neo_colortable*)
		     get_storage(o,image_colortable_program);

		  if (!nct) abort();

		  if (nct->type==NCT_CUBE)
		     nct->u.cube.weight=WEIGHT_NEEDED;
		  else if (nct->type==NCT_FLAT)
		  {
		     unsigned long i;
		     i=nct->u.flat.numentries;
		     while (i--)
			nct->u.flat.entries[i].weight=WEIGHT_NEEDED;
		  }

		  push_object(o);
		  image_colortable_add(1);
		  pop_n_elems(1);
		  /* we will keep flat... */
		  args=2;
	       }

	       if (sp[1-args].u.integer>0 && 
		   THIS->u.flat.numentries>sp[1-args].u.integer)
		  THIS->u.flat=
		     _img_reduce_number_of_colors(THIS->u.flat,
						  sp[1-args].u.integer,
						  THIS->spacefactor);
	    }
	    else 
	       error("Illegal argument 2 to Image.colortable->add|create\n");
	 else
	 {
	    THIS->u.flat=_img_get_flat_from_image(img,256); 
	    if (THIS->u.flat.numentries>256)
	       THIS->u.flat=_img_reduce_number_of_colors(THIS->u.flat,256,
							 THIS->spacefactor);
	    THIS->type=NCT_FLAT;
	 }
      }
      else error("Illegal argument 1 to Image.colortable->add|create\n");
   }
   else if (sp[-args].type==T_ARRAY)
   {
      THIS->u.flat=_img_get_flat_from_array(sp[-args].u.array);
      THIS->type=NCT_FLAT;
   }
   else if (sp[-args].type==T_INT)
   {
      THIS->u.cube=_img_get_cube_from_args(args);
      THIS->type=NCT_CUBE;
   }
   else error("Illegal argument(s) to Image.colortable->add|create\n");
   pop_n_elems(args);
   push_object(THISOBJ); THISOBJ->refs++;

#ifdef COLORTABLE_DEBUG
   fprintf(stderr,"COLORTABLE done (%lx created, %d args was left, sp-1=%lx)\n",THIS,args,sp-1);
#endif

}

void image_colortable_create(INT32 args)
{
   if (args)  /* optimize */
      image_colortable_add(args); 
}

/*
**! method object reduce(int colors)
**!	reduces the number of colors
**!
**!	All needed (see <ref>create</ref>) colors are kept.
**! returns the new <ref>colortable</ref> object
**!
**! arg int colors
**!	target number of colors
**/

void image_colortable_reduce(INT32 args)
{
   struct object *o;
   struct neo_colortable *nct;

   if (!args) error("Missing argument to Image.colortable->reduce\n");
   if (sp[-args].type!=T_INT) error("Illegal argument to Image.colortable->reduce\n");
   
   o=clone_object(THISOBJ->prog,0);
   nct=(struct neo_colortable*)get_storage(o,image_colortable_program);
   
   switch (nct->type = THIS->type)
   {
      case NCT_NONE: pop_n_elems(args); push_object(o); return;
      case NCT_CUBE:
         nct->type=NCT_FLAT;
         nct->u.flat=_img_nct_cube_to_flat(THIS->u.cube);
	 break;
      case NCT_FLAT:
         _img_copy_colortable(nct,THIS);
	 break;
   }

   if (sp[-args].u.integer<1) sp[-args].u.integer=1;

   nct->u.flat=_img_reduce_number_of_colors(nct->u.flat,sp[-args].u.integer,
					    nct->spacefactor);

   pop_n_elems(args);
   push_object(o);
}


/*
**! method object `+(object with,...)
**!	sums colortables
**! returns the resulting new <ref>colortable</ref> object
**!
**! arg object(<ref>colortable</ref>) with
**!	<ref>colortable</ref> object with colors to add
**/

void image_colortable_operator_plus(INT32 args)
{
   struct object *o;
   struct neo_colortable *dest,*src;

   int i;

   THISOBJ->refs++;
   push_object(THISOBJ);
   o=clone_object(THISOBJ->prog,1);
   dest=(struct neo_colortable*)get_storage(o,image_colortable_program);

   for (i=0; i<args; i++)
      if (sp[i-args].type==T_OBJECT)
      {
	 src=(struct neo_colortable*)
	    get_storage(sp[i-args].u.object,image_colortable_program);
	 if (!src) 
	 { 
	    free_object(o); 
	    error("Illegal argument %d to Image.colortable->`+",i+2); 
	 }
	 _img_add_colortable(dest,src);
      }
      else 
      { 
	 free_object(o); 
	 error("Illegal argument %d to Image.colortable->`+",i+2); 
      }
   pop_n_elems(args);
   push_object(o);
}

/*
**! method object `-(object with,...)
**!	subtracts colortables
**! returns the resulting new <ref>colortable</ref> object
**!
**! arg object(<ref>colortable</ref>) with
**!	<ref>colortable</ref> object with colors to subtract
**/

void image_colortable_operator_minus(INT32 args)
{
   struct object *o;
   struct neo_colortable *dest,*src;

   int i;

   THISOBJ->refs++;
   push_object(THISOBJ);
   o=clone_object(THISOBJ->prog,1);
   dest=(struct neo_colortable*)get_storage(o,image_colortable_program);

   for (i=0; i<args; i++)
      if (sp[i-args].type==T_OBJECT)
      {
	 src=(struct neo_colortable*)
	    get_storage(sp[i-args].u.object,image_colortable_program);
	 if (!src) 
	 { 
	    free_object(o); 
	    error("Illegal argument %d to Image.colortable->`-",i+2); 
	 }
	 _img_sub_colortable(dest,src);
      }
      else 
      { 
	 free_object(o); 
	 error("Illegal argument %d to Image.colortable->`-",i+2); 
      }
   pop_n_elems(args);
   push_object(o);
}

void image_colortable_cast_to_array(INT32 args)
{
   struct nct_flat flat;
   int i;
   
   pop_n_elems(args);
   
   if (THIS->type==NCT_NONE)
   {
      f_aggregate(0);
      return;
   }

   if (THIS->type==NCT_CUBE)
      flat=_img_nct_cube_to_flat(THIS->u.cube);
   else
      flat=THIS->u.flat;

   /* sort in number order? */

   for (i=0; i<flat.numentries; i++)
   {
      push_int(flat.entries[i].color.r);
      push_int(flat.entries[i].color.g);
      push_int(flat.entries[i].color.b);
      f_aggregate(3);
   }
   f_aggregate(flat.numentries);

   if (THIS->type==NCT_CUBE)
      free(flat.entries);
}

/*
**! method object cast(string to)
**!	cast the colortable to an array
**!
**!	example: <tt>(array)Image.colortable(img)</tt>
**! returns the resulting array
**!
**! arg string to
**!	must be "array".
**!
**! bugs
**!	ignores argument (ie <tt>(string)colortable</tt> gives an array)
**/

void image_colortable_cast(INT32 args)
{
   if (!args ||
       sp[-args].type!=T_STRING) 
      error("Illegal argument 1 to Image.colortable->cast\n");

   /* CHECK TYPE TO CAST TO HERE! FIXME FIXME FIXME! */

   image_colortable_cast_to_array(args);
}

/*
**! method object tree()
**!	Set the colortable to use a tree algorithm to find
**!     the best color.
**!
**!	example: <tt>colors=Image.colortable(img)->tree</tt>
**!
**!     algorithm time: O[ln n], where n is numbers of colors
**!
**! returns the called object
**!
**! note
**!     This method doesn't figure out the tree, this is 
**!     done on the first use of the colortable.
**!
**!     Not applicable to colorcube types of colortable.
**/

void image_colortable_tree(INT32 args)
{
   if (THIS->lookup_mode!=NCT_TREE) 
   {
      colortable_free_lookup_stuff(THIS);
      THIS->lookup_mode=NCT_TREE;
   }
   pop_n_elems(args);
   push_object(THISOBJ); THISOBJ->refs++;
}

/*
**! method object full()
**!	Set the colortable to use full scan to lookup the closest color.
**!
**!	example: <tt>colors=Image.colortable(img)->full();</tt>
**!
**!     algorithm time: O[n*m], where n is numbers of colors 
**!	and m is number of pixels
**!
**! returns the called object
**!
**! note
**!     Not applicable to colorcube types of colortable.
**/

void image_colortable_full(INT32 args)
{
   if (THIS->lookup_mode!=NCT_FULL) 
   {
      colortable_free_lookup_stuff(THIS);
      THIS->lookup_mode=NCT_FULL;
   }
   pop_n_elems(args);
   push_object(THISOBJ); THISOBJ->refs++;
}

/*
**! method object cubicles()
**! method object cubicles(int r,int g,int b)
**! method object cubicles(int r,int g,int b,int accuracy)
**!	Set the colortable to use the cubicles algorithm to lookup
**!     the closest color. This is a mostly very fast and very
**!     accurate way to find the correct color, and the default
**!     algorithm.
**!
**!
**!	The colorspace is divided in small cubes, each cube
**!	containing the colors in that cube. Each cube then gets
**!	a list of the colors in the cube, and the closest from
**!	the corners and midpoints between corners.  
**!
**!	When a color is needed, the algorithm first finds the
**!	correct cube and then compares with all the colors in
**!	the list for that cube.
**!
**!	example: <tt>colors=Image.colortable(img)->cubicles();</tt>
**!
**!     algorithm time: between O[m] and O[m * n], 
**!	where n is numbers of colors and m is number of pixels
**!
**!	The arguments can be heavy trimmed for the usage
**!	of your colortable; a large number (10�10�10 or bigger)
**!	of cubicles is recommended when you use the colortable
**!	repeatedly, since the calculation takes much
**!	more time then usage.
**!
**!	In some cases, the <ref>full</ref> method is faster.
**!
**! returns the called object
**!
**! arg int r
**! arg int g
**! arg int b
**!     Size, ie how much the colorspace is divided.
**!     Note that the size of each cubicle is at least about 8b,
**!     and that it takes time to calculate them. The number of
**!     cubicles are <tt>r*g*b</tt>, and default is 4,5,4,
**!     ie 80 cubicles. This works good for 200�100 colors.
**!
**! arg int accuracy
**!	Accuracy when checking sides of cubicles.
**!	Default is 16. A value of 1 gives complete accuracy,
**!	ie cubicle() method gives exactly the same result
**!	as full(), but takes (in worst case) 16� the time
**!	to calculate.
**!
**! note
**!     this method doesn't figure out the cubicles, this is 
**!     done on the first use of the colortable
**!
**!     Not applicable to colorcube types of colortable.
**/

void image_colortable_cubicles(INT32 args)
{
   if (THIS->lookup_mode!=NCT_CUBICLES) 
   {
      colortable_free_lookup_stuff(THIS);
      THIS->lookup_mode=NCT_CUBICLES;
   }
   if (args)
      if (args>=3 && 
	  sp[-args].type==T_INT &&
	  sp[2-args].type==T_INT &&
	  sp[1-args].type==T_INT)
      {
	 THIS->lu.cubicles.r=MAX(sp[-args].u.integer,1);
	 THIS->lu.cubicles.g=MAX(sp[1-args].u.integer,1);
	 THIS->lu.cubicles.b=MAX(sp[2-args].u.integer,1);
	 if (args>=4 &&
	     sp[3-args].type==T_INT)
	    THIS->lu.cubicles.accur=MAX(sp[3-args].u.integer,1);
	 else
	    THIS->lu.cubicles.accur=CUBICLE_DEFAULT_ACCUR;
      }
      else
	 error("Illegal arguments to colortable->cubicles()\n");
   else
   {
      THIS->lu.cubicles.r=CUBICLE_DEFAULT_R;
      THIS->lu.cubicles.g=CUBICLE_DEFAULT_G;
      THIS->lu.cubicles.b=CUBICLE_DEFAULT_B;
      THIS->lu.cubicles.accur=CUBICLE_DEFAULT_ACCUR;
   }

   pop_n_elems(args);
   push_object(THISOBJ); THISOBJ->refs++;
}

/*
**! method object map(object image)
**! method object `*(object image)
**! method object ``*(object image)
**!	Map colors in an image object to the colors in 
**!     the colortable, and creates a new image with the
**!     closest colors. 
**!
**! returns a new image object
**!
**! note
**!     Flat (not cube) colortable and not '<ref>full</ref>' method: 
**!     this method does figure out the data needed for
**!	the lookup method, which may take time the first
**!	use of the colortable - the second use is quicker.
**!
**! see also
**!     cubicle, tree, full
**/

static void _img_nct_map_to_cube(rgb_group *s,
				 rgb_group *d,
				 int n,
				 struct neo_colortable *nct)
{
   int red,green,blue;
   int hred,hgreen,hblue;
   int redm,greenm,bluem;
   float redf,greenf,bluef;
   struct nct_cube *cube=&(nct->u.cube);
   rgbl_group sf=nct->spacefactor;

   red=cube->r; 	hred=red/2;      redm=red-1;
   green=cube->g;	hgreen=green/2;  greenm=green-1;
   blue=cube->b; 	hblue=blue/2;    bluem=blue-1;

   redf=255.0/redm;
   greenf=255.0/greenm;
   bluef=255.0/bluem;

   CHRONO("begin cube map");

   if (!cube->firstscale && red && green && blue)
   {
      while (n--)
      {
	 d->r=((int)(((s->r*red+hred)>>8)*redf));
	 d->g=((int)(((s->g*green+hgreen)>>8)*greenf));
	 d->b=((int)(((s->b*blue+hblue)>>8)*bluef));

	 d++;
	 s++;
      }
   }
   else
   {
      while (n--) /* similar to _find_cube_dist() */
      {
	 struct nct_scale *sc;
	 int mindist;
	 int i;
	 char *n;
	 int nc;
	 int rgbr,rgbg,rgbb;
	 int drgbr,drgbg,drgbb;
	 struct lookupcache *lc;
	 rgbr=s->r;
	 rgbg=s->g;
	 rgbb=s->b;

	 lc=nct->lookupcachehash+COLORLOOKUPCACHEHASHVALUE(rgbr,rgbg,rgbb);
	 if (lc->index!=-1 &&
	     lc->src.r==rgbr &&
	     lc->src.g==rgbg &&
	     lc->src.b==rgbb)
	 {
	    *(d++)=lc->dest;
	    s++;
	    continue;
	 }

	 lc->src=*s;

	 if (red && green && blue)
	 {
	    lc->dest.r=d->r=((int)(((rgbr*red+hred)>>8)*redf));
	    lc->dest.g=d->g=((int)(((rgbg*green+hgreen)>>8)*greenf));
	    lc->dest.b=d->b=((int)(((rgbb*blue+hblue)>>8)*bluef));

	    i=((rgbr*red+hred)>>8)+
	       ((rgbg*green+hgreen)>>8)*red+
	       ((rgbb*blue+hblue)>>8)*red*green;

	    mindist=sf.r*SQ(rgbr-d->r)+sf.g*SQ(rgbg-d->g)+sf.b*SQ(rgbb-d->b);
	 }
	 else
	 {
	    mindist=10000000;
	    i=0;
	 }

	 if (mindist>=cube->disttrig)
	 {
	    /* check scales to get better distance if possible */

	    nc=cube->r*cube->g*cube->b;
	    sc=cube->firstscale;
	    while (sc)
	    {
	       /* what step is closest? project... */

	       i=(int)
		  (( sc->steps *
		     ( ((int)rgbr-sc->low.r)*sc->vector.r +
		       ((int)rgbg-sc->low.g)*sc->vector.g +
		       ((int)rgbb-sc->low.b)*sc->vector.b ) ) *
		   sc->invsqvector);

	       if (i<0) i=0; else if (i>=sc->steps) i=sc->steps-1;
	       if (sc->no[i]>=nc) 
	       {
		  float f=i*sc->mqsteps;
		  int drgbr=sc->low.r+(int)(sc->vector.r*f);
		  int drgbg=sc->low.g+(int)(sc->vector.g*f);
		  int drgbb=sc->low.b+(int)(sc->vector.b*f);

		  int ldist=sf.r*SQ(rgbr-drgbr)+
		     sf.g*SQ(rgbg-drgbg)+sf.b*SQ(rgbb-drgbb);

	       
		  if (ldist<mindist)
		  {
		     lc->dest.r=d->r=(unsigned char)drgbr;
		     lc->dest.g=d->g=(unsigned char)drgbg;
		     lc->dest.b=d->b=(unsigned char)drgbb;
		     lc->index=i;
		     mindist=ldist;
		  }
	       }
	    
	       nc+=sc->realsteps;
	    
	       sc=sc->next;
	    }
	 }
	 
	 d++;
	 s++;
      }
   }
   CHRONO("end cube map");
}

static void _img_nct_map_to_flat_full(rgb_group *s,
				      rgb_group *d,
				      int n,
				      struct neo_colortable *nct)
{
   /* no need to build any data, we're using full scan */

   rgbl_group sf=nct->spacefactor;
   int mprim=nct->u.flat.numentries;
   struct nct_flat_entry *feprim=nct->u.flat.entries;

   CHRONO("begin flat/full map");

   while (n--)
   {
      int rgbr,rgbg,rgbb;
      int mindist;
      int m;
      struct nct_flat_entry *fe;
      struct lookupcache *lc;
	 
      rgbr=s->r;
      rgbg=s->g;
      rgbb=s->b;

      /* cached? */
      lc=nct->lookupcachehash+COLORLOOKUPCACHEHASHVALUE(rgbr,rgbg,rgbb);
      if (lc->index!=-1 &&
	  lc->src.r==rgbr &&
	  lc->src.g==rgbg &&
	  lc->src.b==rgbb)
      {
	 *(d++)=lc->dest;
	 s++;
	 continue;
      }

      lc->src=*s;

      mindist=256*256*100; /* max dist is 256�*3 */
      
      fe=feprim;
      m=mprim;
      
      while (m--)
      {
	 int dist=sf.r*SQ(fe->color.r-rgbr)+
	          sf.g*SQ(fe->color.g-rgbg)+
	          sf.b*SQ(fe->color.b-rgbb);
	 
	 if (dist<mindist)
	 {
	    lc->dest=*d=fe->color;
	    mindist=dist;
	    lc->index=fe->no;
	 }
	 
	 fe++;
      }

      s++;
      d++;
   }

   CHRONO("end flat/full map");
}

static  int _cub_find_2cub_add(int *i,int *p,
			       int *p2,int n2,
			       struct nct_flat_entry *fe,
			       rgbl_group sf,
			       int r,int g,int b)
{
   int mindist=256*256*100; /* max dist is 256�*3 */
   int c=0;
   int *p1=p;
   int n=*i;
   int k=1;

   while (n--)
   {
      int dist=sf.r*SQ(fe[*p1].color.r-r)+
	       sf.g*SQ(fe[*p1].color.g-g)+
	       sf.b*SQ(fe[*p1].color.b-b);
      
      if (dist<mindist)
      {
	 c=*p1;
	 mindist=dist;
	 if (!dist) break;
      }

      p1++;
   }
   if (mindist) while (n2--)
   {
      int dist=sf.r*SQ(fe[*p2].color.r-r)+
	       sf.g*SQ(fe[*p2].color.g-g)+
	       sf.b*SQ(fe[*p2].color.b-b);
      
      if (dist<mindist)
      {
	 c=*p2;
	 k=0;
	 if (!dist) break;
	 mindist=dist;
      }

      p2++;
   }

   if (!k)
   {
      n=*i;
      while (n--)
	 if (*p==c) return c; else p++;

      *p=c;
      (*i)++;
   }

   return c;
}

static void _cub_add_cs_2cub_recur(int *i,int *p,
				   int *p2,int n2,
				   struct nct_flat_entry *fe,
				   int rp,int gp,int bp,
				   int rd1,int gd1,int bd1,
				   int rd2,int gd2,int bd2,
				   int *a,int *b,int *c,int *d,
				   rgbl_group sf,
				   int accur)
{

/* a-h-b -> 2
 | |   |
 v e f g
 1 |   |
   c-j-d */

   int e=-1,f=-1,g=-1,h=-1,j=-1;
   int rm1,gm1,bm1;
   int rm2,gm2,bm2;

   if (*a==-1) *a=_cub_find_2cub_add(i,p,p2,n2,fe,sf, rp,gp,bp); /* 0,0 */   
   if (*b==-1) *b=_cub_find_2cub_add(i,p,p2,n2,fe,sf,
                		     rp+rd2,gp+gd2,bp+bd2); /* 0,1 */
   if (*c==-1) *c=_cub_find_2cub_add(i,p,p2,n2,fe,sf,
                		     rp+rd1,gp+gd1,bp+bd1); /* 1,0 */
   if (*d==-1) *d=_cub_find_2cub_add(i,p,p2,n2,fe,sf,
				   rp+rd2+rd1,gp+gd2+gd1,bp+bd2+bd1); /* 1,1 */

   if (rd1+gd1+bd1<=accur && rd2+gd2+bd2<=accur) return;

   if (*a==*b) h=*a;
   if (*c==*d) j=*c;

   if (h!=-1 && h==j) return; /* all done */

   if (*a==*c) e=*a;
   if (*b==*d) g=*b;
   if (*a==*d) f=*a;
   if (*b==*c) f=*b;

   rm1=rd1-(rd1>>1); rd1>>=1;
   gm1=gd1-(gd1>>1); gd1>>=1;
   bm1=bd1-(bd1>>1); bd1>>=1;
   rm2=rd2-(rd2>>1); rd2>>=1;
   gm2=gd2-(gd2>>1); gd2>>=1;
   bm2=bd2-(bd2>>1); bd2>>=1;

   _cub_add_cs_2cub_recur(i,p,p2,n2,fe, rp,gp,bp, rd1,gd1,bd1, rd2,gd2,bd2, a,&h,&e,&f,sf,accur);
   _cub_add_cs_2cub_recur(i,p,p2,n2,fe, rp+rd2,gp+gd2,bp+bd2, rd2?rm1:rd1,gd2?gm1:gd1,bd2?bm1:bd1, rd2?rm2:rd2,gd2?gm2:gd2,bd2?bm2:bd2, &h,b,&f,&g,sf,accur);
   _cub_add_cs_2cub_recur(i,p,p2,n2,fe, rp+rd1,gp+gd1,bp+bd1, rd1?rm1:rd1,gd1?gm1:gd1,bd1?bm1:bd1, rd1?rm2:rd2,gd1?gm2:gd2,bd1?bm2:bd2, &e,&f,c,&j,sf,accur);
   _cub_add_cs_2cub_recur(i,p,p2,n2,fe, rp+rd2+rd1,gp+gd2+gd1,bp+bd2+bd1, rm1,gm1,bm1, rm2,gm2,bm2, &f,&g,&j,d,sf,accur);
}

static INLINE int _cub_find_full_add(int **pp,int *i,int *p,int n,
				     struct nct_flat_entry *fe,
				     int r,int g,int b,
				     rgbl_group sf)
{
   int mindist=256*256*100; /* max dist is 256�*3 */
   int c=0;

   while (n--)
   {
      int dist=sf.r*SQ(fe->color.r-r)+
	       sf.g*SQ(fe->color.g-g)+
	       sf.b*SQ(fe->color.b-b);
      
      if (dist<mindist)
      {
	 c=fe->no;
	 if (!dist) break;
	 mindist=dist;
      }

      fe++;
   }

   n=*i;
   while (n--)
      if (*p==c) return c; else p++;

   *p=c;
   (*i)++;
   (*pp)++;

   return c;
}

static void _cub_add_cs_full_recur(int **pp,int *i,int *p,
				   int n,struct nct_flat_entry *fe,
				   int rp,int gp,int bp,
				   int rd1,int gd1,int bd1,
				   int rd2,int gd2,int bd2,
				   int *a,int *b,int *c,int *d,
				   rgbl_group sf,
				   int accur)
{

/*
   a-h-b -> 2
 | |   |
 v e f g
 1 |   |
   c-i-d
 */

   int e,f,g,h,j;
   int rm1,gm1,bm1;
   int rm2,gm2,bm2;

#if 0
   fprintf(stderr,"%*s_cub_add_cs_full_recur #%02x%02x%02x, %d,%d,%d, %d,%d,%d %d,%d,%d,%d->",
	   rlvl,"",
	   rp,gp,bp,rd1,gd1,bd1,rd2,gd2,bd2,*a,*b,*c,*d);
#endif

   if (*a==-1) *a=_cub_find_full_add(pp,i,p,n,fe,rp,gp,bp,sf); /* 0,0 */   
   if (*b==-1) *b=_cub_find_full_add(pp,i,p,n,fe,rp+rd2,gp+gd2,bp+bd2,sf); /* 0,1 */
   if (*c==-1) *c=_cub_find_full_add(pp,i,p,n,fe,rp+rd1,gp+gd1,bp+bd1,sf); /* 1,0 */
   if (*d==-1) *d=_cub_find_full_add(pp,i,p,n,fe,rp+rd2+rd1, gp+gd2+gd1,bp+bd2+bd1,sf); /* 1,1 */

#if 0
   fprintf(stderr,"%d,%d,%d,%d\n",*a,*b,*c,*d);
#endif

   if (rd1+gd1+bd1<=accur && rd2+gd2+bd2<=accur) return;

   if (*a==*b) h=*a; else h=-1;
   if (*c==*d) j=*c; else j=-1;

   if (h!=-1 && h==j) return; /* all done */

   if (*a==*c) e=*a; else e=-1;
   if (*b==*d) g=*b; else g=-1;
   if (*a==*d) f=*a;
   else if (*b==*c) f=*b;
   else f=-1;

   rm1=rd1>>1; rd1-=rm1;
   gm1=gd1>>1; gd1-=gm1;
   bm1=bd1>>1; bd1-=bm1;
   rm2=rd2>>1; rd2-=rm2;
   gm2=gd2>>1; gd2-=gm2;
   bm2=bd2>>1; bd2-=bm2;
   
   _cub_add_cs_full_recur(pp,i,p,n,fe, rp,gp,bp, rd1,gd1,bd1, rd2,gd2,bd2, a,&h,&e,&f,sf,accur);
   _cub_add_cs_full_recur(pp,i,p,n,fe, rp+rd2,gp+gd2,bp+bd2, rd2?rm1:rd1,gd2?gm1:gd1,bd2?bm1:bd1, rd2?rm2:rd2,gd2?gm2:gd2,bd2?bm2:bd2, &h,b,&f,&g,sf,accur);
   _cub_add_cs_full_recur(pp,i,p,n,fe, rp+rd1,gp+gd1,bp+bd1, rd1?rm1:rd1,gd1?gm1:gd1,bd1?bm1:bd1, rd1?rm2:rd2,gd1?gm2:gd2,bd1?bm2:bd2, &e,&f,c,&j,sf,accur);
   _cub_add_cs_full_recur(pp,i,p,n,fe, rp+rd2+rd1,gp+gd2+gd1,bp+bd2+bd1, rm1,gm1,bm1, rm2,gm2,bm2, &f,&g,&j,d,sf,accur);
}

static INLINE void _cub_add_cs(struct neo_colortable *nct,
			       struct nctlu_cubicle *cub,
			       int **pp,int *i,int *p,
			       int ri,int gi,int bi,
			       int red,int green,int blue,
			       int rp,int gp,int bp,
			       int rd1,int gd1,int bd1,
			       int rd2,int gd2,int bd2)
{
   int a=-1,b=-1,c=-1,d=-1;
#if 0
   fprintf(stderr,
	   " _cub_add_cs %d,%d,%d %d,%d,%d, %d,%d,%d, %d,%d,%d, %d,%d,%d\n",
	   ri,gi,bi,red,green,blue,rp,gp,bp,rd1,gd1,bd1,rd2,gd2,bd2);
#endif

   if (ri<0||gi<0||bi<0||ri>=red||gi>=green||bi>=blue) 
      return; /* no, colorspace ends here */

#if 0
   if (nct->lu.cubicles.cubicles[ri+gi*red+bi*red*green].index) 
      /* use the fact that the cube besides is known */
      _cub_add_cs_2cub_recur(i,p,
			     nct->lu.cubicles.cubicles[ri+gi*red+bi*red*green].index,
			     nct->lu.cubicles.cubicles[ri+gi*red+bi*red*green].n,
			     nct->u.flat.entries,
			     rp,gp,bp, rd1,gd1,bd1, rd2,gd2,bd2,
			     &a,&b,&c,&d,
			     nct->spacefactor,
			     nct->lu.cubicles.accur);
   else
#endif
      _cub_add_cs_full_recur(pp,i,p,
			     nct->u.flat.numentries,
			     nct->u.flat.entries,
			     rp,gp,bp, rd1,gd1,bd1, rd2,gd2,bd2,
			     &a,&b,&c,&d,
			     nct->spacefactor,
			     nct->lu.cubicles.accur);
}

static INLINE void _build_cubicle(struct neo_colortable *nct,
				  int r,int g,int b,
				  int red,int green,int blue,
				  struct nctlu_cubicle *cub)
{
   int rmin,rmax;
   int gmin,gmax;
   int bmin,bmax;

   struct nct_flat_entry *fe=nct->u.flat.entries;
   int n=nct->u.flat.numentries;

   int i=0;
   int *p=malloc(n*sizeof(struct nctlu_cubicle));
   int *pp,*pi; /* write, read */

   if (!p) error("out of memory (kablamm, typ) in _build_cubicle in colortable->map()\n");

   rmin=(r*256)/red;   rmax=((r+1)*256)/red-1;
   gmin=(g*256)/green; gmax=((g+1)*256)/green-1;
   bmin=(b*256)/blue;  bmax=((b+1)*256)/blue-1;

#if 0
   fprintf(stderr,"build cubicle %d,%d,%d #%02x%02x%02x-#%02x%02x%02x...",
	   r,g,b,rmin,gmin,bmin,rmax-1,gmax-1,bmax-1);
#endif

   pp=p;

   while (n--)
   {
      if (fe->color.r>=rmin && fe->color.r<=rmax &&
	  fe->color.g>=gmin && fe->color.g<=gmax &&
	  fe->color.b>=bmin && fe->color.b<=bmax)
      {
	 *pp=fe->no;
	 pp++; i++;
      }

      fe++;
   }

   /* add closest to sides */
   _cub_add_cs(nct,cub,&pp,&i,p,r-1,g,b,red,green,blue,rmin,gmin,bmin,0,gmax-gmin,0,0,0,bmax-bmin);
   _cub_add_cs(nct,cub,&pp,&i,p,r,g-1,b,red,green,blue,rmin,gmin,bmin,rmax-rmin,0,0,0,0,bmax-bmin);
   _cub_add_cs(nct,cub,&pp,&i,p,r,g,b-1,red,green,blue,rmin,gmin,bmin,rmax-rmin,0,0,0,gmax-gmin,0);
   _cub_add_cs(nct,cub,&pp,&i,p,r+1,g,b,red,green,blue,rmax,gmin,bmin,0,gmax-gmin,0,0,0,bmax-bmin);
   _cub_add_cs(nct,cub,&pp,&i,p,r,g+1,b,red,green,blue,rmin,gmax,bmin,rmax-rmin,0,0,0,0,bmax-bmin);
   _cub_add_cs(nct,cub,&pp,&i,p,r,g,b+1,red,green,blue,rmin,gmin,bmax,rmax-rmin,0,0,0,gmax-gmin,0);

#if 0
   fprintf(stderr," size=%d\n",i);

   do
   {
      int j;
      for (j=0; j<i; j++)
	 fprintf(stderr,"#%02x%02x%02x,",
		 nct->u.flat.entries[p[j]].color.r,
		 nct->u.flat.entries[p[j]].color.g,
		 nct->u.flat.entries[p[j]].color.b);
      fprintf(stderr,"\n");
   } 
   while (0);
#endif

   cub->n=i;
   cub->index=realloc(p,i*sizeof(struct nctlu_cubicle));

   if (!cub->index) 
      cub->index=p; /* out of memory, or wierd */
}

static void _img_nct_map_to_flat_cubicles(rgb_group *s,
					  rgb_group *d,
					  int n,
					  struct neo_colortable *nct)
{
   struct nctlu_cubicles *cubs;
   struct nctlu_cubicle *cub;
   int red,green,blue;
   int hred,hgreen,hblue;
   int redgreen;
   struct nct_flat_entry *fe=nct->u.flat.entries;
   int mindist;
   rgbl_group sf=nct->spacefactor;


   cubs=&(nct->lu.cubicles);
   if (!(cubs->cubicles))
   {
      int n=cubs->r*cubs->g*cubs->b;

CHRONO("init flat/cubicles");

      cub=cubs->cubicles=malloc(sizeof(struct nctlu_cubicle)*n);
      
      if (!cub) error("out of memory\n");

      while (n--) /* initiate all to empty */
      {
	 cub->n=0;
	 cub->index=NULL;
	 cub++;
      } /* yes, could be faster with a memset... */
   }

CHRONO("begin flat/cubicles");

   red=cubs->r;   hred=red/2;
   green=cubs->g; hgreen=green/2;
   blue=cubs->b;  hblue=blue/2;
   redgreen=red*green;

   while (n--)
   {
      int rgbr,rgbg,rgbb;
      int r,g,b;
      struct lookupcache *lc;
      int m;
      int *ci;

      rgbr=s->r;
      rgbg=s->g;
      rgbb=s->b;
      
      lc=nct->lookupcachehash+COLORLOOKUPCACHEHASHVALUE(rgbr,rgbg,rgbb);
      if (lc->index!=-1 &&
	  lc->src.r==rgbr &&
	  lc->src.g==rgbg &&
	  lc->src.b==rgbb)
      {
	 *(d++)=lc->dest;
	 s++;
	 continue;
      }

      lc->src=*s;
      
      r=((rgbr*red+hred)>>8);
      g=((rgbg*green+hgreen)>>8);
      b=((rgbb*blue+hblue)>>8);

      cub=cubs->cubicles+r+g*red+b*redgreen;
      
      if (!cub->index) /* need to build that cubicle */
	 _build_cubicle(nct,r,g,b,red,green,blue,cub);

      /* now, compare with the colors in that cubicle */

      m=cub->n;
      ci=cub->index;

      mindist=256*256*100; /* max dist is 256�*3 */
      
      while (m--)
      {
	 int dist=sf.r*SQ(fe[*ci].color.r-rgbr)+
	          sf.g*SQ(fe[*ci].color.g-rgbg)+
	          sf.b*SQ(fe[*ci].color.b-rgbb);
	 
	 if (dist<mindist)
	 {
	    lc->dest=*d=fe[*ci].color;
	    mindist=dist;
	    lc->index=*ci;
	 }
	 
	 ci++;
      }
      
      d++;
      s++;
   }

CHRONO("end flat/cubicles");
}

static void _img_nct_map_to_flat_tree(rgb_group *s,
				      rgb_group *d,
				      int n,
				      struct neo_colortable *nct)
{
   error("colortable->map(): map to flat/tree not implemented\n");   
}

void image_colortable_map(INT32 args)
{
   struct image *src;
   struct image *dest;
   struct object *o;

   if (args<1)
      error("too few arguments to colortable->map()\n");
   if (sp[-args].type!=T_OBJECT ||
       ! (src=(struct image*)get_storage(sp[-args].u.object,image_program)))
      error("illegal argument 1 to colortable->map(), expecting image object\n");

   if (!src->img) 
      error("colortable->map(): source image is empty\n");

   o=clone_object(image_program,0);
   dest=(struct image*)(o->storage);
   *dest=*src;

   dest->img=malloc(sizeof(rgb_group)*src->xsize*src->ysize +1);
   if (!dest->img)
   {
      free_object(o);
      error("colortable->map(): out of memory\n");
   }

   switch (THIS->type)
   {
      case NCT_CUBE:
	 _img_nct_map_to_cube(src->img,dest->img,
			      src->xsize*src->ysize,THIS);
	 pop_n_elems(args);
	 push_object(o);
	 return;
      case NCT_FLAT:
         switch (THIS->lookup_mode)
	 {
	    case NCT_FULL:
  	       _img_nct_map_to_flat_full(src->img,dest->img,
					 src->xsize*src->ysize,THIS);
	       break;
	    case NCT_CUBICLES:
  	       _img_nct_map_to_flat_cubicles(src->img,dest->img,
					     src->xsize*src->ysize,THIS);
	       break;
	    case NCT_TREE:
  	       _img_nct_map_to_flat_tree(src->img,dest->img,
					 src->xsize*src->ysize,THIS);
	       break;
	 }
	 pop_n_elems(args);
	 push_object(o);
	 return;
      default:
	 free_object(o);
	 error("colortable->map(): called colortable is not initiated\n");
   }
}


/*
**! method object spacefactors(int r,int g,int b)
**!	Colortable tuning option, this sets the color space
**!	distance factors. This is used when comparing distances
**!	in the colorspace and comparing grey levels.
**!
**!	Default factors are 2, 3 and 1; blue is much 
**!	darker than green. Compare with <ref>Image.image::grey</ref>().
**!
**! returns the called object
**!
**! note
**!	This has no sanity check. Some functions may bug
**!	if the factors are to high - color reduction functions 
**!	sums grey levels in the image, this could exceed maxint
**!	in the case of high factors. Negative values may 
**!	also cause strange effects. *grin*
**/

void image_colortable_spacefactors(INT32 args)
{
   if (args<3)
      error("To few arguments to colortable->spacefactors()\n");

   if (sp[0-args].type!=T_INT ||
       sp[1-args].type!=T_INT ||
       sp[2-args].type!=T_INT)
      error("Illegal argument(s) to colortable->spacefactors()\n");

   THIS->spacefactor.r=sp[0-args].u.integer;
   THIS->spacefactor.g=sp[1-args].u.integer;
   THIS->spacefactor.b=sp[2-args].u.integer;

   pop_n_elems(args);
   push_object(THISOBJ); THISOBJ->refs++;
}


/***************** global init etc *****************************/


void init_colortable_programs(void)
{
   start_new_program();
   add_storage(sizeof(struct neo_colortable));

   set_init_callback(init_colortable_struct);

   add_function("create",image_colortable_create,
		"function(void:void)|"
		"function(array(array(int)):void)|"
		"function(object,int,mixed ...:void)|"
		"function(int,int,int,void|int ...:void)",0);

   add_function("add",image_colortable_create,
		"function(void:void)|"
		"function(array(array(int)):void)|"
		"function(object,int,mixed ...:void)|"
		"function(int,int,int,void|int ...:void)",0);

   add_function("reduce",image_colortable_reduce,
		"function(int:object)",0);

   /* operators */
   add_function("`+",image_colortable_operator_plus,
		"function(object:object)",0);
   add_function("``+",image_colortable_operator_plus,
		"function(object:object)",0);

   /* cast to array */
   add_function("cast",image_colortable_cast,
		"function(string:array)",0);

   /* modes */
   add_function("tree",image_colortable_tree,
		"function(:object)",0);
   add_function("cubicles",image_colortable_cubicles,
		"function(:object)",0);
   add_function("full",image_colortable_full,
		"function(:object)",0);

   /* map image */
   add_function("map",image_colortable_map,
		"function(object:object)",0);
   add_function("`*",image_colortable_map,
		"function(object:object)",0);
   add_function("``*",image_colortable_map,
		"function(object:object)",0);

   /* tuning image */
   add_function("spacefactors",image_colortable_spacefactors,
		"function(int,int,int:object)",0);

   set_exit_callback(exit_colortable_struct);
  
   image_colortable_program=end_program();
   add_program_constant("colortable",image_colortable_program, 0);
}

void exit_colortable(void) 
{
  if(image_colortable_program)
  {
    free_program(image_colortable_program);
    image_colortable_program=0;
  }
}

