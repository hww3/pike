// -*- Pike -*-

#pike __REAL_VERSION__
//! @ignore
protected private inherit _Charset;
//! @endignore

//! The Charset module supports a wide variety of different character sets, and
//! it is flexible in regard of the names of character sets it accepts. The
//! character case is ignored, as are the most common non-alaphanumeric
//! characters appearing in character set names. E.g. @expr{"iso-8859-1"@}
//! works just as well as @expr{"ISO_8859_1"@}. All encodings specified in
//! RFC 1345 are supported.
//!
//! First of all the Charset module is capable of handling the following
//! encodings of Unicode:
//!
//! @ul
//!   @item utf7
//!   @item utf8
//!   @item utf16
//!   @item utf16be
//!   @item utf16le
//!   @item utf75
//!   @item utf7�
//!     UTF encodings
//!   @item shiftjis
//!   @item euc-kr
//!   @item euc-cn
//!   @item euc-jp
//! @endul
//!
//! Most, if not all, of the relevant code pages are represented, as the
//! following list shows. Prefix the numbers as noted in the list to get
//! the wanted codec:
//!
//! @ul
//!   @item 037
//!   @item 038
//!   @item 273
//!   @item 274
//!   @item 275
//!   @item 277
//!   @item 278
//!   @item 280
//!   @item 281
//!   @item 284
//!   @item 285
//!   @item 290
//!   @item 297
//!   @item 367
//!   @item 420
//!   @item 423
//!   @item 424
//!   @item 437
//!   @item 500
//!   @item 819
//!   @item 850
//!   @item 851
//!   @item 852
//!   @item 855
//!   @item 857
//!   @item 860
//!   @item 861
//!   @item 862
//!   @item 863
//!   @item 864
//!   @item 865
//!   @item 866
//!   @item 868
//!   @item 869
//!   @item 870
//!   @item 871
//!   @item 880
//!   @item 891
//!   @item 903
//!   @item 904
//!   @item 905
//!   @item 918
//!   @item 932
//!   @item 936
//!   @item 950
//!   @item 1026
//!     These may be prefixed with @expr{"cp"@}, @expr{"ibm"@} or
//!     @expr{"ms"@}.
//!   @item 1250
//!   @item 1251
//!   @item 1252
//!   @item 1253
//!   @item 1254
//!   @item 1255
//!   @item 1256
//!   @item 1257
//!   @item 1258
//!     These may be prefixed with @expr{"cp"@}, @expr{"ibm"@},
//!     @expr{"ms"@} or @expr{"windows"@}
//!   @item mysql-latin1
//!     The default charset in MySQL, similar to @expr{cp1252@}.
//! @endul
//!
//! +359 more.

//! Virtual base class for charset decoders.
//! @example
//!   string win1252_to_string( string data )
//!   {
//!     return Locale.Charset.decoder("windows-1252")->feed( data )->drain();
//!   }
class Decoder
{
  //! @decl string charset;
  //!
  //! Name of the charset - giving this name to @[decoder] returns an
  //! instance of the same class as this object.
  //!
  //! @note
  //! This is not necessarily the same name that was actually given to
  //! @[decoder] to produce this object.

  //! Feeds a string to the decoder.
  //!
  //! @param s
  //!   String to be decoded.
  //!
  //! @returns
  //!   Returns the current object, to allow for chaining
  //!   of calls.
  this_program feed(string s);

  // FIXME: There ought to be a finish(string s) function. Now it's
  // possible that certain kinds of coding errors are simply ignored
  // if they occur last in a string. E.g.
  // Locale.Charset.decoder("utf8")->feed("\345")->drain() returns ""
  // instead of throwing an error about the incomplete UTF8 sequence,
  // and there's no good way to get this error.

  //! Get the decoded data, and reset buffers.
  //!
  //! @returns
  //!   Returns the decoded string.
  string drain();

  //! Clear buffers, and reset all state.
  //!
  //! @returns
  //!   Returns the current object to allow for chaining
  //!   of calls.
  this_program clear();
}

//! Virtual base class for charset encoders.
class Encoder
{
  //! An encoder only differs from a decoder in that it has an extra function.
  inherit Decoder;

