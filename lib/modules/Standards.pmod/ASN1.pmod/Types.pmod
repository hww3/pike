/*
 * $Id: Types.pmod,v 1.21 2003/01/20 17:44:01 nilsson Exp $
 *
 * Encodes various asn.1 objects according to the Distinguished
 * Encoding Rules (DER) */

//! Encodes various asn.1 objects according to the Distinguished
//! Encoding Rules (DER)

#pike __REAL_VERSION__

#if constant(Gmp.mpz)

#if 0
#define WERROR werror
#else
#define WERROR(x)
#endif



#define MAKE_COMBINED_TAG(cls, tag) (((tag) << 2) | (cls))

//! Combines tag and class as a single integer, in a somewhat arbitrary
//! way. This works also for tags beyond 31 (although not for tags
//! beyond 2^30. 
//! 
//! @param cls
//!   ASN1 type class
//! @param tag
//!   ASN1 type tag
//! @returns
//!   combined tag
//! @seealso 
//!  @[Standards.ASN1.Types.extract_tag]
//!  @[Standards.ASN1.Types.extract_cls]
int make_combined_tag(int cls, int tag)
{ return MAKE_COMBINED_TAG(cls, tag); }

//! extract ASN1 type tag from a combined tag
//! @seealso 
//!  @[Standards.ASN1.Types.make_combined_tag]
int extract_tag(int i) { return i >> 2; }

//! extract ASN1 type class from a combined tag
//! @seealso 
//!  @[Standards.ASN1.Types.make_combined_tag]
int extract_cls(int i) { return i & 3; }

//! generic base class for ASN1 data types
class asn1_object
{
  constant cls = 0;
  constant tag = 0;
  constant constructed = 0;

  constant type_name = 0;
  string der_encode();

//! get the class of this object
//!
//! @returns
//!   the class of this object  
  int get_cls() { return cls; }

//! get the tag for this object
//!
//! @returns
//!   the tag for this object
  int get_tag() { return tag; }

//!  get the combined tag (tag + class) for this object
//!  
//!  @returns
//!    the combined tag header
  int get_combined_tag()
    { return make_combined_tag(get_tag(), get_cls()); }
  
  string der;
  
  // Should be overridden by subclasses 
  object decode_primitive(string contents)
    { error("decode_primitive not implemented\n"); }

  object begin_decode_constructed(string raw) 
    { error("begin_decode_constructed not implemented\n"); }
  object decode_constructed_element(int i, object e) 
    { error("decode_constructed_element not implemented\n"); }
  object end_decode_constructed(int length) 
    { error("end_decode_constructed not implemented\n"); }

  mapping element_types(int i, mapping types) { return types; }
  object init(mixed ... args) { return this_object(); }

  string to_base_128(int n)
    {
      if (!n)
	return "\0";
      /* Convert tag number to base 128 */
      array(int) digits = ({ });

      /* Array is built in reverse order, least significant digit first */
      while(n)
      {
	digits += ({ (n & 0x7f) | 0x80 });
	n >>= 7;
      }
      digits[0] &= 0x7f;

      return sprintf("%@c", reverse(digits));
    }
  
  string encode_tag()
    {
      int tag = get_tag();
      int cls = get_cls();
      if (tag < 31)
	return sprintf("%c", (cls << 6) | (constructed << 5) | tag);

      return sprintf("%c%s", (cls << 6) | (constructed << 5) | 0x1f,
		     to_base_128(tag) );
    }

  string encode_length(int|object len)
    {
      if (len < 0x80)
	return sprintf("%c", len);
      string s = Gmp.mpz(len)->digits(256);
      if (sizeof(s) >= 0x80)
	error( "asn1.encode.asn1_object->encode_length: Max length exceeded.\n" );
      return sprintf("%c%s", sizeof(s) | 0x80, s);
    }
  
  string build_der(string contents)
    {
      string data = encode_tag() + encode_length(sizeof(contents)) + contents;
      // WERROR(sprintf("build_der: '%s'\n", Crypto.string_to_hex(data)));
      WERROR(sprintf("build_der: %O\n", data));
      return data;
    }

  string record_der(string s)
    {
      return (der = s);
    }

  string record_der_contents(string s)
    {
      record_der(build_der(s));
    }
  
//! get the DER encoded version of this object
//!
//! @returns
//!   DER encoded representation of this object
  string get_der()
    {
      return der || (record_der(der_encode()));
    }

//! create an ASN1 object
  void create(mixed ...args)
    {
      WERROR(sprintf("asn1_object[%s]->create\n", type_name));
      if (sizeof(args))
	init(@args);
    }
}

