#ifndef IMAGE_INITER
#define IMAGE_CLASS(a,b,c,what) extern struct program *what;
#define IMAGE_SUBMODULE(a,b,c) 
#define IMAGE_SUBMODMAG(a,b,c) 
#define IMAGE_FUNCTION(a,name,c,d) void name(INT32 args);
#endif

IMAGE_CLASS("Image",      init_image_image,      exit_image_image, 
	    image_program )

IMAGE_CLASS("Colortable", init_image_colortable, exit_image_colortable, 
	    image_colortable_program )

IMAGE_CLASS("Layer",      init_image_layers,     exit_image_layers,
	    image_layer_program )

IMAGE_CLASS("Font",       init_image_font,       exit_image_font,
	    image_font_program )

IMAGE_SUBMODULE("Color", init_image_colors, exit_image_colors )

IMAGE_SUBMODULE("ANY",   init_image_any,  exit_image_any  ) 
IMAGE_SUBMODULE("AVS",   init_image_avs,  exit_image_avs  ) 
IMAGE_SUBMODULE("BMP",   init_image_bmp,  exit_image_bmp  ) 
IMAGE_SUBMODULE("GD",    init_image_gd,   exit_image_gd   )
IMAGE_SUBMODULE("GIF",   init_image_gif,  exit_image_gif  )
IMAGE_SUBMODULE("HRZ",   init_image_hrz,  exit_image_hrz  ) 
IMAGE_SUBMODULE("ILBM",  init_image_ilbm, exit_image_ilbm ) 
IMAGE_SUBMODULE("PCX",   init_image_pcx,  exit_image_pcx  ) 
IMAGE_SUBMODULE("PNM",   init_image_pnm,  exit_image_pnm  ) 
IMAGE_SUBMODULE("_PSD",  init_image_psd,  exit_image_psd  ) 
IMAGE_SUBMODULE("TGA",   init_image_tga,  exit_image_tga  ) 
IMAGE_SUBMODULE("X",     init_image_x,    exit_image_x    ) 
IMAGE_SUBMODULE("XBM",   init_image_xbm,  exit_image_xbm  ) 
IMAGE_SUBMODULE("_XCF",  init_image_xcf,  exit_image_xcf  ) 
IMAGE_SUBMODULE("XWD",   init_image_xwd,  exit_image_xwd  ) 
IMAGE_SUBMODULE("_XPM",  init_image__xpm, exit_image__xpm ) 

IMAGE_SUBMODMAG("PNG",   init_image_png,  exit_image_png  )

IMAGE_FUNCTION("lay",image_lay,
	       tOr(tFunc(tArr(tOr(tObj,tLayerMap)),tObj),
		   tFunc(tArr(tOr(tObj,tLayerMap))
			 tInt tInt tInt tInt,tObj)),0)