  //! @decl string charset;
  //!
  //! Name of the charset - giving this name to @[encoder] returns
  //! an instance of the same class as this one.
  //!
  //! @note
  //! This is not necessarily the same name that was actually given to
  //! @[encoder] to produce this object.

  //! Change the replacement callback function.
  //!
  //! @param rc
  //!   Function that is called to encode characters
  //!   outside the current character encoding.
  void set_replacement_callback(function(string:string) rc);
}

private class ASCIIDec {
  constant charset = "iso88591";
  protected private string s = "";
  this_program feed(string ss)
  {
    s += ss;
    return this;
  }
  string drain()
  {
    string ss = s;
    s = "";
    return ss;
  }
  this_program clear()
  {
    s = "";
    return this;
  }
}

private class UTF16dec {
  inherit ASCIIDec;
  constant charset = "utf16";
  protected int check_bom=1, le=0;
  string drain() {
    string s = ::drain();
    if(sizeof(s)&1) {
      feed(s[sizeof(s)-1..]);
      s = s[..<1];
    }
    if(check_bom && sizeof(s))
      switch(s[..1]) {
       case "\xfe\xff":
       case "\xff\xfe":
	 le=(s[0]==0xff);
	 s=s[2..];
       default:
	 check_bom=0;
      }
    if(le)
      s = map(s/2, reverse)*"";
    return unicode_to_string(s);
  }
}

private class UTF16LEdec {
  inherit UTF16dec;
  constant charset = "utf16le";
  protected void create() { le=1; }
}

private class ISO6937dec {
  protected Decoder decoder = rfc1345("iso6937");
  protected string trailer = "";
  string drain()
  {
    string res = trailer + decoder->drain();
    trailer = "";
    if (String.width(res) <= 8) {
      return res;
    }
    if ((res[-1] >= 0x0300) && (res[-1] < 0x0370)) {
      // Ends with a combiner. Keep it for later.
      trailer = res[sizeof(res)-1..];
      res = res[..sizeof(res)-2];
    }
    // Swap any combiners with the following character.
    array(int) chars = (array(int)) res;
    int swapped;
    int i;
    foreach(res; int i; int c) {
      if ((c < 0x0300) || (c >= 0x0370)) continue;
      chars[i] = chars[i+1];
      chars[i+1] = c;
      swapped = 1;
    }
    if (!swapped) return res;
    // Recombine the characters.
    return Unicode.normalize((string)chars, "NFC");
  }
  this_program feed(string s)
  {
    decoder->feed(s);
    return this;
  }  
  this_program clear()
  {
    decoder->clear();
    trailer = "";
    return this;
  }
}

// Decode GSM 03.38.
private class GSM03_38dec {
  static Decoder decoder = rfc1345("gsm0338");
  static string trailer = "";
  string drain()
  {
    // Escape sequences for GSM 03.38.
    // cf http://en.wikipedia.org/wiki/Short_message_service
    string res =
      replace(trailer + decoder->drain(),
	      "\e�\e\u039b\e(\e)\e/\e<\e=\e>\e�\ee"/2,
	      "\f^{}\\[~]|\u20ac"/1);
    trailer = "";
    if (sizeof(res) && res[-1] == '\e') trailer = "\e";
    return replace(res, "\e", "");
  }
  this_program feed(string s)
  {
    decoder->feed(s);
    return this;
  }  
  this_program clear()
  {
    decoder->clear();
    trailer = "";
    return this;
  }
}

//! All character set names are normalized through this function
//! before compared.
string normalize(string in) {
  if(!in) return 0;
  string out = replace(lower_case(in),
		      ({ "_",".",":","-","(",")" }),
		      ({ "","","","","","" }));

  if (has_prefix(out, "unicode")) {
    // The mailserver at wi2-96.rz.fh-wiesbaden.de uses the encoding
    // unicode-1-1-utf-7. We're not interested in the Unicode version...
    sscanf(out, "unicode%*d%s", out);
  }

  if( (out=="isoir91" || out=="isoir92") && in[-2]!='9')
    return sprintf("%s-%c", out[..<1], out[-1]);

  sscanf(out, "cs%s", out);

  int cp;
  sscanf(out, "cp%d", cp) ||
    sscanf(out, "ms%d", cp) ||
    sscanf(out, "ibm%d", cp) ||
    sscanf(out, "ccsid%d", cp) ||
    sscanf(out, "%d", cp);

  if(cp==932) return "shiftjis";
  if(cp) return (string)cp;

  return out;
}