//! Compound object primitive
class asn1_compound
{
  inherit asn1_object;

  constant constructed = 1;

//! contents of compound object, elements are from @[Standards.ASN1.Types]
  array elements = ({ });

  object init(array args)
    {
      WERROR(sprintf("asn1_compound[%s]->init(%O)\n", type_name, args));
      elements = args;
      foreach(elements, mixed o)
	if (!o || !objectp(o))
	  error( "asn1_compound: Non-object argument!\n" );
      WERROR(sprintf("asn1_compound: %O\n", elements));
      return this_object();
    }

  object begin_decode_constructed(string raw)
    {
      WERROR(sprintf("asn1_compound[%s]->begin_decode_constructed\n",
		     type_name));
      record_der_contents(raw);
      return this_object();
    }

  object decode_constructed_element(int i, object e)
    {
      WERROR(sprintf("asn1_compound[%s]->decode_constructed_element(%O)\n",
		     type_name, e));
      if (i != sizeof(elements))
	error("decode_constructed_element: Unexpected index!\n");
      elements += ({ e });
      return this_object();
    }
  
  object end_decode_constructed(int length)
    {
      if (length != sizeof(elements))
	error("end_decode_constructed: Invalid length!\n");

      return this_object();
    }

  string debug_string()
    {
      WERROR(sprintf("asn1_compound[%s]->debug_string(), elements = %O\n",
		     type_name, elements));
      return "(" + type_name + " " + elements->debug_string() * ",\n" + ")";
    }
}

//! string object primitive
class asn1_string
{
  inherit asn1_object;

//! value of object
  string value;

  object init(string s)
    {
      value = s;
      return this_object();
    }

  string der_encode()
    {
      return build_der(value);
    }

  object decode_primitive(string contents)
    {
      record_der_contents(contents);
      value = contents;
      return this_object();
    }
  
  string debug_string()
    {
      WERROR(sprintf("asn1_string[%s]->debug_string(), value = %O\n",
		     type_name, value));
      return sprintf("%s %O", type_name, value);
    }
}

// FIXME: What is the DER-encoding of TRUE???
// According to Jan Petrous, the LDAP spec says that 0xff is right.

//! boolean object
class asn1_boolean
{
  inherit asn1_object;
  constant tag = 1;
  constant type_name = "BOOLEAN";

//! value of object
  int value;
  
  object init(int x) { value = x; return this_object(); }

  string der_encode() { return build_der(value ? "\377" : "\0"); }
  object decode_primitive(string contents)
    {
      if (sizeof(contents) != 1)
      {
	WERROR("asn1_boolean->decode_primitive: Bad length.\n");
	return 0;
      }
      record_der_contents(contents);
      value = contents[0];
      return this_object();
    }
  string debug_string()
    {
      return value ? "TRUE" : "FALSE";
    }
}
   
//! Integer object 
//! All integers are represented as bignums, for simplicity
class asn1_integer
{
  inherit asn1_object;
  constant tag = 2;
  constant type_name = "INTEGER";
  
//! value of object
  object value;

  object init(int|object n)
    {
      value = Gmp.mpz(n);
      WERROR(sprintf("i = %s\n", value->digits()));
      return this_object();
    }

  string der_encode()
    {
      string s;
      
      if (value < 0)
      {
	object n = value + pow(256, (- value)->size(256));
	s = n->digits(256);
	if (!(s[0] & 0x80))
	  s = "\377" + s;
      } else {
	s = value->digits(256);
	if (s[0] & 0x80)
	  s = "\0" + s;
      }
      return build_der(s);
    }
  
  object decode_primitive(string contents)
    {
      record_der_contents(contents);
      value = Gmp.mpz(contents, 256);
      if (contents[0] & 0x80)  /* Negative */
	value -= pow(256, sizeof(contents));
      return this_object();
    }
  
  string debug_string()
    {
      return sprintf("INTEGER (%d) %s", value->size(), value->digits());
    }

}

//! Enumerated object
class asn1_enumerated
{
  inherit asn1_integer;
  constant tag = 10;
  constant type_name ="ENUMERATED";
}

//! Bit string object
class asn1_bit_string
{
  inherit asn1_object;
  constant tag = 3;
  constant type_name = "BIT STRING";

//! value of object
  string value;

  int unused = 0;
  
  object init(string s) { value = s; return this_object(); }

  string der_encode()
    {
      return build_der(sprintf("%c%s", unused, value));
    }

//! @fixme
//!   document me!
  int set_length(int len)
    {
      if (len)
      {
	value = value[..(len + 7)/8];
	unused = (- len) % 8;
	value = sprintf("%s%c", value[..sizeof(value)-2], value[-1]
		    & ({ 0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80 })[unused]);
      } else {
	unused = 0;
	value = "";
      }
    }
  
