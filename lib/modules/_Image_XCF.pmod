inherit Image._XCF;

#define SIGNED(X) if(X>=(1<<31)) X=-((1<<32)-X)

class PathPoint
{
  int type;
  float x;
  float y;
}

class Path
{
  string name;
  int ptype;
  int tattoo;
  int closed;
  int state;
  int locked;
  array (PathPoint) points = ({});
}

class Guide
{
  int pos;
  int vertical;
  void create(string data)
  {
    sscanf(data, "%4c%c", pos,vertical);vertical--;
    SIGNED(pos);
  }
}

class Parasite
{
  string name;
  int flags;
  string data;

  void create( string _n, int _f, string _d )
  {
    name = _n;
    data = _d;
    flags = _f;
  }
}

array(Parasite) decode_parasites( string data )
{
  array res = ({});
  while(strlen(data))
  {
    int slen, flags;
    string value, name;
    sscanf(data, "%4c", slen);
    name = data[..slen-2];
    data = data[slen..];
    sscanf(data, "%4c%4c", flags, slen);
    res += ({ Parasite( name,flags,data[8..slen+8-1] ) });
    data = data[slen+8..];
  }
  return res;
}

#define FLAG(X,Y) case PROP_##X: sscanf(p->data, "%4c", flags->Y); break;
#define INT(X,Y) case PROP_##X: sscanf(p->data, "%4c", Y); break;
#define SINT(X,Y) case PROP_##X: sscanf(p->data, "%4c", Y); SIGNED(Y); break;

class Hierarchy
{
  Image.image img;
  Image.image alpha;
  int width;
  int height;
  int bpp;

  Hierarchy set_image( int x, int y, int bp, array tiles, int compression,
                       Image.colortable cmap)
  {
    width = x;
    height = y;
    bpp = bp;
    img = Image.image( x, y, 0,0,0 );
    if(!(bp & 1 ))
      alpha = Image.image( x, y, 255,255,255 );
    switch(compression)
    {
     case COMPRESS_NONE:
     case COMPRESS_RLE:
       _decode_tiles( img,alpha,tiles,compression,bpp,cmap );
       break;
     default:
       error("Image tile compression type not supported\n");
    }
    return this_object();
  }

  Hierarchy qsi(object _i, object _a, int _w, int _h,int _b)
  {
    img = _i;
    alpha = _a;
    width = _w;
    height = _h;
    bpp = _b;
    return this_object();
  }

  Hierarchy copy()
  {
    return Hierarchy()->qsi( img,alpha,width,height,bpp );
  }

  Hierarchy get_opaqued( int opaque_value )
  {
    Hierarchy res = copy();
    if(opaque_value != 255)
    {
      if(res->alpha)
        res->alpha *= opaque_value/255.0;
      else
        res->alpha = Image.image(width,height,
                                 opaque_value,opaque_value,opaque_value);
    }
    return res;
  }
}

int iid;
Hierarchy decode_image_data( mapping what, object i )
{
  Hierarchy h =
    Hierarchy( )->set_image(what->width, what->height, what->bpp,
                            what->tiles, i->compression, i->colormap );
  return h;
}

class Channel
{
  string name;
  int width;
  int height;
  int opacity;
  int r, g, b;
  int tattoo;
  Hierarchy image_data;
  object parent;
  mapping flags = ([]);
  array (Parasite) parasites;

  void decode_properties( array properties )
  {
    foreach(properties, mapping p)
    {
      switch(p->type)
      {
       case PROP_ACTIVE_CHANNEL:
         parent->active_channel = this_object();
         break;
       case PROP_SELECTION:
         parent->selection = this_object();
         break;
         INT(OPACITY,opacity);
         FLAG(VISIBLE,visible);
         FLAG(LINKED,linked);
         FLAG(PRESERVE_TRANSPARENCY,preserve_transparency);
         FLAG(EDIT_MASK,edit_mask);
         FLAG(SHOW_MASKED,show_masked);
         INT(TATTOO,tattoo);
       case PROP_COLOR:
         sscanf( p->data, "%c%c%c", r, g, b);
         break;

       case PROP_PARASITES:
         parasites = decode_parasites( p->data );
         break;
      }
    }
  }