private mapping(string:Decoder) custom_decoders = ([]);

//! Adds a custom defined character set decoder. The name is
//! normalized through the use of @[normalize].
void set_decoder(string name, Decoder decoder)
{
  custom_decoders[normalize(name)]=decoder;
}

//! Returns a charset decoder object.
//! @param name
//!   The name of the character set to decode from. Supported charsets
//!   include (not all supported charsets are enumerable):
//!   "iso_8859-1:1987", "iso_8859-1:1998", "iso-8859-1", "iso-ir-100",
//!   "latin1", "l1", "ansi_x3.4-1968", "iso_646.irv:1991", "iso646-us",
//!   "iso-ir-6", "us", "us-ascii", "ascii", "cp367", "ibm367", "cp819",
//!   "ibm819", "iso-2022" (of various kinds), "utf-7", "utf-8" and
//!   various encodings as described by RFC1345.
//! @throws
//!   If the asked-for @[name] was not supported, an error is thrown.
Decoder decoder(string name)
{
  name = normalize(name);

  if( custom_decoders[name] )
    return custom_decoders[name]();

  if(!name || (<
    "iso885911987", "iso885911998", "iso88591", "isoir100",
    "latin1", "l1", "ansix341968", "iso646irv1991", "iso646us",
    "isoir6", "us", "usascii", "ascii", "367", "819",
    "isolatin1">)[name])
    return ASCIIDec();

  if(has_prefix(name, "iso2022"))
    return ISO2022Dec();

  program p = ([
    "utf7": UTF7dec,
    "utf8": UTF8dec,
    "utfebcdic": UTF_EBCDICdec,
    "utf16": UTF16dec,
    "utf16be": UTF16dec,
    "utf16le": UTF16LEdec,
    "utf75": UTF7_5dec,
    "utf7�": UTF7_5dec,
    "shiftjis": ShiftJisDec,
    "mskanji": ShiftJisDec,
    "isoir156": ISO6937dec,
    "iso6937": ISO6937dec,
    "iso69372001": ISO6937dec,
    "gsm": GSM03_38dec,
    "gsm0338": GSM03_38dec,
  ])[name];

  if(p)
    return p();

  if(has_prefix(name, "euc")) {
    string sub = ([
      "kr":"korean",
      "jp":"x0208",
      "cn":"chinese",
    ])[name[3..]];

    if(sub)
      return EUCDec(sub, name);
  }

  if( (< "extendedunixcodepackedformatforjapanese",
	 "eucpkdfmtjapanese" >)[ name ] )
    return EUCDec("x0208", "eucpkdfmtjapanese");


  if( (< "gb18030", "gbk", "936", "949" >)[ name ] )
    return MulticharDec(name);

  Decoder o = rfc1345(name);

  if(o)
    return o;

  if ((o = Locale.Charset.Tables[name]) &&
      (p = o->decoder)) {
    return p();
  }

  error("Unknown character encoding "+name+"\n");
}

private class ASCIIEnc
{
  constant charset = "iso88591";
  protected string s = "";
  protected string|void replacement;
  protected function(string:string)|void repcb;
  protected string low_convert(string s, string|void r,
			       function(string:string)|void rc)
  {
    int i = sizeof(s);
    string rr;
    while(--i>=0)
      if(s[i]>255)
	if(rc && (rr = rc(s[i..i])))
	  s=s[..i-1]+low_convert(rr,r)+s[i+1..];
	else if(r)
	  s=s[..i-1]+low_convert(r)+s[i+1..];
	else
	  encode_error (s, i, charset, "Character unsupported by encoding.\n");
    return s;
  }
  this_program feed(string ss)
  {
    s += low_convert(ss, replacement, repcb);
    return this;
  }
  string drain()
  {
    string ss = s;
    s = "";
    return ss;
  }
  this_program clear()
  {
    s = "";
    return this;
  }
  void set_replacement_callback(function(string:string) rc)
  {
    repcb = rc;
  }
  protected void create(string|void r, string|void rc)
  {
    replacement = r;
    repcb = rc;
  }
}