  object decode_primitive(string contents)
    {
      record_der_contents(contents);
      if (!sizeof(contents))
	return 0;
      unused = contents[0];

      if (unused >= 8)
	return 0;
      value = contents[1..];
      return this_object();
    }
  
  string debug_string()
    {
      return sprintf("BIT STRING (%d) %s",
		     sizeof(value) * 8 - unused,
		     (Gmp.mpz(value, 256) >> unused)->digits(2));
    }
}

//! Octet string object
class asn1_octet_string
{
  inherit asn1_string;
  constant tag = 4;
  constant type_name = "OCTET STRING";
}

//! Null object
class asn1_null
{
  inherit asn1_object;
  constant tag = 5;
  constant type_name = "NULL";
  
  string der_encode() { return build_der(""); }

  object decode_primitive(string contents)
    {
      record_der_contents(contents);
      return !sizeof(contents) && this_object();
    }

  string debug_string() { return "NULL"; }
}

//! Object identifier object
class asn1_identifier
{
  inherit asn1_object;
  constant tag = 6;
  constant type_name = "OBJECT IDENTIFIER";
  
//! value of object
  array(int) id;

  object init(int ...args)
    {
      if ( (sizeof(args) < 2)
	   || (args[0] > 2)
	   || (args[1] >= ( (args[0] < 2) ? 40 : 176) ))
	error( "asn1.encode.asn1_identifier->init: Invalid object identifier.\n" );
      id = args;
      return this_object();
    }

  mixed _encode() { return id; }
  void _decode(mixed data) { id=data; }

//! @fixme
//!   document me!
  object append(int ...args)
    {
      return object_program(this_object())(@id, @args);
    }
  
  string der_encode()
    {
      return build_der(sprintf("%c%@s", 40 * id[0] + id[1],
			       Array.map(id[2..], to_base_128)));
    }

  object decode_primitive(string contents)
    {
      record_der_contents(contents);

      if (contents[0] < 120)
	id = ({ contents[0] / 40, contents[0] % 40 });
      else
	id = ({ 2, contents[0] - 80 });
      int index = 1;
      while(index < sizeof(contents))
      {
	int element = 0;
	do
	{
	  element = element << 7 | (contents[index] & 0x7f);
	} while(contents[index++] & 0x80);
	id += ({ element });
      }
      return this_object();
    }

  string debug_string()
    {
      return "IDENTIFIER " + (array(string)) id * ".";
    }

  int `==(mixed other)
    {
      return (objectp(other) &&
	      (object_program(this_object()) == object_program(other)) &&
	      equal(id, other->id));
    }
}

int asn1_utf8_valid (string s)
{
  return 1;
}

//! UTF8 string object
//!
//! Character set: ISO/IEC 10646-1 (compatible with Unicode).
//!
//! Variable width encoding, see rfc2279.
class asn1_utf8_string
{

  inherit asn1_string;
  constant tag = 12;
  constant type_name = "UTF8String";

  string der_encode()
    {
      return build_der(string_to_utf8(value));
    }

  object decode_primitive(string contents)
    {
      record_der(contents);
      if (catch {
       value = utf8_to_string(contents);
      })
       return 0;
      
      return this_object();
    }
}  

//! Sequence object
class asn1_sequence
{
  inherit asn1_compound;
  constant tag = 16;
  constant type_name = "SEQUENCE";
  
  string der_encode()
    {
      WERROR(sprintf("asn1_sequence->der: elements = '%O\n",
		     elements));
      array(string) a = Array.map(elements, "get_der");
      WERROR(sprintf("asn1_sequence->der: der_encode(elements) = '%O\n", a));
      return build_der(`+("", @ a));
    }
}

//! Set object
class asn1_set
{
  inherit asn1_compound;
  constant tag = 17;
  constant type_name = "SET";
  
  int compare_octet_strings(string r, string s)
    {
      for(int i = 0;; i++)
      {
	if (i == sizeof(r))
	  return (i = sizeof(s)) ? 0 : 1;
	if (i == sizeof(s))
	  return -1;
	if (r[i] < s[i])
	  return 1;
	else if (r[i] > s[i])
	  return -1;
      }
    }

