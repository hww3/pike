// Extracts IPTC Information Interchange Model data (aka "IPTC
// headers") from JPEG files created with PhotoShop.
// 
// http://www.iptc.org/IIM/
//
// $Id$
//
// Anders Johansson & Henrik Grubbström

#pike __REAL_VERSION__

mapping(int:mapping(int:string)) fields =
  ([
    1: ([   // ENVELOPE RECORD
      0:   "model version",
      5:   "destinatino",
      20:  "file format",
      22:  "file format version",
      30:  "service identifier",
      40:  "envelope number",
      50:  "product i.d.",
      60:  "envelope, priority",
      70:  "date sent",
      80:  "time sent",
      90:  "coded character set",
      100: "uno",
      120: "arm Identifier",
      122: "arm Version",
    ]),
    2: ([    // APPLICATION RECORD
      0:   "record version",
      3:   "object type reference",
      4:   "object attribut reference",
      5:   "object name",
      7:   "edit status",
      8:   "editorial update",
      10:  "urgency",
      12:  "subject reference",
      15:  "category",
      20:  "supplemental category",
      22:  "fixture identifier",
      25:  "keywords",
      26:  "content location code",
      27:  "content location name",
      30:  "release date",
      35:  "release time",
      37:  "expiration date",
      38:  "expiration time",
      40:  "special instructions",
      42:  "action advised",
      45:  "reference service",
      47:  "reference date",
      50:  "reference number",
      55:  "date created",
      60:  "time created",
      62:  "digital creation date",
      63:  "digital creation time",
      65:  "originating program",
      70:  "program version",
      75:  "object cycle",
      80:  "by-line",
      85:  "by-line title",
      90:  "city",
      92:  "sub-location",
      95:  "province/state",
      100: "country/primary location code",
      101: "country/primary location name",
      103: "original transmission reference",
      105: "headline",
      110: "credit",
      115: "source",
      116: "copyright notice",
      118: "contact",
      120: "caption/abstract",
      122: "writer/editor",
      125: "rasterized caption",
      130: "image type",
      131: "image orientation",
      135: "language identifier",
      150: "audio type",
      151: "audio sampling rate",
      152: "audio sampling resolution",
      153: "audio duration",
      154: "audio outcue",
      200: "objectdata preview file format",
      201: "objectdata preview file format version",
      202: "objectdata preview data",

      // This one seems to contain a charset name...
      //   eg "CP_1252" or "CP_2"
      183: "charset",
    ]),
    3: ([ ]), // DIGITAL NEWSPHOTO PARAMETER
    4: ([ ]), // Not Allocated,
    5: ([ ]), // Not Allocated,
    6: ([ ]), // ABSTRACT RELATIONSHIP
    7: ([     // PRE-OBJECTDATA DESCRIPTOR
      10: "size mode",
      20: "max subfile size",
      90: "objectdata size announced",
      95: "maximum objectdata size",
    ]),
    8: ([     // OBJECTDATA
      10: "subfile",
    ]),
    9: ([     // POST-OBJECTDATA DESCRIPTOR
      10: "confirmed objectdata size",
    ])
  ]);

mapping(int:multiset(int)) binary_fields = ([
  1: (<20, 22>),
  2: (<0>),
]);

protected int short_value(string str)
{
  return (str[0]<<8)|str[1];
  //return (str[1]<<8)|str[0];
}

protected mapping(string:string|array(string)) decode_photoshop_data(string data)
{
  mapping(string:string|array(string)) res = ([]);

  // 0x0404 is IPTC IIM
  array blocks = (data / "8BIM\4\4")[1..];
  if (!sizeof(blocks)) {
    werror("No 8BIM/IPTC IIM markers found in data\n");
    return res;
  }
  //werror("blocks: %O\n", blocks);
  foreach(blocks, string block) {
    //werror("block: %O\n", String.string2hex(block));
    if (sizeof(block) < 6) {
      werror("Malformed 8BIM block\n");
      continue;
    }

    string block_type_2;
    int block_length;
    string info;
    
    if (block[0]) {
      // Photoshop 6.0 format with header description text of variable length.
      // The two bytes after the description text is zero padding, then comes
      // the two bytes of data length.
      int dsclen = block[0];
      //werror("dsclen: %d\n", dsclen);
      block_type_2 = block[1..dsclen];
      block_length = short_value(block[dsclen+3..dsclen+4]);
      info = block[5+dsclen..4+dsclen+block_length];
    }
    else {
      block_type_2 = block[..3];
      //werror("block_type_2: %O\n", block_type_2);
      block_length = short_value(block[4..5]);
      info = block[6..5 + block_length];
    }

#if 0
    werror("block_length: %O\n"
	   "actual length: %O\n"
	   "info: %O\n", block_length, sizeof(info), info);
#endif /* 0 */

    while (sizeof(info)) {
      if (sizeof(info) < 6) {
	//werror("Short info %O\n", info);
	break;
      }
      int segment_marker = info[0];
      int record_set = info[1];
      int id = info[2];
      int size = short_value(info[3..4]);
      string data = info[5..size+5-1];
      info = info[size+5..];

      if (segment_marker != '\x1c') {
	if (segment_marker == '\x6f') {
	  // I have not found any documentation for this segment,
	  // but I use it to detect Nyhedstjeneste.
	  if ((record_set == 110) && (!id)) {
	    res->charset = ({ "iso-8859-1" });
	    continue;
	  }
	}
#if 1
	werror("Unknown segment marker: 0x%02x\n"
	       "record_set: %d\n"
	       "id: %d\n"
	       "data: %O\n", segment_marker, record_set, id, data);
#endif /* 1 */
	break;
      }

      if (!has_value(indices(fields), record_set)) {
	werror("Unknown record set marker: %O\n", record_set);
	break;
      }

      //werror("%3d: ", id);
      //werror("%O\n", data);
      //werror("info: %O\n", String.string2hex(info));
      string label =
	fields[record_set][id] ||
	(string)record_set + ":" + (string)id;

      if (label == "coded character set") {
	if (data == "\e%G") {
	  res->charset = (res->charset || ({})) + ({ "UTF-8" });
	} else if (data == "\e%5") {
	  res->charset = (res->charset || ({})) + ({ "iso-8859-1" });
	}	
      }

      if (label == "special instructions" && lower_case(data) == "nyhedstjeneste")
	res->charset = (res->charset || ({})) + ({ "iso-8859-1" });

      if ((binary_fields[record_set] && binary_fields[record_set][id]) ||
	  (<3, 7>)[record_set]) {
	// Decode binary fields.
	data = (string)Gmp.mpz(data, 256);
      }

      // werror("RAW: %O:%O\n", label, data);

      if (res[label])
	res[label] += ({ data });
      else
	res[label] = ({ data });
    }
  }
  return res;
}