private class UTF16enc {
  inherit ASCIIEnc;
  constant charset = "utf16";
  protected private string low_convert(string s, string|void r,
				       function(string:string)|void rc)
  {
    int i = sizeof(s);
    string rr;
    while(--i>=0)
      if(s[i]>0x10ffff)
	if(rc && (rr = rc(s[i..i])))
	  s=s[..i-1]+low_convert(rr,r)+s[i+1..];
	else if(r)
	  s=s[..i-1]+low_convert(r)+s[i+1..];
	else
	  encode_error (s, i, charset, "Character unsupported by encoding.\n");
    return s;
  }
  this_program feed(string ss) {
    s += ss;
    return this;
  }
  string drain() {
    string ss = s;
    s = "";
    catch {
      return string_to_unicode(ss);
    };
    ss = low_convert(ss, replacement, repcb);
    return string_to_unicode(ss);
  }
}

private class UTF16LEenc {
  inherit UTF16enc;
  constant charset = "utf16le";
  string drain() {
    return map(::drain()/2, reverse)*"";
  }
}

private class ISO6937enc {
  protected Encoder encoder;
  protected void create(string|void replacement,
		     function(string:string)|void repcb)
  {
    encoder = rfc1345("iso6937", 1, replacement, repcb);
  }
  string drain()
  {
    return encoder->drain();
  }
  this_program feed(string s)
  {
    s = Unicode.normalize(s, "NFD");
    if (String.width(s) > 8) {
      // Swap any combiners with the preceeding character.
      array(int) chars = (array(int)) s;
      int swapped;
      int i;
      foreach(s; int i; int c) {
	if ((!i) || (c < 0x0300) || (c >= 0x0370)) continue;
	chars[i] = chars[i-1];
	chars[i-1] = c;
	swapped = 1;
      }
      if (swapped) {
	s = (string)chars;
      }
    }
    encoder->feed(s);
    return this;
  }  
  this_program clear()
  {
    encoder->clear();
    return this;
  }
}

// Encode GSM 03.38.
private class GSM03_38enc {
  static Encoder encoder;
  static void create(string|void replacement,
		     function(string:string)|void repcb)
  {
    encoder = rfc1345("gsm0338", 1, replacement, repcb);
  }
  string drain()
  {
    return encoder->drain();
  }
  this_program feed(string s)
  {
    // Escape sequences for GSM 03.38.
    // cf http://en.wikipedia.org/wiki/Short_message_service
    s = replace(s,
		"\f^{}\\[~]|\u20ac"/1,
		"\e�\e\u039b\e(\e)\e/\e<\e=\e>\e�\ee"/2);
    encoder->feed(s);
    return this;
  }  
  this_program clear()
  {
    encoder->clear();
    return this;
  }
}

private mapping(string:Encoder) custom_encoders = ([]);

//! Adds a custom defined character set encoder. The name is
//! normalized through the use of @[normalize].
void set_encoder(string name, Encoder encoder)
{
  custom_encoders[normalize(name)]=encoder;
}