  string der_encode()
    {
      WERROR(sprintf("asn1_set->der: elements = '%O\n",
		     elements));
      array(string) a = Array.map(elements, "get_der");
      WERROR(sprintf("asn1_set->der: der_encode(elements) = '%O\n", a));
      return build_der(`+("", @Array.sort_array(a, compare_octet_strings)));
    }
}

static int wide_string (string s)
{
  int i, m;
  for (int i = 0; i < sizeof (s); i += 500)
    m = max (m, @values (s[i..i + 499]));
  if (m < 256) return 0;
  else if (m < 65536) return 1;
  else return 2;
}

object(Regexp) asn1_printable_invalid_chars =
  Regexp ("([^-A-Za-z0-9 '()+,./:=?])");

int asn1_printable_valid (string s)
{
  if (wide_string (s)) return 0;
  return !asn1_printable_invalid_chars->match (s);
}

//! PrintableString object
class asn1_printable_string
{
  inherit asn1_string;
  constant tag = 19;
  constant type_name = "PrintableString";
}

object(Regexp) asn1_teletex_invalid_chars =
  Regexp ("([\\\\{}\240����\255�])");

int asn1_teletex_valid (string s)
{
  if (wide_string (s))
    // T.61 encoding of wide strings not implemented.
    return 0;
  return !asn1_teletex_invalid_chars->match (s);
}

//! TeletexString object
//!
//! Avoid this one; it seems to be common that this type is used to
//! label strings encoded with the ISO 8859-1 character set (use
//! asn1_broken_teletex_string for that). From
//! http://www.mindspring.com/~asn1/nlsasn.htm:
//!
//! /.../ Types GeneralString, VideotexString, TeletexString
//! (T61String), and GraphicString exist in earlier versions
//! [pre-1994] of ASN.1. They are considered difficult to use
//! correctly by applications providing national language support.
//! Varying degrees of application support for T61String values seems
//! to be most common in older applications. Correct support is made
//! more difficult, as character values available in type T61String
//! have changed with the addition of new register entries from the
//! 1984 through 1997 versions.
//!
//! This implementation is based on the description of T.61 and T.51
//! in "Some functions for representing T.61 characters from the
//! X.500 Directory Service in ISO 8859 terminals (Version 0.2. July
//! 1994.)" by Enrique Silvestre Mora (mora@@si.uji.es), Universitat
//! Jaume I, Spain, found in the package
//! ftp://pereiii.uji.es/pub/uji-ftp/unix/ldap/iso-t61.translation.tar.Z
//!
//! The translation is only complete for 8-bit latin 1 strings. It
//! encodes strictly to T.61, but decodes from the superset T.51.
class asn1_teletex_string
{

  inherit asn1_string;
  constant tag = 20;
  constant type_name = "TeletexString";	// Alias: T61String

#define ENC_ERR(char) char
#define DEC_ERR(str) str
#define DEC_COMB_MARK "\300"

#define GR(char) "\301" char	/* Combining grave accent */
#define AC(char) "\302" char	/* Combining acute accent */
#define CI(char) "\303" char	/* Combining circumflex accent */
#define TI(char) "\304" char	/* Combining tilde */
#define MA(char) "\305" char	/* Combining macron */
#define BR(char) "\306" char	/* Combining breve */
#define DA(char) "\307" char	/* Combining dot above */
#define DI(char) "\310" char	/* Combining diaeresis */
#define RA(char) "\312" char	/* Combining ring above */
#define CE(char) "\313" char	/* Combining cedilla */
#define UN(char) "\314" char	/* Combining underscore (note 6) */
#define DO(char) "\315" char	/* Combining double acute accent */
#define OG(char) "\316" char	/* Combining ogonek */
#define CA(char) "\317" char	/* Combining caron */

  constant encode_from = ({
    /*"#", "$",*/ "�",		// Note 3
    "\\", "{", "}",		// Note 7
    "\240",			// No-break space (note 7)
    "�",			// Multiplication sign
    "�",			// Division sign
    "�",			// Superscript one
    "�",			// Registered sign (note 7)
    "�",			// Copyright sign (note 7)
    "�",			// Not sign (note 7)
    "�",			// Broken bar (note 7)
    "�",			// Latin capital ligature ae
    "�",			// Feminine ordinal indicator
    "�",			// Latin capital letter o with stroke
    "�",			// Masculine ordinal indicator
    "�",			// Latin capital letter thorn
    "�",			// Latin small ligature ae
    "�",			// Latin small letter eth
    "�",			// Latin small letter o with stroke
    "�",			// Latin small letter sharp s
    "�",			// Latin small letter thorn
    "\255",			// Soft hyphen (note 7)
    "�",			// Latin capital letter eth (no equivalent)
    // Combinations
    "^", "`", "~",		// Note 4
    "�", "�", "�", "�",
    "�", "�", "�", "�", "�", "�",
    "�", "�", "�", "�", "�", "�",
    "�",
    "�",
    "�", "�", "�", "�",
    "�", "�", "�", "�",
    "�", "�", "�", "�",
    "�", "�", "�", "�",
    "�",
    "�",
    "�", "�", "�", "�", "�",
    "�", "�", "�", "�", "�",
    "�", "�", "�", "�",
    "�", "�", "�", "�",
    "�",
    "�",
  });

