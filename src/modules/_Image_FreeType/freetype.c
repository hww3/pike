/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id: freetype.c,v 1.13 2002/11/29 20:56:46 jhs Exp $
*/

#include "config.h"
#include "global.h"
RCSID("$Id: freetype.c,v 1.13 2002/11/29 20:56:46 jhs Exp $");
#include "module.h"
#include "pike_error.h"

#ifdef HAVE_LIBFT2
#include <freetype/freetype.h>
#include "pike_macros.h"
#include "object.h"
#include "constants.h"
#include "interpret.h"
#include "svalue.h"
#include "threads.h"
#include "array.h"
#include "mapping.h"
#include "stralloc.h"
#include "builtin_functions.h"
#include "module_support.h"
#include "operators.h"
#include "../Image/image.h"
#endif /* HAVE_LIBFT2 */


#ifdef HAVE_LIBFT2

#define sp Pike_sp
#define fp Pike_fp

static FT_Library library;
static struct program *face_program;
#ifdef DYNAMIC_MODULE
static struct program *image_program;
#else
extern struct program *image_program;
#endif

struct face
{
  FT_Face face;
};

#define TFACE   ((struct face*)Pike_fp->current_storage)->face

/*! @module Image */

/*! @module FreeType
 *! Pike glue for the FreeType2 library, http://www.freetype.org/
 */

/*! @class Face
 *! A FreeType font face. We recommend using the more generic font handling
 *! interfaces in @[Image.Fonts] instead.
 */
static void image_ft_face_free( struct object *o )
{
  FT_Done_Face( TFACE );
}

/*! @decl Image.Image write_char(int char)
 *! @fixme
 */
static void image_ft_face_write_char( INT32 args )
{
  FT_GlyphSlot  slot = TFACE->glyph; /* optimize. */
  int c, x, y;
  struct image *i;
  struct object *o;
  rgb_group *d;

  if( sp[-args].type != T_INT )
    Pike_error("Bad argument 1 to write_char\n" );

  c = sp[-args].u.integer;

  if( FT_Load_Char( TFACE, c, FT_LOAD_RENDER ) )
    Pike_error("The character %d is not available\n", c );

  push_int( slot->bitmap.width );
  push_int( slot->bitmap.rows );
  o = clone_object( image_program, 2 );
  i = ((struct image *)o->storage);
  d = i->img;
  if( slot->bitmap.pixel_mode == ft_pixel_mode_grays )
  {
    int p = slot->bitmap.pitch;
    int g = slot->bitmap.num_grays;
    char *s = slot->bitmap.buffer;
    if( s )
      for( y = 0; y<i->ysize; y++ )
        for( x = 0; x<i->xsize; x++ )
        {
          int pv = (s[ x + y*p ] * g) >> 8;
          d->r = pv;
          d->g = pv;
          d->b = pv;
          d++;
        }
  } else
    Pike_error("Unhandled bitmap format received from renderer\n");

  push_text( "img" ); push_object( o );
  push_text( "x" )  ; push_int( slot->bitmap_left );
  push_text( "y" )  ; push_int( slot->bitmap_top );
  push_text( "advance" )  ; push_int( (slot->advance.x+62) >> 6 );
  push_text( "descender" ); push_int( TFACE->size->metrics.descender>>6 );
  push_text( "ascender" ); push_int( TFACE->size->metrics.ascender>>6 );
  push_text( "height" ); push_int( TFACE->size->metrics.height>>6 );
  
  f_aggregate_mapping( 14 );
}

/*! @decl int get_kerning(int l, int r)
 *! @fixme
 */
static void image_ft_face_get_kerning( INT32 args )
{
  INT_TYPE l, r;
  FT_Vector kern;
  get_all_args( "get_kerning", args, "%d%d", &l, &r );
  l = FT_Get_Char_Index( TFACE, l );
  r = FT_Get_Char_Index( TFACE, r );
  if( FT_Get_Kerning( TFACE, l, r, ft_kerning_default, &kern ) )
    kern.x = 0;
  pop_n_elems( args );
  push_int( kern.x );
}

/*! @decl void attach_file(string file)
 *! @fixme
 */
static void image_ft_face_attach_file( INT32 args )
{
  char *path;
  get_all_args( "attach_file", args, "%s", &path );
  if( FT_Attach_File( TFACE, path ) )
    Pike_error("Failed to attach file\n");
  pop_n_elems( args );
  push_int( 0 );
}

/*! @decl Face set_size(int width, int height)
 */
static void image_ft_face_set_size( INT32 args )
{
  int w, h;
  if( (args != 2) || (sp[-args].type != sp[1-args].type)
      || (sp[-args].type != T_INT) )
    Pike_error("Illegal arguments to set_size\n");
  w = sp[-2].u.integer;
  h = sp[-1].u.integer;

  if( FT_Set_Pixel_Sizes( TFACE, w, h ) )
    Pike_error("Failed to set size\n");
  pop_n_elems( 2 );
  ref_push_object( fp->current_object );
}

/*! @decl mapping info()
 *! @returns
 *!   @mapping
 *!     @member string "family"
 *!       The font family, or the string "unknown"
 *!     @member string "style_name"
 *!       The name of the font style, or "unknown"
 *!     @member int "face_flags"
 *!     @member int "style_flags"
 *!       The sum of all face/style flags respectively.
 *!   @endmapping
 */