//! Returns a charset encoder object.
//!
//! @param name
//!   The name of the character set to encode to. Supported charsets
//!   include (not all supported charsets are enumerable):
//!   "iso_8859-1:1987", "iso_8859-1:1998", "iso-8859-1", "iso-ir-100",
//!   "latin1", "l1", "ansi_x3.4-1968", "iso_646.irv:1991", "iso646-us",
//!   "iso-ir-6", "us", "us-ascii", "ascii", "cp367", "ibm367", "cp819",
//!   "ibm819", "iso-2022" (of various kinds), "utf-7", "utf-8" and
//!   various encodings as described by RFC1345.
//!
//! @param replacement
//!   The string to use for characters that cannot be represented in
//!   the charset. It's used when @[repcb] is not given or when it returns
//!   zero. If no replacement string is given then an error is thrown
//!   instead.
//!
//! @param repcb
//!   A function to call for every character that cannot be
//!   represented in the charset. If specified it's called with one
//!   argument - a string containing the character in question. If it
//!   returns a string then that one will replace the character in the
//!   output. If it returns something else then the @[replacement]
//!   argument will be used to decide what to do.
//!
//! @throws
//!   If the asked-for @[name] was not supported, an error is thrown.
Encoder encoder(string name, string|void replacement,
		function(string:string)|void repcb)
{
  name = normalize(name);

  if( custom_encoders[name] )
    return custom_encoders[name](replacement, repcb);

  if(!name || (<
    "iso885911987", "iso885911998", "iso88591", "isoir100",
    "latin1", "l1", "ansix341968", "iso646irv1991", "iso646us",
    "isoir6", "us", "usascii", "ascii", "367", "819",
    "isolatin1">)[name])
    // FIXME: This doesn't accurately check the range of valid
    // characters according to the chosen charset.
    return ASCIIEnc(replacement, repcb);

  if(has_prefix(name, "iso2022"))
    return ISO2022Enc(name[7..], replacement, repcb);

  program p = ([
    "utf7": UTF7enc,
    "utf8": UTF8enc,
    "utfebcdic": UTF_EBCDICenc,
    "utf16": UTF16enc,
    "utf16be": UTF16enc,
    "utf16le": UTF16LEenc,
    "utf75": UTF7_5enc,
    "utf7�": UTF7_5enc,
    "gb18030": GB18030Enc,
    "gbk": GBKenc,
    "gsm": GSM03_38enc,
    "gsm0338": GSM03_38enc,
    "936": GBKenc,
    "shiftjis": ShiftJisEnc,
    "mskanji": ShiftJisEnc,
    "isoir156": ISO6937enc,
    "iso6937": ISO6937enc,
    "iso69372001": ISO6937enc,
  ])[name];

  if(p)
    return p(replacement, repcb);

  if(has_prefix(name, "euc")) {
    string sub = ([
      "kr":"korean",
      "jp":"x0208",
      "cn":"chinese",
    ])[name[3..]];

    if(sub)
      return EUCEnc(sub, name, replacement, repcb);
  }

  if( (< "extendedunixcodepackedformatforjapanese",
	 "eucpkdfmtjapanese" >)[ name ] )
    return EUCEnc("x0208", "eucpkdfmtjapanese", replacement, repcb);

  Encoder o = rfc1345(name, 1, replacement, repcb);

  if(o)
    return o;

  if ((o = Locale.Charset.Tables[name]) &&
      (p = o->encoder)) {
    return p(replacement, repcb);
  }

  error("Unknown character encoding "+name+"\n");
}