mapping get_information(Stdio.File fd)
{
  string marker = fd->read(2);
  string photoshop_data = "";

  if (marker == "\xc5\xd0") {
    // Probably a DOS EPS Binary Header.
    string tmp = fd->read(28);
    if (!has_prefix(tmp, "\xd3\xc6")) return ([]);
    int offset;
    sscanf(tmp, "%*2c%-4c", offset);
    offset -= 30;
    if (offset < 0) return ([]);
    if (offset > 0) fd->read(offset);
    marker = fd->read(2);	// Should be a PS header.
  }
  if (marker == "%!") {
    int bytes = -1;
    // Note: We use the split iterator by hand to make sure '\r' is
    //       valid as a line terminator.
    foreach(String.SplitIterator(marker, (<'\r','\n'>), 1,
				 fd->read_function(8192));
	    int lineno; string line) {
      if (line[0] != '%') continue;
      if (bytes < 0) sscanf(line, "%%BeginPhotoshop:%*[ ]%d", bytes);
      else if (has_prefix(line, "%EndPhotoshop")) {
	break;
      } else if (has_prefix(line, "% ")) {
#if constant(String.hex2string)
	photoshop_data += String.hex2string(line[2..]);
#else
	photoshop_data += Crypto.hex_to_string(line[2..]);
#endif
	if (sizeof(photoshop_data) >= bytes) break;
      } else {
#if constant(String.hex2string)
	photoshop_data += String.hex2string(line[1..]);
#else
	photoshop_data += Crypto.hex_to_string(line[1..]);
#endif
	if (sizeof(photoshop_data) >= bytes) break;
      }
    }
  } else if (marker == "\xff\xd8") {
    do {
      string app = fd->read(2);
      if (sizeof(app) != 2)
      break;
      string length_s = fd->read(2);
      int length;
      if (sizeof(length_s) == 2)
	length = short_value(length_s);
      else
	break;
      //werror ("length: %O\n", short_value(length_s));

      string data = fd->read((length-2) & 0xffff);
      if (app == "\xff\xed") // APP14 Photoshop
      {
	//werror("data: %O\n", data);
	photoshop_data = data;
	break;
      }
    } while (1);
  } else {
    //werror("unknown marker: %O neither JPEG nor Postscript\n", marker);
    return ([]);
  }

  if (!sizeof(photoshop_data)) return ([]);

  mapping res = decode_photoshop_data(photoshop_data);

  if (sizeof(res)) {
    // IIMV 4.1 Chapter 3 Section 1.6 (a):
    //   Record 1:xx shall use coded character set ISO 646 International
    //   Reference Version or ISO 4873 Default Version.
    //
    // IIMV 4.1 Chapter 5 1:90:
    //   The control functions apply to character oriented DataSets in
    //   records 2-6. They also apply to record 8, unless the objectdata
    //   explicitly, or the File Format implicitly, defines character sets
    //   otherwise.
    //   [...]
    //   If 1:90 is omitted, the default for records 2-6 and 8 is ISO 646
    //   IRV (7 bits) or ISO 4873 DV (8 bits). Record 1 shall always use
    //   ISO 646 IRV or ISO 4873 DV respectively.
    //
    // In practice the above of course isn't true, and it seems
    // that macintosh encoding is used in place of ISO 4873 DV.
    //
    // 1: "iso646irv" or "iso4873dv",
    //
    // Most application record fields seem to be encoded
    // with the macintosh charset.
    //
    // This has been verified for the fields:
    //   "by-line"
    //   "caption/abstract"
    //   "city"
    //   "copyright notice"
    //   "headline"
    //   "keywords"
    //   "object name"
    //   "source"
    //   "special instructions"
    //   "supplemental category"
    //   "writer/editor"
    // and is assumed for the remainder.
    //
    // Some do however (eg Nyhedstjeneste in Denmark) use ISO-8859-1.
    //
    // We attempt some DWIM...

    string charset;
    if (!res->charset) {
      charset = "macintosh";
    } else {
      charset = lower_case(res->charset[0]);

      // Remap to standard names:
      charset = ([
	"cp_1252":"windows1252",
	"cp_2":"macintosh",
      ])[charset] || charset;
    }
    //werror("Charset: %O\n", charset);
    res->charset = ({ charset });
    object decoder = Locale.Charset.decoder(charset);
    foreach(res; string key; array(string) vals) {
      res[key] = map(vals,
		     lambda(string val, object decoder) {
		       return decoder->feed(val)->drain();
		     }, decoder);
    }
  }

  return res;
}