static void image_ft_face_info( INT32 args )
{
  push_text( "family" );
  if( TFACE->family_name )
    push_text( TFACE->family_name );
  else
    push_text( "unknown" );
  push_text( "style" );
  if( TFACE->style_name )
    push_text( TFACE->style_name );
  else
    push_text( "unknown" );
  push_text( "face_flags" );  push_int( TFACE->face_flags );
  push_text( "style_flags" );  push_int( TFACE->style_flags );
  f_aggregate_mapping( 8 );
}

/*! @decl void create(string font)
 *! @param font
 *!   The path of the font file to use
 */
static void image_ft_face_create( INT32 args )
{
  int er;
  if( !args || sp[-args].type != T_STRING )
    Pike_error("Illegal argument 1 to FreeType.Face. Expected string.\n");
  er = FT_New_Face( library, sp[-args].u.string->str, 0, &TFACE );
  if( er == FT_Err_Unknown_File_Format )
    Pike_error( "Failed to parse the font file %s\n", sp[-args].u.string->str );
  else if( er )
    Pike_error( "Failed to open the font file %s\n", sp[-args].u.string->str );
  pop_n_elems( args );
  push_int( 0 );
}

/*! @endclass */

PIKE_MODULE_EXIT
{
  if( face_program )
    free_program( face_program );
}

/*! @decl constant FACE_FLAG_SCALABLE
 *! @decl constant FACE_FLAG_FIXED_WIDTH
 *! @decl constant FACE_FLAG_SFNT
 *! @decl constant FACE_FLAG_HORIZONTAL
 *! @decl constant FACE_FLAG_VERTICAL
 *! @decl constant FACE_FLAG_KERNING
 *! @decl constant FACE_FLAG_FAST_GLYPHS
 *! @decl constant FACE_FLAG_MULTIPLE_MASTERS
 *! @decl constant FACE_FLAG_GLYPH_NAMES
 *! @fixme
 */

/*! @decl constant STYLE_FLAG_ITALIC
 *! @decl constant STYLE_FLAG_BOLD
 *! @fixme
 */

/*! @endmodule */

/*! @endmodule */

PIKE_MODULE_INIT
{
  if( !FT_Init_FreeType( &library ) )
  {
#ifdef DYNAMIC_MODULE
    push_string(make_shared_string("Image"));
    push_int(0);
    SAFE_APPLY_MASTER("resolv",2);
    if (sp[-1].type==T_OBJECT)
    {
      push_string(make_shared_string("Image"));
      f_index(2);
      image_program=program_from_svalue(sp-1);
    }
    pop_n_elems(1);
#endif /* DYNAMIC_MODULE */

    start_new_program( );
    ADD_STORAGE( struct face );

    ADD_FUNCTION("create",image_ft_face_create, tFunc(tStr,tVoid), 0 );
    ADD_FUNCTION("set_size",image_ft_face_set_size,tFunc(tInt tInt,tObj),0);
    ADD_FUNCTION("attach_file",image_ft_face_attach_file,tFunc(tString,tVoid),0);
    ADD_FUNCTION("info",image_ft_face_info,tFunc(tVoid,tMapping),0);
    ADD_FUNCTION("write_char",image_ft_face_write_char,tFunc(tInt,tObj),0);
    ADD_FUNCTION("get_kerning",image_ft_face_get_kerning,
                 tFunc(tInt tInt,tInt),0);
    
    set_exit_callback( image_ft_face_free );

    face_program = end_program();
    add_program_constant("Face", face_program, 0 );
    add_integer_constant( "FACE_FLAG_SCALABLE", FT_FACE_FLAG_SCALABLE, 0 );
    add_integer_constant( "FACE_FLAG_FIXED_WIDTH", FT_FACE_FLAG_FIXED_WIDTH ,0); 
    add_integer_constant( "FACE_FLAG_SFNT", FT_FACE_FLAG_SFNT, 0 );
    add_integer_constant( "FACE_FLAG_HORIZONTAL", FT_FACE_FLAG_HORIZONTAL, 0 );
    add_integer_constant( "FACE_FLAG_VERTICAL", FT_FACE_FLAG_VERTICAL, 0 );
    add_integer_constant( "FACE_FLAG_KERNING", FT_FACE_FLAG_KERNING, 0 );
    add_integer_constant( "FACE_FLAG_FAST_GLYPHS", FT_FACE_FLAG_FAST_GLYPHS, 0 );
    add_integer_constant( "FACE_FLAG_MULTIPLE_MASTERS", FT_FACE_FLAG_MULTIPLE_MASTERS, 0 );
    add_integer_constant( "FACE_FLAG_GLYPH_NAMES", FT_FACE_FLAG_GLYPH_NAMES, 0 );


    add_integer_constant( "STYLE_FLAG_ITALIC", FT_STYLE_FLAG_ITALIC, 0 );
    add_integer_constant( "STYLE_FLAG_BOLD", FT_STYLE_FLAG_BOLD, 0 );

  }
}
#else
PIKE_MODULE_EXIT{}
PIKE_MODULE_INIT{}
#endif