protected constant MIBenum = ([
  3:"ANSI_X3.4-1968",
  4:"ISO_8859-1:1987",
  5:"ISO_8859-2:1987",
  6:"ISO_8859-3:1988",
  7:"ISO_8859-4:1988",
  8:"ISO_8859-5:1988",
  9:"ISO_8859-6:1987",
  10:"ISO_8859-7:1987",
  11:"ISO_8859-8:1988",
  12:"ISO_8859-9:1989",
  13:"ISO-8859-10",
  14:"ISO_6937-2-add",
  15:"JIS_X0201",
  16:"JIS_Encoding",
  17:"Shift_JIS",
  18:"Extended_UNIX_Code_Packed_Format_for_Japanese",
  19:"Extended_UNIX_Code_Fixed_Width_for_Japanese",
  20:"BS_4730",
  21:"SEN_850200_C",
  22:"IT",
  23:"ES",
  24:"DIN_66003",
  25:"NS_4551-1",
  26:"NF_Z_62-010",
  27:"ISO-10646-UTF-1",
  28:"ISO_646.basic:1983",
  29:"INVARIANT",
  30:"ISO_646.irv:1983",
  31:"NATS-SEFI",
  32:"NATS-SEFI-ADD",
  33:"NATS-DANO",
  34:"NATS-DANO-ADD",
  35:"SEN_850200_B",
  36:"KS_C_5601-1987",
  37:"ISO-2022-KR",
  38:"EUC-KR",
  39:"ISO-2022-JP",
  40:"ISO-2022-JP-2",
  41:"JIS_C6220-1969-jp",
  42:"JIS_C6220-1969-ro",
  43:"PT",
  44:"greek7-old",
  45:"latin-greek",
  46:"NF_Z_62-010_(1973)",
  47:"Latin-greek-1",
  48:"ISO_5427",
  49:"JIS_C6226-1978",
  50:"BS_viewdata",
  51:"INIS",
  52:"INIS-8",
  53:"INIS-cyrillic",
  54:"ISO_5427:1981",
  55:"ISO_5428:1980",
  56:"GB_1988-80",
  57:"GB_2312-80",
  58:"NS_4551-2",
  59:"videotex-suppl",
  60:"PT2",
  61:"ES2",
  62:"MSZ_7795.3",
  63:"JIS_C6226-1983",
  64:"greek7",
  65:"ASMO_449",
  66:"iso-ir-90",
  67:"JIS_C6229-1984-a",
  68:"JIS_C6229-1984-b",
  69:"JIS_C6229-1984-b-add",
  70:"JIS_C6229-1984-hand",
  71:"JIS_C6229-1984-hand-add",
  72:"JIS_C6229-1984-kana",
  73:"ISO_2033-1983",
  74:"ANSI_X3.110-1983",
  75:"T.61-7bit",
  76:"T.61-8bit",
  77:"ECMA-cyrillic",
  78:"CSA_Z243.4-1985-1",
  79:"CSA_Z243.4-1985-2",
  80:"CSA_Z243.4-1985-gr",
  81:"ISO_8859-6-E",
  82:"ISO_8859-6-I",
  83:"T.101-G2",
  84:"ISO_8859-8-E",
  85:"ISO_8859-8-I",
  86:"CSN_369103",
  87:"JUS_I.B1.002",
  88:"IEC_P27-1",
  89:"JUS_I.B1.003-serb",
  90:"JUS_I.B1.003-mac",
  91:"greek-ccitt",
  92:"NC_NC00-10:81",
  93:"ISO_6937-2-25",
  94:"GOST_19768-74",
  95:"ISO_8859-supp",
  96:"ISO_10367-box",
  97:"latin-lap",
  98:"JIS_X0212-1990",
  99:"DS_2089",
  100:"us-dk",
  101:"dk-us",
  102:"KSC5636",
  103:"UNICODE-1-1-UTF-7",
  104:"ISO-2022-CN",
  105:"ISO-2022-CN-EXT",
  106:"UTF-8",
  109:"ISO-8859-13",
  110:"ISO-8859-14",
  111:"ISO-8859-15",
  112:"ISO-8859-16",
  113:"GBK",
  114:"GB18030",
  1000:"ISO-10646-UCS-2",
  1001:"ISO-10646-UCS-4",
  1002:"ISO-10646-UCS-Basic",
  1003:"ISO-10646-Unicode-Latin1",
  1005:"ISO-Unicode-IBM-1261",
  1006:"ISO-Unicode-IBM-1268",
  1007:"ISO-Unicode-IBM-1276",
  1008:"ISO-Unicode-IBM-1264",
  1009:"ISO-Unicode-IBM-1265",
  1010:"UNICODE-1-1",
  1011:"SCSU",
  1012:"UTF-7",
  1013:"UTF-16BE",
  1014:"UTF-16LE",
  1015:"UTF-16",
  1016:"CESU-8",
  1017:"UTF-32",
  1018:"UTF-32BE",
  1019:"UTF-32LE",
  1020:"BOCU-1",
  2000:"ISO-8859-1-Windows-3.0-Latin-1",
  2001:"ISO-8859-1-Windows-3.1-Latin-1",
  2002:"ISO-8859-2-Windows-Latin-2",
  2003:"ISO-8859-9-Windows-Latin-5",
  2004:"hp-roman8",
  2005:"Adobe-Standard-Encoding",
  2006:"Ventura-US",
  2007:"Ventura-International",
  2008:"DEC-MCS",
  2009:"IBM850",
  2010:"IBM852",
  2011:"IBM437",
  2012:"PC8-Danish-Norwegian",
  2013:"IBM862",
  2014:"PC8-Turkish",
  2015:"IBM-Symbols",
  2016:"IBM-Thai",
  2017:"HP-Legal",
  2018:"HP-Pi-font",
  2019:"HP-Math8",
  2020:"Adobe-Symbol-Encoding",
  2021:"HP-DeskTop",
  2022:"Ventura-Math",
  2023:"Microsoft-Publishing",
  2024:"Windows-31J",
  2025:"GB2312",
  2026:"Big5",
  2027:"macintosh",
  2028:"IBM037",
  2029:"IBM038",
  2030:"IBM273",
  2031:"IBM274",
  2032:"IBM275",
  2033:"IBM277",
  2034:"IBM278",
  2035:"IBM280",
  2036:"IBM281",
  2037:"IBM284",
  2038:"IBM285",
  2039:"IBM290",
  2040:"IBM297",
  2041:"IBM420",
  2042:"IBM423",
  2043:"IBM424",
  2044:"IBM500",
  2045:"IBM851",
  2046:"IBM855",
  2047:"IBM857",
  2048:"IBM860",
  2049:"IBM861",
  2050:"IBM863",
  2051:"IBM864",
  2052:"IBM865",
  2053:"IBM868",
  2054:"IBM869",
  2055:"IBM870",
  2056:"IBM871",
  2057:"IBM880",
  2058:"IBM891",
  2059:"IBM903",
  2060:"IBM904",
  2061:"IBM905",
  2062:"IBM918",
  2063:"IBM1026",
  2064:"EBCDIC-AT-DE",
  2065:"EBCDIC-AT-DE-A",
  2066:"EBCDIC-CA-FR",
  2067:"EBCDIC-DK-NO",
  2068:"EBCDIC-DK-NO-A",
  2069:"EBCDIC-FI-SE",
  2070:"EBCDIC-FI-SE-A",
  2071:"EBCDIC-FR",
  2072:"EBCDIC-IT",
  2073:"EBCDIC-PT",
  2074:"EBCDIC-ES",
  2075:"EBCDIC-ES-A",
  2076:"EBCDIC-ES-S",
  2077:"EBCDIC-UK",
  2078:"EBCDIC-US",
  2079:"UNKNOWN-8BIT",
  2080:"MNEMONIC",
  2081:"MNEM",
  2082:"VISCII",
  2083:"VIQR",
  2084:"KOI8-R",
  2085:"HZ-GB-2312",
  2086:"IBM866",
  2087:"IBM775",
  2088:"KOI8-U",
  2089:"IBM00858",
  2090:"IBM00924",
  2091:"IBM01140",
  2092:"IBM01141",
  2093:"IBM01142",
  2094:"IBM01143",
  2095:"IBM01144",
  2096:"IBM01145",
  2097:"IBM01146",
  2098:"IBM01147",
  2099:"IBM01148",
  2100:"IBM01149",
  2101:"Big5-HKSCS",
  2102:"IBM1047",
  2103:"PTCP154",
  2250:"windows-1250",
  2251:"windows-1251",
  2252:"windows-1252",
  2253:"windows-1253",
  2254:"windows-1254",
  2255:"windows-1255",
  2256:"windows-1256",
  2257:"windows-1257",
  2258:"windows-1258",
  2259:"TIS-620"
]);

