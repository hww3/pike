// LDAP client protocol implementation for Pike.
//
// $Id: ldap_privates.pmod,v 1.3 1999/08/25 05:04:32 hubbe Exp $
//
// Honza Petrous, hop@unibase.cz
//
// ----------------------------------------------------------------------
//
// ToDo List:
//
//	- v2 operations: 
//		modify
//
// History:
//
//	v2.0  1999-02-19 Create separate file. Implementation the following
//			 classes:
//			  - asn1_enumerated
//			  - asn1_boolean
//			  - asn1_application_sequence
//			  - asn1_application_octet_string
//			  - asn1_context_integer
//			  - asn1_context_octet_string
//			  - asn1_context_sequence
//			  - asn1_context_set
//			  - ldap_der_decode
//



// --------------- Standards.ASN1.Types private add-on --------------------
// This is very poor defined own ASN.1 objects (not enough time to clean it!)
//import Standards.ASN1.Encode;

#if constant(Standards.ASN1.Types.asn1_integer)

class asn1_enumerated
{
  inherit Standards.ASN1.Types.asn1_integer;
  constant tag = 10;
  constant type_name = "ENUMERATED";
}

class asn1_boolean
{
  inherit Standards.ASN1.Types.asn1_object;
  constant tag = 1;
  constant type_name = "BOOLEAN";
  int value;

  object init(int n) {
    if(n)
      n=0xff;
    value = n;
    return (this_object());
  }

  string der_encode() { return build_der(value? "\377" : "\0"); }

  object decode_primitive(string contents) {
    record_der(contents);
    value = ( contents != "\0" );
    return this_object();
  }

  string debug_string() {
    return value ? "TRUE" : "FALSE";
  }

}

class asn1_application_sequence
{
  inherit Standards.ASN1.Types.asn1_sequence;
  constant cls = 1;
  constant type_name = "APPLICATION SEQUENCE";
  int tagx;

  int get_tag() { return tagx; }

  void init(int tagid, array args) {
    ::init(args);
    tagx = tagid;
  }

}

class asn1_application_octet_string
{
  inherit Standards.ASN1.Types.asn1_octet_string;
  constant cls = 1;
  constant type_name = "APPLICATION OCTET_STRING";
  int tagx;

  int get_tag() { return tagx; }

  void init(int tagid, string arg) {
    ::value = arg;
    tagx = tagid;
  }

}

class asn1_context_integer
{
  inherit Standards.ASN1.Types.asn1_integer;
  constant cls = 2;
  constant type_name = "CONTEXT INTEGER";
  int tagx;

  int get_tag() { return tagx; }

  void init(int tagid, int arg) {
    ::init(arg);
    tagx = tagid;
  }
}

class asn1_context_octet_string
{
  inherit Standards.ASN1.Types.asn1_octet_string;
  constant cls = 2;
  constant type_name = "CONTEXT OCTET_STRING";
  int tagx;

  int get_tag() { return tagx; }

  void init(int tagid, string arg) {
    ::init(arg);
    tagx = tagid;
  }
}


class asn1_context_sequence
{
  inherit Standards.ASN1.Types.asn1_sequence;
  constant cls = 2;
  constant type_name = "CONTEXT SEQUENCE";
  int tagx;

  int get_tag() { return tagx; }

  void init(int tagid, array arg) {
    ::init(arg);
    tagx = tagid;
  }

}

class asn1_context_set
{
  inherit Standards.ASN1.Types.asn1_set;
  constant cls = 2;
  constant type_name = "CONTEXT SET";
  int tagx;

  int get_tag() { return tagx; }

  void init(int tagid, array arg) {
    ::init(arg);
    tagx = tagid;
  }

}

#if 1
object|mapping der_decode(object data, mapping types)
{
  int raw_tag = data->get_uint(1);
  int len;
  string contents;

  if ( (raw_tag & 0x1f) == 0x1f)
    error("ASN1.Decode: High tag numbers is not supported\n");

  int len = data->get_uint(1);
  if (len & 0x80)
    len = data->get_uint(len & 0x7f);

  contents = data->get_fix_string(len);

  int tag = raw_tag & 0xdf; // Class and tag bits

  program p = types[tag];

  if (raw_tag & 0x20)
  {
    /* Constructed encoding */

    array elements = ({ });
    object struct = ADT.struct(contents);

    if (!p)
    {
      while (!struct->is_empty())
        elements += ({ der_decode(struct, types) });

      return Standards.ASN1.Decode.constructed(tag, contents, elements);
    }

    object res = p();

    // hop: For non-universal classes we provide tag number
    if(tag & 0xC0)
      res = p(tag & 0x1F, ({}));

    res->begin_decode_constructed(contents);

    int i;

    /* Ask object which types it expects for field i, decode it, and
     * record the decoded object */
    for(i = 0; !struct->is_empty(); i++)
    {
      res->decode_constructed_element
        (i, der_decode(struct,
                       res->element_types(i, types)));
    }
    return res->end_decode_constructed(i);
  }
  else
  {
    /* Primitive encoding */
    return p ? p()->decode_primitive(contents)
      : Standards.ASN1.Decode.primitive(tag, contents);
  }
}
#endif

static mapping ldap_type_proc =
                    ([ 1 : asn1_boolean,
                       2 : Standards.ASN1.Types.asn1_integer,
                       3 : Standards.ASN1.Types.asn1_bit_string,
                       4 : Standards.ASN1.Types.asn1_octet_string,
                       5 : Standards.ASN1.Types.asn1_null,
                       6 : Standards.ASN1.Types.asn1_identifier,
                       // 9 : asn1_real,
                       10 : asn1_enumerated,
                       16 : Standards.ASN1.Types.asn1_sequence,
                       17 : Standards.ASN1.Types.asn1_set,
                       19 : Standards.ASN1.Types.asn1_printable_string,
                       20 : Standards.ASN1.Types.asn1_teletex_string,
                       23 : Standards.ASN1.Types.asn1_utc,
                       65 : asn1_application_sequence,
                       68 : asn1_application_sequence,
                       69 : asn1_application_sequence,
                       71 : asn1_application_sequence,
                       73 : asn1_application_sequence,
                       75 : asn1_application_sequence,
                       77 : asn1_application_sequence,
                       79 : asn1_application_sequence
                    ]);

object|mapping ldap_der_decode(string data)
{
  return der_decode(ADT.struct(data), ldap_type_proc);
}

#endif

// ------------- end of ASN.1 API hack -----------------------------