  constant encode_to = ({
    /*"#", "$",*/ "\250",	// Note 3
    ENC_ERR("\\"), ENC_ERR("{"), ENC_ERR("}"), // Note 7
    ENC_ERR("\240"),		// No-break space (note 7)
    "\264",			// Multiplication sign
    "\270",			// Division sign
    "\321",			// Superscript one
    ENC_ERR("�"),		// Registered sign (note 7)
    ENC_ERR("�"),		// Copyright sign (note 7)
    ENC_ERR("�"),		// Not sign (note 7)
    ENC_ERR("�"),		// Broken bar (note 7)
    "\341",			// Latin capital ligature ae
    "\343",			// Feminine ordinal indicator
    "\351",			// Latin capital letter o with stroke
    "\353",			// Masculine ordinal indicator
    "\354",			// Latin capital letter thorn
    "\361",			// Latin small ligature ae
    "\363",			// Latin small letter eth
    "\371",			// Latin small letter o with stroke
    "\373",			// Latin small letter sharp s
    "\374",			// Latin small letter thorn
    ENC_ERR("\255"),		// Soft hyphen (note 7)
    ENC_ERR("�"),		// Latin capital letter eth (no equivalent)
    // Combinations
    CI(" "), GR(" "), TI(" "),	// Note 4
    AC(" "), DI(" "), MA(" "), CE(" "),
    GR("A"), AC("A"), CI("A"), TI("A"), DI("A"), RA("A"),
    GR("a"), AC("a"), CI("a"), TI("a"), DI("a"), RA("a"),
    CE("C"),
    CE("c"),
    GR("E"), AC("E"), CI("E"), DI("E"),
    GR("e"), AC("e"), CI("e"), DI("e"),
    GR("I"), AC("I"), CI("I"), DI("I"),
    GR("i"), AC("i"), CI("i"), DI("i"),
    TI("N"),
    TI("n"),
    GR("O"), AC("O"), CI("O"), TI("O"), DI("O"),
    GR("o"), AC("o"), CI("o"), TI("o"), DI("o"),
    GR("U"), AC("U"), CI("U"), DI("U"),
    GR("u"), AC("u"), CI("u"), DI("u"),
    GR("Y"),
    GR("y"),
  });

  constant decode_from = ({
    /*"#", "$",*/ "\244", "\246", "\250", // Note 3
    /*"^", "`", "~",*/		// Note 4
    "\251",			// Left single quotation mark (note 7)
    "\252",			// Left double quotation mark (note 7)
    "\254",			// Leftwards arrow (note 7)
    "\255",			// Upwards arrow (note 7)
    "\256",			// Rightwards arrow (note 7)
    "\257",			// Downwards arrow (note 7)
    "\264",			// Multiplication sign
    "\270",			// Division sign
    "\271",			// Right single quotation mark (note 7)
    "\272",			// Right double quotation mark (note 7)
    "\300",			// Note 5
    GR(""),			// Combining grave accent
    AC(""),			// Combining acute accent
    CI(""),			// Combining circumflex accent
    TI(""),			// Combining tilde
    MA(""),			// Combining macron
    BR(""),			// Combining breve
    DA(""),			// Combining dot above
    DI(""),			// Combining diaeresis
    "\311",			// Note 5
    RA(""),			// Combining ring above
    CE(""),			// Combining cedilla
    UN(""),			// Combining underscore (note 6)
    DO(""),			// Combining double acute accent
    OG(""),			// Combining ogonek
    CA(""),			// Combining caron
    "\320",			// Em dash (note 7)
    "\321",			// Superscript one
    "\322",			// Registered sign (note 7)
    "\323",			// Copyright sign (note 7)
    "\324",			// Trade mark sign (note 7)
    "\325",			// Eighth note (note 7)
    "\326",			// Not sign (note 7)
    "\327",			// Broken bar (note 7)
    "\330", "\331", "\332", "\333", // Note 2
    "\334",			// Vulgar fraction one eighth (note 7)
    "\335",			// Vulgar fraction three eighths (note 7)
    "\336",			// Vulgar fraction five eighths (note 7)
    "\337",			// Vulgar fraction seven eighths (note 7)
    "\340",			// Ohm sign
    "\341",			// Latin capital ligature ae
    "\342",			// Latin capital letter d with stroke
    "\343",			// Feminine ordinal indicator
    "\344",			// Latin capital letter h with stroke
    "\345",			// Note 2
    "\346",			// Latin capital ligature ij
    "\347",			// Latin capital letter l with middle dot
    "\350",			// Latin capital letter l with stroke
    "\351",			// Latin capital letter o with stroke
    "\352",			// Latin capital ligature oe
    "\353",			// Masculine ordinal indicator
    "\354",			// Latin capital letter thorn
    "\355",			// Latin capital letter t with stroke
    "\356",			// Latin capital letter eng
    "\357",			// Latin small letter n preceded by apostrophe
    "\360",			// Latin small letter kra
    "\361",			// Latin small ligature ae
    "\362",			// Latin small letter d with stroke
    "\363",			// Latin small letter eth
    "\364",			// Latin small letter h with stroke
    "\365",			// Latin small letter dotless i
    "\366",			// Latin small ligature ij
    "\367",			// Latin small letter l with middle dot
    "\370",			// Latin small letter l with stroke
    "\371",			// Latin small letter o with stroke
    "\372",			// Latin small ligature oe
    "\373",			// Latin small letter sharp s
    "\374",			// Latin small letter thorn
    "\375",			// Latin small letter t with stroke
    "\376",			// Latin small letter eng
    "\377",			// Soft hyphen (note 7)
  });

