//   $Id: Dims.pmod,v 1.1 2002/01/17 20:18:11 nilsson Exp $
//
//   Imagedimensionreadermodule for Pike.
//   Created by Johan Sch�n, <js@roxen.com>.
//
//   This software is based in part on the work of the Independent JPEG Group.

//! @appears Image.Dims

#define M_SOF0  0xC0		/* Start Of Frame N */
#define M_SOF1  0xC1		/* N indicates which compression process */
#define M_SOF2  0xC2		/* Only SOF0-SOF2 are now in common use */
#define M_SOF3  0xC3
#define M_SOF5  0xC5		/* NB: codes C4 and CC are NOT SOF markers */
#define M_SOF6  0xC6
#define M_SOF7  0xC7
#define M_SOF9  0xC9
#define M_SOF10 0xCA
#define M_SOF11 0xCB
#define M_SOF13 0xCD
#define M_SOF14 0xCE
#define M_SOF15 0xCF
#define M_SOI   0xD8		/* Start Of Image (beginning of datastream) */
#define M_EOI   0xD9		/* End Of Image (end of datastream) */
#define M_SOS   0xDA		/* Start Of Scan (begins compressed data) */
#define M_COM   0xFE		/* COMment */

int(0..255) read_1_byte(Stdio.File f)
{
  return f->read(1)[0];
}

int(0..65535) read_2_bytes(Stdio.File f)
{
  int c;
  sscanf( f->read(2), "%2c", c );
  return c;
}

int(0..65535) read_2_bytes_intel(Stdio.File f)
{
  int c;
  sscanf( f->read(2), "%-2c", c);
  return c;
}


/*
 * Read the initial marker, which should be SOI.
 * For a JFIF file, the first two bytes of the file should be literally
 * 0xFF M_SOI.  To be more general, we could use next_marker, but if the
 * input file weren't actually JPEG at all, next_marker might read the whole
 * file and then return a misleading error message...
 */

int first_marker(Stdio.File f)
{
  int c1, c2;
    
  c1 = read_1_byte(f);
  c2 = read_1_byte(f);
  if (c1!=0xFF||c2!=M_SOI) return 0;
  return c2;
}


/*
 * Find the next JPEG marker and return its marker code.
 * We expect at least one FF byte, possibly more if the compressor used FFs
 * to pad the file.
 * There could also be non-FF garbage between markers.  The treatment of such
 * garbage is unspecified; we choose to skip over it but emit a warning msg.
 * NB: this routine must not be used after seeing SOS marker, since it will
 * not deal correctly with FF/00 sequences in the compressed image data...
 */

int next_marker(Stdio.File f)
{
  int c;
  int discarded_bytes = 0;
    
  /* Find 0xFF byte; count and skip any non-FFs. */
  c = read_1_byte(f);
  while (c != 0xFF) {
    discarded_bytes++;
    c = read_1_byte(f);
  }
  /* Get marker code byte, swallowing any duplicate FF bytes.  Extra FFs
   * are legal as pad bytes, so don't count them in discarded_bytes.
   */
  do {
    c = read_1_byte(f);
  } while (c == 0xFF);
  return c;
}

/* Skip over an unknown or uninteresting variable-length marker */
int skip_variable(Stdio.File f)
{
  int length = read_2_bytes(f);
  if (length < 2) return 0;  /* Length includes itself, so must be at least 2 */
  length -= 2;
  f->seek(f->tell()+length);
  return 1;
}

  
array(int) get_JPEG(Stdio.File f)
{
  int marker;
  /* Expect SOI at start of file */
  if (first_marker(f) != M_SOI)
    return 0;
    
  /* Scan miscellaneous markers until we reach SOS. */
  for (;;)
  {
    marker = next_marker(f);
    switch (marker) {
    case M_SOF0:		/* Baseline */
    case M_SOF1:		/* Extended sequential, Huffman */
    case M_SOF2:		/* Progressive, Huffman */
    case M_SOF3:		/* Lossless, Huffman */
    case M_SOF5:		/* Differential sequential, Huffman */
    case M_SOF6:		/* Differential progressive, Huffman */
    case M_SOF7:		/* Differential lossless, Huffman */
    case M_SOF9:		/* Extended sequential, arithmetic */
    case M_SOF10:		/* Progressive, arithmetic */
    case M_SOF11:		/* Lossless, arithmetic */
    case M_SOF13:		/* Differential sequential, arithmetic */
    case M_SOF14:		/* Differential progressive, arithmetic */
    case M_SOF15:		/* Differential lossless, arithmetic */
      int length = read_2_bytes(f);	/* usual parameter length count */
      int data_precision = read_1_byte(f);
      int image_height = read_2_bytes(f);
      int image_width = read_2_bytes(f);
      return ({ image_width,image_height });
      break;
	
    case M_SOS:			/* stop before hitting compressed data */
      return 0;

    case M_EOI:			/* in case it's a tables-only JPEG stream */
      return 0;
	
    default:			/* Anything else just gets skipped */
      if(!skip_variable(f)) return 0;   /* we assume it has a parameter count... */
      break;
    }
  }
}

// GIF-header:
// typedef struct _GifHeader
// {
//   // Header
//   BYTE Signature[3];     /* Header Signature (always "GIF") */
//   BYTE Version[3];       /* GIF format version("87a" or "89a") */
//   // Logical Screen Descriptor
//   WORD ScreenWidth;      /* Width of Display Screen in Pixels */
//   WORD ScreenHeight;     /* Height of Display Screen in Pixels */
//   BYTE Packed;           /* Screen and Color Map Information */
//   BYTE BackgroundColor;  /* Background Color Index */
//   BYTE AspectRatio;      /* Pixel Aspect Ratio */
// } GIFHEAD;

array(int) get_GIF(Stdio.File f)
{
  int marker;
  int offs=f->tell();
  if(f->read(3)!="GIF") return 0;
  f->seek(offs+6);
  int image_width = read_2_bytes_intel(f);
  int image_height = read_2_bytes_intel(f);
  return ({ image_width, image_height });
}

array(int) get_PNG(Stdio.File f)
{
  int marker;
  int offs=f->tell();
  f->seek(offs+1);
  if(f->read(3)!="PNG") return 0;
  f->seek(offs+12);
  if(f->read(4)!="IHDR") return 0;
  read_2_bytes(f);
  int image_width = read_2_bytes(f);
  read_2_bytes_intel(f);
  int image_height = read_2_bytes(f);
  return ({ image_width, image_height });
}

//! Read dimensions from a JPEG, GIF or PNG file and return an array with
//! width and height, or if the file isn't a valid image, 0.
array(int) get(string|Stdio.File file)
{
  if(stringp(file))
    file=Stdio.File(file,"r");

  if(!file->tell)
    return 0; // Not a file object

  int offset=file->tell();
  array a;

  if(a=get_JPEG(file))
    return a;

  file->seek(offset);
  if(a=get_GIF(file))
    return a;

  file->seek(offset);
  if (a=get_PNG(file))
    return a;

  return 0;
}