  void create( mapping d, object p )
  {

    parent = p;
    name = d->name;
    width = d->width;
    height = d->height;
    image_data = decode_image_data( d->image_data, parent );
    if(d->properties) decode_properties( d->properties );
  }
}


class LayerMask
{
  inherit Channel;
}

class Layer
{
  string name;
  int opacity;
  int type;
  int mode;
  int tattoo;
  mapping flags = ([]);
  int width, height;
  int xoffset, yoffset;
  array (Parasite) parasites;
  LayerMask mask;
  Hierarchy image;

  object parent;

  void decode_properties( array properties )
  {
    foreach( properties, mapping p)
    {
      switch(p->type)
      {
       case PROP_ACTIVE_LAYER:
         parent->active_layer = this_object();
         break;
       case PROP_SELECTION:
         parent->selection = this_object();
         break;
       case PROP_OFFSETS:
         sscanf(p->data, "%4c%4c", xoffset, yoffset);
         SIGNED(xoffset);
         SIGNED(yoffset);
         break;
       INT(OPACITY,opacity);
       FLAG(VISIBLE,visible);
       FLAG(LINKED,linked);
       FLAG(PRESERVE_TRANSPARENCY,preserve_transparency);
       FLAG(APPLY_MASK,apply_mask);
       FLAG(EDIT_MASK,edit_mask);
       FLAG(SHOW_MASK,show_mask);
       INT(MODE,mode);
       INT(TATTOO,tattoo);
       case PROP_PARASITES:
         parasites = decode_parasites( p->data );
         break;
      }
    }
  }

  void create( mapping data, object pa )
  {
    parent = pa;
    name = data->name;
    type = data->type;
    width = data->width;
    height = data->height;
    decode_properties( data->properties );
    image = decode_image_data( data->image_data, pa );
    if(data->mask)
      mask = LayerMask( data->mask, pa );
  }
}

class GimpImage
{
  int width;
  int height;
  int compression;
  int type;
  int tattoo_state;
  float xres = 72.0;
  float yres = 72.0;
  int res_unit;
  Image.colortable colormap;
  Image.colortable meta_colormap;
  array(Layer) layers = ({});            // bottom-to-top
  array(Channel) channels = ({});       // unspecified order, really
  array(Guide) guides = ({});
  array(Parasite) parasites = ({});
  array(Path) paths = ({});

  Layer active_layer;
  Channel active_channel;
  Channel selection;


  static string read_point_bz1( string data, Path path )
  {
    object p = PathPoint( );
    int x, y;
    sscanf(data, "%4c%4c%4c%s", p->type, x, y);
    SIGNED(x);
    SIGNED(y);
    p->x = (float)x;
    p->y = (float)y;
    return data;
  }

  static string read_point_bz2( string data, Path path )
  {
    object p = PathPoint( );
    sscanf(data, "%4c%4F%4F%s", p->type, p->x, p->y);
    return data;
  }

  static string decode_one_path( string data, Path path )
  {
    int nlen, version, num_points;
    sscanf(data, "%4c", nlen );
    path->name = data[..nlen-2];
    data = data[nlen..];
    sscanf(data, "%4c%4c%4c%4c%4c",
           path->locked, path->state, path->closed, num_points, version);
    switch(version)
    {
     case 1:
       while(num_points--)
         data = read_point_bz1( data, path );
       break;
     case 2:
       sscanf(data, "%4c%s", path->ptype, data );
       while(num_points--)
         data = read_point_bz2( data, path );
       break;
     case 3:
       sscanf(data, "%4%4cc%s", path->ptype, path->tattoo, data );
       while(num_points--)
         data = read_point_bz2( data, path );
       break;
     default:
       data ="";
    }
    return data;
  }

  array(Path) decode_paths( string data )
  {
    int last_selected_row;
    int num_paths;
    array res = ({});
    sscanf( data, "%4c%4c%s", last_selected_row, num_paths, data );
    while(num_paths--)
    {
      Path path = Path();
      data = decode_one_path( data, path );
      res += ({ path });
    }
    return res;
  }