  constant decode_to = ({
    /*"#", "$",*/ "$", "#", "\244", // Note 3
    /*"^", "`", "~",*/		// Note 4
    DEC_ERR("\251"),		// Left single quotation mark (note 7)
    DEC_ERR("\252"),		// Left double quotation mark (note 7)
    DEC_ERR("\254"),		// Leftwards arrow (note 7)
    DEC_ERR("\255"),		// Upwards arrow (note 7)
    DEC_ERR("\256"),		// Rightwards arrow (note 7)
    DEC_ERR("\257"),		// Downwards arrow (note 7)
    "�",			// Multiplication sign
    "�",			// Division sign
    DEC_ERR("\271"),		// Right single quotation mark (note 7)
    DEC_ERR("\272"),		// Right double quotation mark (note 7)
    DEC_ERR("\300"),		// Note 5
    DEC_COMB_MARK GR(""),	// Combining grave accent
    DEC_COMB_MARK AC(""),	// Combining acute accent
    DEC_COMB_MARK CI(""),	// Combining circumflex accent
    DEC_COMB_MARK TI(""),	// Combining tilde
    DEC_COMB_MARK MA(""),	// Combining macron
    DEC_COMB_MARK BR(""),	// Combining breve
    DEC_COMB_MARK DA(""),	// Combining dot above
    DEC_COMB_MARK DI(""),	// Combining diaeresis
    DEC_ERR("\311"),		// Note 5
    DEC_COMB_MARK RA(""),	// Combining ring above
    DEC_COMB_MARK CE(""),	// Combining cedilla
    DEC_COMB_MARK UN(""),	// Combining underscore (note 6)
    DEC_COMB_MARK DO(""),	// Combining double acute accent
    DEC_COMB_MARK OG(""),	// Combining ogonek
    DEC_COMB_MARK CA(""),	// Combining caron
    DEC_ERR("\320"),		// Em dash (note 7)
    "�",			// Superscript one
    "�",			// Registered sign (note 7)
    "�",			// Copyright sign (note 7)
    DEC_ERR("\324"),		// Trade mark sign (note 7)
    DEC_ERR("\325"),		// Eighth note (note 7)
    "�",			// Not sign (note 7)
    "�",			// Broken bar (note 7)
    DEC_ERR("\330"), DEC_ERR("\331"), DEC_ERR("\332"), DEC_ERR("\333"), // Note 2
    DEC_ERR("\334"),		// Vulgar fraction one eighth (note 7)
    DEC_ERR("\335"),		// Vulgar fraction three eighths (note 7)
    DEC_ERR("\336"),		// Vulgar fraction five eighths (note 7)
    DEC_ERR("\337"),		// Vulgar fraction seven eighths (note 7)
    DEC_ERR("\340"),		// Ohm sign
    "�",			// Latin capital ligature ae
    DEC_ERR("\342"),		// Latin capital letter d with stroke
    "�",			// Feminine ordinal indicator
    DEC_ERR("\344"),		// Latin capital letter h with stroke
    DEC_ERR("\345"),		// Note 2
    DEC_ERR("\346"),		// Latin capital ligature ij
    DEC_ERR("\347"),		// Latin capital letter l with middle dot
    DEC_ERR("\350"),		// Latin capital letter l with stroke
    "�",			// Latin capital letter o with stroke
    DEC_ERR("\352"),		// Latin capital ligature oe
    "�",			// Masculine ordinal indicator
    "�",			// Latin capital letter thorn
    DEC_ERR("\355"),		// Latin capital letter t with stroke
    DEC_ERR("\356"),		// Latin capital letter eng
    DEC_ERR("\357"),		// Latin small letter n preceded by apostrophe
    DEC_ERR("\360"),		// Latin small letter kra
    "�",			// Latin small ligature ae
    DEC_ERR("\362"),		// Latin small letter d with stroke
    "�",			// Latin small letter eth
    DEC_ERR("\364"),		// Latin small letter h with stroke
    DEC_ERR("\365"),		// Latin small letter dotless i
    DEC_ERR("\366"),		// Latin small ligature ij
    DEC_ERR("\367"),		// Latin small letter l with middle dot
    DEC_ERR("\370"),		// Latin small letter l with stroke
    "�",			// Latin small letter o with stroke
    DEC_ERR("\372"),		// Latin small ligature oe
    "�",			// Latin small letter sharp s
    "�",			// Latin small letter thorn
    DEC_ERR("\375"),		// Latin small letter t with stroke
    DEC_ERR("\376"),		// Latin small letter eng
    "\255",			// Soft hyphen (note 7)
  });