//! Returns a decoder for the encoding schema denoted by MIB @[mib].
Decoder decoder_from_mib(int mib) {
  Decoder d=MIBenum[mib] && decoder(MIBenum[mib]);
  if(!d) error("Unknown mib %d.\n", mib);
  return d;
}

//! Returns an encoder for the encoding schema denoted by MIB @[mib].
Encoder encoder_from_mib(int mib,  string|void replacement,
			 function(string:string)|void repcb) {
  Encoder e=MIBenum[mib] && encoder(MIBenum[mib], replacement, repcb);
  if(!e) error("Unknown mib %d.\n", mib);
  return e;
}

protected string format_err_msg (
  string intro, string err_str, int err_pos, string charset, string reason)
{
  string pre_context = err_pos > 23 ?
    sprintf ("...%O", err_str[err_pos - 20..err_pos - 1]) :
    err_pos > 0 ?
    sprintf ("%O", err_str[..err_pos - 1]) :
    "";
  string post_context = err_pos < sizeof (err_str) - 24 ?
    sprintf ("%O...", err_str[err_pos + 1..err_pos + 20]) :
    err_pos + 1 < sizeof (err_str) ?
    sprintf ("%O", err_str[err_pos + 1..]) :
    "";
  err_str = sprintf ("%s[0x%x]%s",
		     pre_context, err_str[err_pos], post_context);

  return intro + " " + err_str + " using " + charset +
    (reason ? ": " + reason : ".\n");
}