  void decode_properties(array props)
  {
    foreach( props, mapping p)
    {
      switch(p->type)
      {
       case PROP_COLORMAP:
         if(type == INDEXED)
           meta_colormap = colormap = Image.colortable( p->data );
         else
           meta_colormap = Image.colortable( p->data );
         break;
       case PROP_COMPRESSION:
         compression = p->data[0];
         break;
       case PROP_GUIDES:
         guides = Array.map(p->data/5,Guide);
         break;
       case PROP_RESOLUTION:
         sscanf( p->data, "%4f%4f", xres,yres);
         if (xres < 1e-5 || xres> 1e+5 || yres<1e-5 || yres>1e+5)
           xres = yres = 72.0;
         break;
       case PROP_TATTOO:
         sscanf(p->data, "%4c", tattoo_state );
         break;
       case PROP_PARASITES:
         parasites = decode_parasites( p->data );
         break;
       case PROP_UNIT:
         sscanf(p->data, "%4c", res_unit );
         break;
       case PROP_PATHS:
         paths = decode_paths( p->data );
         break;
       case PROP_USER_UNIT:
         /* NYI */
         break;
      }
    }
  }

  void create( mapping data )
  {
    type = data->type;
    decode_properties( data->properties );
    width = data->width;
    height = data->height;
    foreach(data->layers, mapping l )
      layers += ({ Layer( l, this_object() ) });
    foreach(data->channels, mapping c )
      channels += ({ Channel( c, this_object() ) });
  }
}



GimpImage __decode( string|mapping what )
{
  if(stringp(what)) what = ___decode( what );
  return GimpImage(what);
}


string translate_mode( int mode )
{
  switch( mode )
  {
   case NORMAL_MODE:      return "normal";
   case MULTIPLY_MODE:    return "multiply";
   case ADDITION_MODE:    return "add";
   case DIFFERENCE_MODE:  return "difference";
   case SUBTRACT_MODE:    return "subtract";
   case DIVIDE_MODE:      return "divide";
   case DISSOLVE_MODE:    return "dissolve";
   case DARKEN_ONLY_MODE: return "min";
   case LIGHTEN_ONLY_MODE:return "max";
   case HUE_MODE:         return "hue";
   case SATURATION_MODE:  return "saturation";
   case COLOR_MODE:       return "color";
   case VALUE_MODE:       return "value";
   case SCREEN_MODE:      return "screen";
   case OVERLAY_MODE:     return "overlay";

   default:
     werror("WARNING: XCF: Unsupported mode: "+mode+"\n");
     return "normal";
  }
}

array decode_layers( string|object|mapping what, mapping|void opts )
{
  if(!opts) opts = ([]);

  if(!objectp( what ) )
    what = __decode( what );

  mapping lopts = ([ "tiled":1, ]);
  array layers = ({});
  if( opts->background )
  {
    lopts->image = Image.Image( 32, 32, opts->background );
    lopts->alpha = Image.Image( 32, 32, Image.Color.white );
    lopts->alpha_value = 1.0;
    layers = ({ Image.Layer( lopts ) });
  }

  foreach(what->layers, object l)
  {
    if(l->flags->visible || opts->draw_all_layers)
    {
      Hierarchy   h   = l->image;
      Image.Layer lay = Image.Layer( h->img,
                                     h->alpha,
                                     translate_mode( l->mode ) );


      /* Not really layer related */
      lay->set_misc_value( "image_xres", l->parent->xres );
      lay->set_misc_value( "image_yres", l->parent->yres );
      lay->set_misc_value( "image_colormap", l->parent->colormap );
      lay->set_misc_value( "image_guides", l->parent->guides );
      lay->set_misc_value( "image_parasites", l->parent->parasites );

      /* But these are. :) */
      lay->set_misc_value( "name", l->name );
      lay->set_misc_value( "tattoo", l->tattoo );
      lay->set_misc_value( "parasites", l->parasites );
      lay->set_misc_value( "visible", l->flags->visible );
      if( l == l->parent->active_layer )
        lay->set_misc_value( "active", 1 );

      h->img = 0; h->alpha = 0;

      lay->set_alpha_value( l->opacity / 255.0 );
      lay->set_offset( l->xoffset, l->yoffset );

      if(l->mask && l->flags->apply_mask)
      {
        l->mask = 0;
        object a = l->alpha();
        if(a)
          a *= l->mask->image;
        else
          a = l->mask->image;
        if( a->xsize() != l->image->img->xsize() ||
            a->ysize() != l->image->img->ysize() )
          a = a->copy( 0,0, l->image->image->xsize(),
                       l->image->image->ysize(), 255,255,255 );
        lay->set_alpha( a );
      }
      layers += ({ lay });
    }
  }
  return layers;
}