  constant decode_comb = ([
    GR(" "): "`",
    AC(" "): "�",
    CI(" "): "^",
    TI(" "): "~",
    DI(" "): "�",
    // RA(" "): DEC_ERR(RA(" ")),
    MA(" "): "�",
    // BR(" "): DEC_ERR(BR(" ")),
    // DA(" "): DEC_ERR(DA(" ")),
    CE(" "): "�",
    // DO(" "): DEC_ERR(DO(" ")),
    // OG(" "): DEC_ERR(OG(" ")),
    // CA(" "): DEC_ERR(CA(" ")),
    GR("A"): "�", AC("A"): "�", CI("A"): "�", TI("A"): "�", DI("A"): "�", RA("A"): "�",
    GR("a"): "�", AC("a"): "�", CI("a"): "�", TI("a"): "�", DI("a"): "�", RA("a"): "�",
    CE("C"): "�",
    CE("c"): "�",
    GR("E"): "�", AC("E"): "�", CI("E"): "�", DI("E"): "�",
    GR("e"): "�", AC("e"): "�", CI("e"): "�", DI("e"): "�",
    GR("I"): "�", AC("I"): "�", CI("I"): "�", DI("I"): "�",
    GR("i"): "�", AC("i"): "�", CI("i"): "�", DI("i"): "�",
    TI("N"): "�",
    TI("n"): "�",
    GR("O"): "�", AC("O"): "�", CI("O"): "�", TI("O"): "�", DI("O"): "�",
    GR("o"): "�", AC("o"): "�", CI("o"): "�", TI("o"): "�", DI("o"): "�",
    GR("U"): "�", AC("U"): "�", CI("U"): "�", DI("U"): "�",
    GR("u"): "�", AC("u"): "�", CI("u"): "�", DI("u"): "�",
    GR("Y"): "�",
    GR("y"): "�",
  ]);

  /* Notes from Moras paper:

     (1) All characters in 0xC0-0xCF are non-spacing characters.  They are
     all diacritical marks.  To be represented stand-alone, they need to
     be followed by a SPACE (0x20).  They can appear, also, before
     letters if the couple is one of the defined combinations.

     (2) Reserved for future standardization.

     (3) Current terminals may send and receive 0xA6 and 0xA4 for the NUMBER
     SIGN and DOLLAR SIGN, respectively.  When receiving codes 0x23 and
     0x24, they may interpret them as NUMBER SIGN and CURRENCY SIGN,
     respectively.  Future applications should code the NUMBER SIGN,
     DOLLAR SIGN and CURRENCY SIGN as 0x23, 0x24 and 0xA8, respectively.

     (4) Terminals should send only the codes 0xC1, 0xC3 and 0xC4, followed
     by SPACE (0x20) for stand-alone GRAVE ACCENT, CIRCUMFLEX ACCENT and
     TILDE, respectively.  Nevertheless the terminal shall interpret the
     codes 0x60, 0x5E and 0x7E as GRAVE, CIRCUMFLEX and TILDE,
     respectively.

     (5) This code position is reserved and shall not be used.

     (6) It is recommended to implement the "underline" function by means of
     the control function SGR(4) instead of the "non-spacing underline"
     graphic character.

     (7) Not used in current teletex service (Recommendation T.61).
  */

#undef GR
#undef AC
#undef CI
#undef TI
#undef MA
#undef BR
#undef DA
#undef DI
#undef RA
#undef CE
#undef UN
#undef DO
#undef OG
#undef CA