class DecodeError
//! Error thrown when decode fails (and no replacement char or
//! replacement callback has been registered).
//!
//! @fixme
//! This error class is not actually used by this module yet - decode
//! errors are still thrown as untyped error arrays. At this point it
//! exists only for use by other modules.
{
  inherit Error.Generic;
  constant error_type = "charset_decode";
  constant is_charset_decode_error = 1;

  string err_str;
  //! The string that failed to be decoded.

  int err_pos;
  //! The failing position in @[err_str].

  string charset;
  //! The decoding charset, typically as known to
  //! @[Locale.Charset.decoder].
  //!
  //! @note
  //! Other code may produce errors of this type. In that case this
  //! name is something that @[Locale.Charset.decoder] does not accept
  //! (unless it implements exactly the same charset), and it should
  //! be reasonably certain that @[Locale.Charset.decoder] never
  //! accepts that name in the future (unless it is extended to
  //! implement exactly the same charset).

  protected void create (string err_str, int err_pos, string charset,
		      void|string reason, void|array bt)
  {
    this_program::err_str = err_str;
    this_program::err_pos = err_pos;
    this_program::charset = charset;
    ::create (format_err_msg ("Error decoding",
			      err_str, err_pos, charset, reason),
	      bt);
  }
}

void decode_error (string err_str, int err_pos, string charset,
		   void|string reason, void|mixed... args)
//! Throws a @[DecodeError] exception. See @[DecodeError.create] for
//! details about the arguments. If @[args] is given then the error
//! reason is formatted using @expr{sprintf(@[reason], @@@[args])@}.
{
  if (sizeof (args)) reason = sprintf (reason, @args);
  throw (DecodeError (err_str, err_pos, charset, reason, backtrace()[..<1]));
}

class EncodeError
//! Error thrown when encode fails (and no replacement char or
//! replacement callback has been registered).
//!
//! @fixme
//! This error class is not actually used by this module yet - encode
//! errors are still thrown as untyped error arrays. At this point it
//! exists only for use by other modules.
{
  inherit Error.Generic;
  constant error_type = "charset_encode";
  constant is_charset_encode_error = 1;

  string err_str;
  //! The string that failed to be encoded.

  int err_pos;
  //! The failing position in @[err_str].

  string charset;
  //! The encoding charset, typically as known to
  //! @[Locale.Charset.encoder].
  //!
  //! @note
  //! Other code may produce errors of this type. In that case this
  //! name is something that @[Locale.Charset.encoder] does not accept
  //! (unless it implements exactly the same charset), and it should
  //! be reasonably certain that @[Locale.Charset.encoder] never
  //! accepts that name in the future (unless it is extended to
  //! implement exactly the same charset).

  protected void create (string err_str, int err_pos, string charset,
		      void|string reason, void|array bt)
  {
    this_program::err_str = err_str;
    this_program::err_pos = err_pos;
    this_program::charset = charset;
    ::create (format_err_msg ("Error encoding",
			      err_str, err_pos, charset, reason),
	      bt);
  }
}

void encode_error (string err_str, int err_pos, string charset,
		   void|string reason, void|mixed... args)
//! Throws an @[EncodeError] exception. See @[EncodeError.create] for
//! details about the arguments. If @[args] is given then the error
//! reason is formatted using @expr{sprintf(@[reason], @@@[args])@}.
{
  if (sizeof (args)) reason = sprintf (reason, @args);
  throw (EncodeError (err_str, err_pos, charset, reason, backtrace()[..<1]));
}