mapping _decode( string|mapping what, mapping|void opts )
{
  if(!opts) opts = ([]);

  GimpImage data = __decode( what );
  what = 0;

  Image.Layer res = Image.lay(decode_layers( data, opts ),
                              0,0,data->width,data->height );
  Image.Image img = res->image();
  Image.Image alpha = res->alpha();
  res = 0;

  if(opts->draw_guides)
    foreach( data->guides, Guide g )
      if(g->vertical)
      {
        img->line( g->pos, 0, g->pos, img->ysize(), 0,155,0 );
        if( alpha )
          alpha->line(  g->pos, 0, g->pos, img->ysize(), 255,255,255 );
      }
      else
      {
        img->line( 0,g->pos,  img->xsize(), g->pos, 0,155,0 );
        if( alpha )
          alpha->line(  0,g->pos, img->xsize(),g->pos, 255,255,255 );
      }

  if(opts->draw_selection)
    if(data->selection)
      img->paste_alpha_color( data->selection->image_data->img*0.25,
                              data->selection->r, data->selection->g,
                              data->selection->b );

  if(opts->mark_layers)
  {
    foreach(data->layers, Layer l)
    {
      if(l->flags->visible || opts->draw_all_layers)
      {
        int x1 = l->xoffset;
        int x2 = l->xoffset+l->width;
        int y1 = l->yoffset;
        int y2 = l->yoffset+l->height;
        img->setcolor(0,0,255);
        img->line( x1,y1,x2,y1 );
        img->line( x2,y1,x2,y2 );
        img->line( x2,y2,x1,y2 );
        img->line( x1,y2,x1,y1 );
        if(alpha)
        {
          alpha->setcolor(0,0,255);
          alpha->line( x1,y1,x2,y1 );
          alpha->line( x2,y1,x2,y2 );
          alpha->line( x2,y2,x1,y2 );
          alpha->line( x1,y2,x1,y1 );
        }
      }
    }
  }

  if(opts->mark_layer_names)
  {
    foreach(data->layers, Layer l)
    {
      if(l->flags->visible || opts->draw_all_layers)
      {
        int x, y;
        int x1 = l->xoffset;
        int y1 = l->yoffset+3;
        object a = opts->mark_layer_names->write( l->name );
        for( x=-1; x<2; x++ )
          for( y=-1; y<2; y++ )
          {
            img->paste_alpha_color( a, 0,0,0, x1+x, y1+y );
            if(alpha)
              alpha->paste_alpha_color(a,255,255,255, x1+x,y1+y );
          }
        img->paste_alpha_color( a, 255,255,255, x1, y1 );
      }
    }
  }

  if(opts->mark_active_layer)
  {
    if(data->active_layer)
    {
      int x1 = data->active_layer->xoffset;
      int x2 = data->active_layer->xoffset+data->active_layer->width;
      int y1 = data->active_layer->yoffset;
      int y2 = data->active_layer->yoffset+data->active_layer->height;
      img->setcolor(255,0,0);
      img->line( x1,y1,x2,y1 );
      img->line( x2,y1,x2,y2 );
      img->line( x2,y2,x1,y2 );
      img->line( x1,y2,x1,y1 );
      if(alpha)
      {
        alpha->setcolor(0,0,255);
        alpha->line( x1,y1,x2,y1 );
        alpha->line( x2,y1,x2,y2 );
        alpha->line( x2,y2,x1,y2 );
        alpha->line( x1,y2,x1,y1 );
      }
    }
  }

  Array.map( data->layers, lambda(object o) { destruct(o); } );
  destruct( data );
  return
  ([
    "image":img,
    "alpha":alpha,
  ]);
}


object decode( string what,mapping|void opts )
{
  return _decode( what,opts )->image;
}