  string der_encode()
  {
    return build_der (replace (value, encode_from, encode_to));
  }

  object decode_primitive (string contents)
  {
    record_der (contents);

    array(string) parts =
      replace (contents, decode_from, decode_to) / DEC_COMB_MARK;
    value = parts[0];
    foreach (parts[1..], string part)
      value += (decode_comb[part[..1]] || DEC_ERR(part[..1])) + part[2..];

    return this_object();
  }

#undef ENC_ERR
#undef DEC_ERR
#undef DEC_COMB_MARK
}

int asn1_broken_teletex_valid (string s)
{
  return !wide_string (s);
}

//! (broken) TeletexString object
//!
//! Encodes and decodes latin1, but labels it TeletexString, as is
//! common in many broken programs (e.g. Netscape 4.0X).
class asn1_broken_teletex_string
{

  inherit asn1_string;
  constant tag = 20;
  constant type_name = "TeletexString";	// Alias: T61String
}

object(Regexp) asn1_IA5_invalid_chars =
  Regexp ("([\180-\377])");

int asn1_IA5_valid (string s)
{
  if (wide_string (s)) return 0;
  return !asn1_printable_invalid_chars->match (s);
}

//! IA5 String object
//!
//! Character set: ASCII. Fixed width encoding with 1 octet per
//! character.
class asn1_IA5_string
{

  inherit asn1_string;
  constant tag = 22;
  constant type_name = "IA5STRING";
}

class asn1_utc
{
  inherit asn1_string;
  constant tag = 23;
  constant type_name = "UTCTime";
}

int asn1_universal_valid (string s)
{
  return 1;
}

//! Universal String object
//!
//! Character set: ISO/IEC 10646-1 (compatible with Unicode).
//! Fixed width encoding with 4 octets per character.
//!
//! @fixme
//! The encoding is very likely UCS-4, but that's not yet verified.
class asn1_universal_string
{
  inherit asn1_octet_string;
  constant tag = 28;
  constant type_name = "UniversalString";

  string der_encode()
  {
    error( "asn1_universal_string: Encoding not implemented\n" );
  }

  object decode_primitive (string contents)
  {
    error( "asn1_universal_string: Decoding not implemented\n" );
  }
}

int asn1_bmp_valid (string s)
{
  return wide_string (s) <= 1;
}

//! BMP String object
//!
//! Character set: ISO/IEC 10646-1 (compatible with Unicode).
//! Fixed width encoding with 2 octets per character.
//!
//! FIXME: The encoding is very likely UCS-2, but that's not yet verified.
class asn1_bmp_string
{
  inherit asn1_octet_string;
  constant tag = 30;
  constant type_name = "BMPString";

  string der_encode()
  {
    return build_der (string_to_unicode (value));
  }

  object decode_primitive (string contents)
  {
    record_der (contents);
    value = unicode_to_string (contents);
    return this_object();
  }
}

//! meta-instances handle a particular explicit tag and set of types
//!
//! @fixme 
//!  document me!
class meta_explicit
{
  int real_tag;
  int real_cls;
  
  mapping valid_types;

  class `()
    {
      inherit asn1_compound;
      constant type_name = "EXPLICIT";
      constant constructed = 1;
      
      int get_tag() { return real_tag; }
      int get_cls() { return real_cls; }
      
      object contents;

      object init(object o)
	{
	  contents = o;
	  return this_object();
	}
    
      string der_encode()
	{
	  WERROR(sprintf("asn1_explicit->der: contents = '%O\n",
			 contents));
	  return build_der(contents->get_der());
	}

      object decode_constructed_element(int i, object e)
	{
	  if (i)
	    error("decode_constructed_element: Unexpected index!\n");
	  contents = e;
	  return this_object();
	}

      object end_decode_constructed(int length)
      {
	if (length != 1)
	  error("end_decode_constructed: length != 1!\n");
	return this_object();
      }

      mapping element_types(int i, mapping types)
	{
	  if (i)
	    error("element_types: Unexpected index!\n");
	  return valid_types || types;
	}

      string debug_string()
	{
	  return type_name + "[" + (int) real_tag + "]"
	    + contents->debug_string();
	}
    }
  
  void create(int cls, int tag, mapping|void types)
    {
      real_cls = cls;
      real_tag = tag;
      valid_types = types;
    }
}

#endif /* Gmp.mpz */
