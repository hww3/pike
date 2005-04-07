#pike __REAL_VERSION__

// LDAP client protocol implementation for Pike.
//
// $Id: client.pike,v 1.95 2005/04/07 18:36:15 mast Exp $
//
// Honza Petrous, hop@unibase.cz
//
// ----------------------------------------------------------------------
//
// History:
//
//	v0.0  1998-05-25 Starting up!
//	v1.0  1998-06-21 Core functions (open, bind, unbind, delete, add,
//			 compare, search), only V2 operations,
//			 some additional restrictions
//	v1.1u 1998-07-03 Unfinished version (for testing)
//	v1.2a 1998-07-14 New beta code (I love jump over versions like Roxen ;)
//			 - added asn1_enumerated, asn1_boolean, asn1_nulllist
//			   and asn1_application_null
//			 - search op: corrected ASN1 type
//			 - unbind op: corrected ASN1 type
//	v1.2d 1998-08    - added logging of bad LDAP PDU in readmsg
//	v1.3  1998-09-18 - resolved "null attribute list" bug  in search
//			   (Now is compatible with MS Exchange ;-)
//	v1.9  1999-03	 - redesign of LDAP.pmod
//			 !!! Library uses new ASN1 API => uncompatible !!!
//			 !!! with Pike 0.5 (needed Pike 0.6 or higher) !!!
//			 - sliced library code to the several files:
//			   * LDAP.pmod/protocol.pike
//			     (low level code)
//			   * LDAP.pmod/client.pike
//			     (main code)
//			   * LDAP.pmod/ldap_errors.h
//			     (error array)
//			   * LDAP.pmod/ldap_globals.h
//			     (global defininitions)
//			   * LDAP.pmod/ldap_privates.pmod
//			     (ASN.1 LDAP-related classes and hacks)
//			 - changed default of 'ldap_scope' to 0
//			 - search filter now correctly processed '\(' & '\)'
//			   [! Still unimplemented escaped conditions chars!]
//
//	v1.10 1999-03-28 - moved core to the new 'protocol' code
//	      1999-03-28 - rewritten ldap_[op] startup code
//
//	v1.11 1999-04-10 - search filter now processed multiple wild '*' chars
//			   [ Escaping untested, yet ]
//	v1.13 2000-02-12 - fixed search NOT op bug (end revision normalized)
//
//	v1.14 2000-02-17 - added decoding of UTF8 strings for v3 protocol
//
//	newer versions - see CVS at roxen.com (hop)
//
//			 - corrected deUTF8 values in result
//			 -
//
// Specifications:
//
//	RFC 1558			  (search filter representations)
//	RFC 1777,1778,1779		  (version2 spec)
//	RFC 1823			  (v2 API)
//	RFC 2251,2252,2253,2254,2255,2256 (version3 spec)
//	draft-ietf-asid-ldap-c-api-00.txt (v3 API)
//	RFC 2279   			  (UTF-8)
//	RFC 2696			  (paged requests)
//
//	Interesting, applicable
//	RFC 2307   (LDAP as network information services; draft?)


#if constant(.ldap_privates)

#include "ldap_globals.h"

#if constant(SSL.Cipher.CipherAlgorithm)
import SSL.Constants;
#endif

import ".";

// ------------------------

// ASN.1 decode macros

#define ASN1_GET_RESULTAPP(X)           ((X)->elements[1]->get_tag())
#define ASN1_GET_DN(X)			((X)->elements[0]->value)
#define ASN1_GET_ATTR_ARRAY(X)		(sizeof ((X)->elements) > 1 &&	\
					 (array) ((X)->elements[1]->elements))
#define ASN1_GET_ATTR_NAME(X)		((X)->elements[0]->value)
#define ASN1_GET_ATTR_VALUES(X)         ((X)->elements[1]->elements->value)

#define ASN1_DECODE_RESULTAPP(X)	ASN1_GET_RESULTAPP (.ldap_privates.ldap_der_decode(X))
#define ASN1_DECODE_RESULTCODE(X)	(int)(.ldap_privates.ldap_der_decode(X)->elements[1]->elements[0]->value->cast_to_int())
#define ASN1_DECODE_RESULTSTRING(X)	(.ldap_privates.ldap_der_decode(X)->elements[1]->elements[2]->value)
#define ASN1_DECODE_RESULTREFS(X)	(.ldap_privates.ldap_der_decode(X)->elements[1]->elements[3]->elements)
#define ASN1_DECODE_RAWDEBUG(X)		(.ldap_privates.ldap_der_decode(X)->debug_string())

 //! Contains the client implementation of the LDAP protocol.
 //! All of the version 2 protocol features are implemented
 //! but only the base parts of the version 3. 

  inherit .protocol;

  private {
    string bound_dn;		// When actually bound, set to the bind DN.
    string ldap_basedn;		// baseDN
    int ldap_scope;		// SCOPE_*
    int ldap_deref;		// 0: ...
    int ldap_sizelimit;
    int ldap_timelimit;
    mapping lauth = ([]);
    object default_filter_obj;	// Filter object parsed from lauth->filter.
    result last_rv;		// last returned value
  }

//! @ignore
static function(string:string) get_attr_decoder (string attr,
						 DO_IF_DEBUG (void|int nowarn))
{
  if (mapping(string:mixed) attr_descr = get_attr_type_descr (attr)) {
    if (function(string:string) decoder =
	syntax_decode_fns[attr_descr->syntax_oid])
      return decoder;
#ifdef DEBUG
    else if (!get_constant_name (attr_descr->syntax_oid))
      werror ("Warning: Unknown syntax %O for attribute %O - "
	      "binary content assumed.\n", attr_descr->syntax_oid, attr);
#endif
  }
#ifdef DEBUG
  else if (!nowarn && !has_suffix (attr, ";binary") && !has_value (attr, ";binary;"))
    werror ("Warning: Couldn't fetch attribute description for %O - "
	    "binary content assumed.\n%s", attr, describe_backtrace(backtrace()));
#endif
  return 0;
}
//! @endignore

static function(string:string) get_attr_encoder (string attr)
{
  if (mapping(string:mixed) attr_descr = get_attr_type_descr (attr)) {
    if (function(string:string) encoder =
	syntax_encode_fns[attr_descr->syntax_oid])
      return encoder;
#ifdef DEBUG
    else if (!get_constant_name (attr_descr->syntax_oid))
      werror ("Warning: Unknown syntax %O for attribute %O - "
	      "binary content assumed.\n", attr_descr->syntax_oid, attr);
#endif
  }
#ifdef DEBUG
  else if (!has_suffix (attr, ";binary") && !has_value (attr, ";binary;"))
    werror ("Warning: Couldn't fetch attribute description for %O - "
	    "binary content assumed.\n", attr);
#endif
  return 0;
}

  //! Contains the result of a LDAP search.
  //!
  //! @seealso
  //!  @[LDAP.client.search], @[LDAP.client.result.fetch]
  //!
  class result // ------------------
  {

    private int resultcode = LDAP_SUCCESS;
    private string resultstring;
    private int actnum = 0;
    private array(mapping(string:string|array(string))) entry = ({});
    private int flags;
    array(string) referrals;

    static array decode_entries (array(string) rawres)
    {
      array(mapping(string:string|array(string))) res = ({});

#define DECODE_ENTRIES(SET_DN, SET_ATTR) do {				\
	if (flags & SEARCH_MULTIVAL_ARRAYS_ONLY) {			\
	  foreach (rawres, string rawent) {				\
	    object derent = .ldap_privates.ldap_der_decode (rawent)->elements[1]; \
	    if (array(object) derattribs = ASN1_GET_ATTR_ARRAY (derent)) { \
	      string dn = (SET_DN);					\
	      mapping(string:string|array) attrs = (["dn": dn]);	\
	      foreach (derattribs, object derattr) {			\
		string attr;						\
		{SET_ATTR;}						\
		sscanf (attr, "%[^;]", string bare_attr);		\
		if (mapping(string:mixed) attr_descr =			\
		    get_attr_type_descr (bare_attr)) {			\
		  if (attr_descr["SINGLE-VALUE"]) {			\
		    if (sizeof (attrs[attr])) {				\
		      DO_IF_DEBUG (					\
			if (sizeof (attrs[attr]) > 1)			\
			  ERROR ("Got multple values %O for single valued " \
				 "attribute %O.\n", attrs[attr], attr);	\
		      );						\
		      attrs[attr] = attrs[attr][0];			\
		    }							\
		    else						\
		      attrs[attr] = 0;					\
		  }							\
		}							\
		DO_IF_DEBUG (						\
		  else if (dn != "")					\
		    werror ("Warning: Couldn't fetch attribute description for %O - " \
			    "multivalued attribute assumed.\n", attr);	\
		);							\
	      }								\
	      res += ({attrs});						\
	    }								\
	  }								\
	}								\
									\
	else {								\
	  foreach (rawres, string rawent) {				\
	    object derent = .ldap_privates.ldap_der_decode (rawent)->elements[1]; \
	    if (array(object) derattribs = ASN1_GET_ATTR_ARRAY (derent)) { \
	      string dn = (SET_DN);					\
	      mapping(string:array) attrs = (["dn": ({dn})]);		\
	      foreach (derattribs, object derattr) {			\
		string attr;						\
		{SET_ATTR;}						\
	      }								\
	      res += ({attrs});						\
	    }								\
	  }								\
	}								\
      } while (0)

      if (ldap_version < 3) {
	// Use the values raw.
	if (flags & SEARCH_LOWER_ATTRS)
	  DECODE_ENTRIES (ASN1_GET_DN (derent), {
	      attrs[attr = lower_case (ASN1_GET_ATTR_NAME (derattr))] =
		ASN1_GET_ATTR_VALUES (derattr);
	    });
	else
	  DECODE_ENTRIES (ASN1_GET_DN (derent), {
	      attrs[attr = ASN1_GET_ATTR_NAME (derattr)] =
		ASN1_GET_ATTR_VALUES (derattr);
	    });
      }

      else {
	// LDAPv3: Decode values as appropriate according to the
	// schema. Note that attributes with the ";binary" option
	// won't be matched by get_attr_type_descr and are therefore
	// left untouched.
	if (flags & SEARCH_LOWER_ATTRS)
	  DECODE_ENTRIES (utf8_to_string (ASN1_GET_DN (derent)), {
	      attr = lower_case (ASN1_GET_ATTR_NAME (derattr));
	      if (function(string:string) decoder =
		  // Microsoft AD has several attributes in its root DSE
		  // that they have not bothered to include in their
		  // schema. So if this is the root being fetched then
		  // send the nowarn flag to get_attr_encoder to avoid
		  // complaints about that.
		  get_attr_decoder (attr, DO_IF_DEBUG (dn == "")))
		attrs[attr] = map (ASN1_GET_ATTR_VALUES (derattr), decoder);
	      else
		attrs[attr] = ASN1_GET_ATTR_VALUES (derattr);
	    });
	else
	  DECODE_ENTRIES (utf8_to_string (ASN1_GET_DN (derent)), {
	      attr = ASN1_GET_ATTR_NAME (derattr);
	      if (function(string:string) decoder =
		  get_attr_decoder (attr, DO_IF_DEBUG (dn == "")))
		attrs[attr] = map (ASN1_GET_ATTR_VALUES (derattr), decoder);
	      else
		attrs[attr] = ASN1_GET_ATTR_VALUES (derattr);
	    });
      }

#undef DECODE_ENTRIES

      return res;
    }

    //!
    //! You can't create instances of this object yourself.
    //! The only way to create it is via a search of a LDAP server.
    object|int create(array rawres, void|int stuff, void|int flags) {
    // rawres: array of result in raw format, but WITHOUT LDAP PDU !!!
    // stuff: 1=bind result; ...

      this_program::flags = flags;

      // Note: Might do additional schema queries to the server while
      // decoding the result. That means possible interleaving problem
      // if search() is extended to not retrieve the complete reply at
      // once.

      int lastel = sizeof(rawres) - 1;

      if (lastel < 0) {
	seterr (LDAP_LOCAL_ERROR);
        THROW(({"LDAP: Internal error.\n",backtrace()}));
	return -ldap_errno;
      }
      DWRITE(sprintf("result.create: rawres=%O\n",rawres[lastel]));

      // The last element of 'rawres' is result itself
      resultcode = ASN1_DECODE_RESULTCODE(rawres[lastel]);
      DWRITE(sprintf("result.create: code=%d\n",resultcode));
      resultstring = ASN1_DECODE_RESULTSTRING(rawres[lastel]);
      if (resultstring == "")
	resultstring = 0;
      else if (ldap_version >= 3)
	resultstring = utf8_to_string (resultstring);
      DWRITE(sprintf("result.create: str=%s\n",resultstring));
#ifdef V3_REFERRALS
      // referral (v3 mode)
      if(resultcode == 10) {
        referrals = ({});
        foreach(ASN1_DECODE_RESULTREFS(rawres[lastel]), object ref1)
	  referrals += ({ ref1->value });
        DWRITE(sprintf("result.create: refs=%O\n",referrals));
      }
#endif
      DWRITE(sprintf("result.create: elements=%d\n",lastel+1));
#if 0
      DWRITE(sprintf("result.create: entries=%O\n",rawres[..lastel-1]));
#endif

      entry = decode_entries (rawres[..lastel-1]);

#if 0
      // Context specific proccessing of 'rawres'
      switch(stuff) {
        case 1:	DWRITE("result.create: stuff=1\n");
		break;
        default:	DWRITE(sprintf("result.create: stuff=%d\n", stuff));

      }
#endif

      return this;

    }

    //!
    //! Returns the error number in the search result.
    //!
    //! @seealso
    //!  @[error_string], @[server_error_string]
    int error_number() { return resultcode; }

    //!
    //! Returns the description of the error in the search result.
    //! This is the error string from the server, or a standard error
    //! message corresponding to the error number if the server didn't
    //! provide any description.
    //!
    //! @seealso
    //!  @[server_error_string], @[error_number]
    string error_string() {
      return resultstring || ldap_error_strings[resultcode];
    }

    //! Returns the error string from the server, or zero if the
    //! server didn't provide any.
    //!
    //! @seealso
    //!  @[error_string], @[error_number]
    string server_error_string() {return resultstring;}

    //!
    //! Returns the number of entries.
    //!
    //! @seealso
    //!  @[LDAP.client.result.count_entries]
    int num_entries() { return sizeof (entry); }

    //!
    //! Returns the number of entries from current cursor
    //! possition till end of the list.
    //!
    //! @seealso
    //!  @[LDAP.client.result.first], @[LDAP.client.result.next]
    int count_entries() { return sizeof (entry) - actnum; }

    //! Returns the current entry pointed to by the cursor.
    //! a mapping with an entry for each attribute. Each value
    //! is an array of the values for the attribute. The
    //!
    //! @param index
    //!  Optional argument can be used for direct access
    //!  to the entry other then currently pointed by cursor.
    //!
    //! @returns
    //!  The return value is a mapping describing the entry:
    //!
    //!  @mapping
    //!  @member string attribute
    //!    An attribute in the entry. The value is an array containing
    //!    the returned attribute value(s) on string form.
    //!
    //!  @member string "dn"
    //!    This special entry contains the object name of the entry as
    //!    a distinguished name.
    //!  @endmapping
    //!
    //!  Zero is returned if the cursor is outside the valid range of
    //!  entries.
    //!
    //! @note
    //!  It's undefined whether or not destructive operations on the
    //!  returned mapping will affect future @[fetch] calls for the
    //!  same entry.
    //!
    //! @note
    //!  In Pike 7.6 and earlier, the special @expr{"dn"@} entry was
    //!  incorrectly returned in UTF-8 encoded form for LDAPv3
    //!  connections.
    //!
    //! @seealso
    //!   @[fetch_all]
    int|mapping(string:string|array(string)) fetch(int|void idx) {

      if (!idx)
	idx = actnum + 1;
      if ((idx <= num_entries()) && (idx > 0)) {
	actnum = idx - 1;
        return entry[actnum];
      }
      return 0;
    }

    //!
    //! Returns distinguished name (DN) of the current entry in the
    //! result list. Note that this is the same as getting the
    //! @expr{"dn"@} field from the return value from @[fetch].
    //!
    //! @note
    //!  In Pike 7.6 and earlier, this field was incorrectly returned
    //!  in UTF-8 encoded form for LDAPv3 connections.
    string get_dn()
    {
      string|array(string) dn = fetch()->dn;
      return stringp (dn) ? dn : dn[0];	// To cope with SEARCH_MULTIVAL_ARRAYS_ONLY.
    }

    //!
    //! Initialized the result cursor to the first entry
    //! in the result list.
    //!
    //! @seealso
    //!  @[LDAP.client.result.next]
    void first() { actnum = 0; }

    //!
    //! Moves the result cursor to the next entry
    //! in the result list. Returns number of remained entries
    //! in the result list. Returns 0 at the end.
    //!
    //! @seealso
    //!  @[LDAP.client.result.next]
    int next() {
      if (actnum < sizeof (entry))
	actnum++;
      return count_entries();
    }

    array(mapping(string:string|array(string))) fetch_all()
    //! Convenience function to fetch all entries at once. The cursor
    //! isn't affected.
    //!
    //! @returns
    //!   Returns an array where each element is the entry from the
    //!   result. Don't be destructive on the returned value.
    //!
    //! @seealso
    //!   @[fetch]
    {
      return entry;
    }

  } // end of class 'result' ---------------

  // helper functions and macros

#ifdef ENABLE_PAGED_SEARCH
#define IF_ELSE_PAGED_SEARCH(X,Y)	X
#else /* !ENABLE_PAGED_SEARCH */
#define IF_ELSE_PAGED_SEARCH(X,Y)	Y
#endif

#ifdef LDAP_PROTOCOL_PROFILE
#define PROFILE(STR, CODE...) DWRITE_PROF(STR + ": %O\n", gauge {CODE;})
#else
#define PROFILE(STR, CODE...) do { CODE; } while(0)
#endif

  private int chk_ver() {

    if ((ldap_version != 2) && (ldap_version != 3)) {
      seterr (LDAP_PROTOCOL_ERROR);
      THROW(({"LDAP: Unknown/unsupported protocol version.\n",backtrace()}));
      return -ldap_errno;
    }
    return 0;
  }

  private int chk_binded() {
  // For version 2: we must be 'binded' first !!!

    switch (ldap_version) {
      case 2:
	if (!bound_dn) {
	  seterr (LDAP_PROTOCOL_ERROR);
	  THROW(({"LDAP: Must bind first.\n",backtrace()}));
	  return -ldap_errno;
	}
	break;
      case 3:
	if (!bound_dn)
	  bind();
	break;
    }
    return 0;
  }

  private int chk_dn(string dn) {

    if ((!dn) || (!sizeof(dn))) {
      seterr (LDAP_INVALID_DN_SYNTAX);
      THROW(({"LDAP: Invalid DN syntax.\n",backtrace()}));
      return -ldap_errno;
    }
    return 0;
  }

  //! Several information about code itself and about active connection too
  mapping info;

#ifndef PARSE_RFCS
  //! @decl void create()
  //! @decl void create(string url)
  //! @decl void create(string url, object context)
  //!
  //! Create object. The first optional argument can be used later
  //! for subsequence operations. The second one can specify
  //! TLS context of connection. The default context only allows
  //! 128-bit encryption methods, so you may need to provide your
  //! own context if your LDAP server supports only export encryption.
  //!
  //! @param url
  //!  LDAP server URL on the form
  //!  @expr{"ldap://hostname/basedn?attrlist?scope?ext"@}. See RFC
  //!  2255. It can also be a mapping as returned by
  //!  @[Protocol.LDAP.parse_ldap_url].
  //!
  //! @param context
  //!  TLS context of connection
  //!
  //! @seealso
  //!  @[LDAP.client.bind], @[LDAP.client.search]
  void create(string|mapping(string:mixed)|void url, object|void context)
  {

    info = ([ "code_revision" : ("$Revision: 1.95 $"/" ")[1] ]);

    if(!url || !sizeof(url))
      url = LDAP_DEFAULT_URL;

    if (mappingp (url))
      lauth = url;
    else
      lauth = parse_ldap_url(url);

    if(!stringp(lauth->scheme) ||
       ((lauth->scheme != "ldap")
#if constant(SSL.Cipher.CipherAlgorithm)
	&& (lauth->scheme != "ldaps")
#endif
	)) {
      THROW(({"Unknown scheme in server URL.\n",backtrace()}));
    }

    if(!lauth->host)
      lauth += ([ "host" : LDAP_DEFAULT_HOST ]);
    if(!lauth->port)
      lauth += ([ "port" : lauth->scheme == "ldap" ? LDAP_DEFAULT_PORT : LDAPS_DEFAULT_PORT ]);

#if constant(SSL.Cipher.CipherAlgorithm)
    if(lauth->scheme == "ldaps" && !context) {
      context = SSL.context();
      // Allow only strong crypto
      context->preferred_suites = ({
	SSL_rsa_with_idea_cbc_sha,
	SSL_rsa_with_rc4_128_sha,
	SSL_rsa_with_rc4_128_md5,
	SSL_rsa_with_3des_ede_cbc_sha,
      });
    }
#endif
    if(!(low_fd->connect(lauth->host, lauth->port))) {
      //errno = ldapfd->errno();
      seterr (LDAP_SERVER_DOWN);
      DWRITE("client.create: ERROR: can't open socket.\n");
      //ldapfd->destroy();
      //ldap=0;
      //ok = 0;
      //if(con_fail)
      //  con_fail(this, @extra_args);
      THROW(({"Failed to connect to LDAP server.\n",backtrace()}));
    }

#if constant(SSL.Cipher.CipherAlgorithm)
    if(lauth->scheme == "ldaps") {
      context->random = Crypto.Random.random_string;
      ::create(SSL.sslfile(low_fd, context, 1,1));
      info->tls_version = ldapfd->version;
    } else
      ::create(low_fd);
#else
    if(lauth->scheme == "ldaps") {
	THROW(({"LDAP: LDAPS is not available without SSL support.\n",backtrace()}));
    }
    else
      ::create(low_fd);
#endif

    DWRITE("client.create: connected!\n");

    DWRITE(sprintf("client.create: remote = %s\n", low_fd->query_address()));
    DWRITE_HI("client.OPEN: " + lauth->host + ":" + (string)(lauth->port) + " - OK\n");

    reset_options();
  } // create
#endif

void reset_options()
//! Resets all connection options, such as the scope and the base DN,
//! to the defaults determined from the LDAP URL when the connection
//! was created.
{
  set_scope (lauth->scope || SCOPE_BASE);
  set_basedn (lauth->basedn);
  ldap_deref = 0;
  ldap_sizelimit = 0;
  ldap_timelimit = 0;
  last_rv = 0;
}

  private mixed send_bind_op(string name, string password) {
  // Simple BIND operation

    object msgval, vers, namedn, auth, app;
    string pass = password;
    password = "censored";

    vers = Standards.ASN1.Types.asn1_integer(ldap_version);
    namedn = Standards.ASN1.Types.asn1_octet_string(name);
    auth = ASN1_CONTEXT_OCTET_STRING(0, pass);
    // SASL credentials ommited

    msgval = ASN1_APPLICATION_SEQUENCE(0, ({vers, namedn, auth}));

    return do_op(msgval);
  }

  private mixed send_starttls_op(object|void context) {

    object msgval;
#if constant(SSL.Cipher.CipherAlgorithm)

    // can we do this now?
    if(ldapfd->context) 
    {
      THROW(({"LDAP: TLS/SSL already established.\n",backtrace()}));
    }

    // NOTE: should we be on the lookout for requests in flight?

    

    msgval = ASN1_APPLICATION_SEQUENCE(23, ({Standards.ASN1.Types.OctetString("1.3.6.1.4.1.1466.20037")}));

    do_op(msgval);
    int result = ASN1_DECODE_RESULTCODE(readbuf);
    if(result!=0) return 0;
    // otherwise, we can try to negotiate.
      if(!context)
      {
        context = SSL.context();
        // Allow only strong crypto
        context->preferred_suites = ({
  	  SSL_rsa_with_idea_cbc_sha,
 	  SSL_rsa_with_rc4_128_sha,
	  SSL_rsa_with_rc4_128_md5,
	  SSL_rsa_with_3des_ede_cbc_sha,
        });
      }
    object _f = ldapfd;
    ldapfd=SSL.sslfile(_f, context, 1, 1);
    return 1;
#endif
  return 0;    
  }

  //!  Requests that a SSL/TLS session be negotiated on the connection.
  //!  If the connection is already secure, this call will fail.
  //!
  //!  @param context
  //!    an optional SSL.context object to provide to the 
  //!    SSL/TLS connection client.
  //!  
  //!  Returns @expr{1@} on success, @expr{0@} otherwise.
  //!
  int start_tls (void|SSL.context context) {

    int id;
    mixed raw;
    if(ldap_version < 3)
    {
      seterr (LDAP_PROTOCOL_ERROR);
      THROW(({"LDAP: Unknown/unsupported protocol version.\n",backtrace()}));
      return -ldap_errno;
    }

    return send_starttls_op(context||UNDEFINED);

    return 1;
  } // start_tls

   //! @decl int bind()
  //! @decl int bind(string dn, string password)
  //! @decl int bind(string dn, string password, int version)
  //!
  //! Authenticates connection to the direcory.
  //!
  //! First form uses default value previously entered in create.
  //!
  //! Second form uses value from parameters:
  //!
  //! @param dn
  //!  The distinguished name (DN) of an entry aginst which will
  //!  be made authentication.
  //! @param password
  //!  Password used for authentication.
  //!
  //! Third form allows specify the version of LDAP protocol used
  //! by connection to the LDAP server.
  //!
  //! @param version
  //!  Only @expr{2@} or @expr{3@} can be entered.
  //!
  //! @returns
  //!  Returns @expr{1@} on success, @expr{0@} otherwise.
  //!
  //! @note
  //!  Only simple authentication type is implemented. So be warned
  //!  clear text passwords are sent to the directory server.
  //!
  //! @note
  //!   The API change: the returning code was changed in Pike 7.3+
  //!   to follow his logic better.
  int bind (string|void dn, string|void password, int|void version) {

    int id;
    mixed raw;
    string pass = password;
    password = "censored";

    if (!version)
      version = LDAP_DEFAULT_VERSION;
    if (chk_ver())
      return 0;

    if (bound_dn && ldap_version <= 2) {
      ERROR ("Can't bind a connection more than once in LDAPv2.\n");
      return 0;
    }

    if (!stringp(dn))
      dn = mappingp(lauth->ext) ? lauth->ext->bindname||"" : "";
    if (!stringp(pass))
      pass = "";
    ldap_version = version;
    if(ldap_version == 3) {
      dn = string_to_utf8(dn);
      pass = string_to_utf8(pass);
    }
    if(intp(raw = send_bind_op(dn, pass))) {
      THROW(({error_string()+"\n",backtrace()}));
      return 0;
    }

   bound_dn = 0;
   last_rv = result(({raw}),1);
   if (!last_rv->error_number())
     bound_dn = dn;
   DWRITE_HI(sprintf("client.BIND: %s\n", last_rv->error_string()));
   seterr (last_rv->error_number(), last_rv->error_string());
   return !!bound_dn;

  } // bind


  private int send_unbind_op() {
  // UNBIND operation

    writemsg(ASN1_APPLICATION_OCTET_STRING(2, ""));

    //ldap::close();

    return 1;
  }

#if 0
  void destroy() {

    //send_unbind_op();

    // Hazard area: General confusion error. /mast
    //destruct(this);
  }
#endif

  //!
  //! Unbinds from the directory and close the connection.
  int unbind () {

    if (send_unbind_op() < 1) {
      THROW(({error_string()+"\n",backtrace()}));
      return -ldap_errno;
    }
    bound_dn = 0;
    DWRITE_HI("client.UNBIND: OK\n");

  } // unbind

  private int|string send_op_withdn(int op, string dn) {
  // DELETE, ...

    return do_op(ASN1_APPLICATION_OCTET_STRING(op, dn));

  }

  //!
  //! Deletes entry from the LDAP server.
  //!
  //! @param dn
  //!  The distinguished name of deleted entry.
  //!
  //! @returns
  //!  Returns @expr{1@} on success, @expr{0@} otherwise.
  //!
  //! @note
  //!   The API change: the returning code was changed in Pike 7.3+
  //!	to follow his logic better.
  int delete (string dn) {

    int id;
    mixed raw;

    if (chk_ver())
      return 0;
    if (chk_binded())
      return 0;
    if (chk_dn(dn))
      return 0;
    if(ldap_version == 3) {
      dn = string_to_utf8(dn);
    }
    if(intp(raw = send_op_withdn(10, dn))) {
      THROW(({error_string()+"\n",backtrace()}));
      return 0;
    }

   last_rv = result(({raw}));
   DWRITE_HI(sprintf("client.DELETE: %s\n", last_rv->error_string()));
   seterr (last_rv->error_number(), last_rv->error_string());
   return !last_rv->error_number();

  } // delete

  private int|string send_compare_op(string dn, string attr, string value) {
  // COMPARE

    object msgval;

    msgval = ASN1_APPLICATION_SEQUENCE(14, 
		({ Standards.ASN1.Types.asn1_octet_string(dn),
		Standards.ASN1.Types.asn1_sequence(
		  ({ Standards.ASN1.Types.asn1_octet_string(attr),
		  Standards.ASN1.Types.asn1_octet_string(value)
		  }))
		})
	     );

    return do_op(msgval);
  }


  //!
  //! Compares an attribute value with one in the directory.
  //!
  //! @param dn
  //!  The distinguished name of the entry.
  //!
  //! @param attr
  //!  The type (aka name) of the attribute to compare.
  //!
  //! @param value
  //!  The value to compare with. It's UTF-8 encoded automatically if
  //!  the attribute syntax specifies that.
  //!
  //! @returns
  //!  Returns @expr{1@} if at least one of the values for the
  //!  attribute in the directory is equal to @[value], @expr{0@} if
  //!  it didn't match or there was some error (use @[error_number] to
  //!  find out).
  //!
  //! @note
  //!   This function has changed arguments since version 7.6. From
  //!   7.3 to 7.6 it was effectively useless since it always returned
  //!   true.
  //!
  //! @note
  //!   The equality matching rule for the attribute governs the
  //!   comparison. There are attributes where the assertion syntax
  //!   used here isn't the same as the attribute value syntax.
  int compare (string dn, string attr, string value) {

    int id;
    mixed raw;

    // if (!aval || sizeof(aval)<2)
    //  error
    if (chk_ver())
      return 0;
    if (chk_binded())
      return 0;
    if (chk_dn(dn))
      return 0;
    if(ldap_version == 3) {
      dn = string_to_utf8(dn);
      if (function(string:string) encoder = get_attr_encoder (attr))
	value = encoder (value);
    }
    if(intp(raw = send_compare_op(dn, attr, value))) {
      THROW(({error_string()+"\n",backtrace()}));
      return 0;
    }

   last_rv = result(({raw}));
   DWRITE_HI(sprintf("client.COMPARE: %s\n", last_rv->error_string()));
   seterr (last_rv->error_number(), last_rv->error_string());
   return last_rv->error_number() == LDAP_COMPARE_TRUE;

  } // compare

  private int|string send_add_op(string dn, mapping(string:array(string)) attrs) {
  // ADD

    object msgval;
    string atype;
    array(object) oatt = ({});

    foreach(indices(attrs), atype) {
      string aval;
      array(object) ohlp = ({});

      foreach(values(attrs[atype]), aval)
	ohlp += ({Standards.ASN1.Types.asn1_octet_string(aval)});
      oatt += ({Standards.ASN1.Types.asn1_sequence(
		({Standards.ASN1.Types.asn1_octet_string(atype),
		  Standards.ASN1.Types.asn1_set(ohlp)
		}))
	      });
    }

    msgval = ASN1_APPLICATION_SEQUENCE(8, 
		({Standards.ASN1.Types.asn1_octet_string(dn),
		  Standards.ASN1.Types.asn1_sequence(oatt)
		}));

    return do_op(msgval);
  }


  //!  The Add Operation allows a client to request the addition
  //!  of an entry into the directory
  //!
  //! @param dn
  //!  The Distinguished Name of the entry to be added.
  //!
  //! @param attrs
  //!  The mapping of attributes and their values that make up the
  //!  content of the entry being added. Values that are sent UTF-8
  //!  encoded according the the attribute syntaxes are encoded
  //!  automatically.
  //!
  //! @returns
  //!  Returns @expr{1@} on success, @expr{0@} otherwise.
  //!    
  //! @note
  //!   The API change: the returning code was changed in Pike 7.3+
  //!	to follow his logic better.
  int add (string dn, mapping(string:array(string)) attrs) {

    int id;
    mixed raw;

    if (chk_ver())
      return 0;
    if (chk_binded())
      return 0;
    if (chk_dn(dn))
      return 0;
    if(ldap_version == 3) {
      dn = string_to_utf8(dn);
      attrs += ([]);
      // No need to UTF-8 encode the attribute names themselves since
      // only ascii chars are allowed in them.
      foreach (indices(attrs), string attr)
	if (function(string:string) encoder = get_attr_encoder (attr))
	  attrs[attr] = map (attrs[attr], encoder);
    }
    if(intp(raw = send_add_op(dn, attrs))) {
      THROW(({error_string()+"\n",backtrace()}));
      return 0;
    }

    last_rv = result(({raw}));
    DWRITE_HI(sprintf("client.ADD: %s\n", last_rv->error_string()));
    seterr (last_rv->error_number(), last_rv->error_string());
    return !last_rv->error_number();

  } // add

static mapping(string:array(string)) simple_read (string object_name,
						  object filter,
						  array attrs)
// Makes a base object search for object_name. The result is returned
// as a mapping where the attribute types have been lowercased and the
// string values are unprocessed.
{
  object|int search_request =
    make_search_op(object_name, 0, 0, 0, 0, 0, filter, attrs);
  //werror("search_request: %O\n", search_request);

  string|int raw;
  if(intp(raw = do_op(search_request))) {
    THROW(({error_string()+"\n",backtrace()}));
    return 0;
  }

  mapping(string:array(string)) res = ([]);

  do {
    object response = .ldap_privates.ldap_der_decode(raw);
    if (ASN1_GET_RESULTAPP (response) == 5) break;
    //werror("res: %O\n", res);
    response = response->elements[1];
    foreach(ASN1_GET_ATTR_ARRAY (response), object attr)
      res[lower_case (ASN1_GET_ATTR_NAME (attr))] = ASN1_GET_ATTR_VALUES (attr);
    // NB: The msgid stuff is defunct in readmsg, so we can
    // just as well pass a zero there. :P
    if (intp(raw = readmsg(0))) {
      THROW(({error_string()+"\n",backtrace()}));
      return 0;
    }
  } while (1);

  return res;
}

static mapping(string:array(string)) root_dse;

array(string) get_root_dse_attr (string attr)
//! Returns the value of an attribute in the root DSE (DSA-Specific
//! Entry) of the bound server. The result is cached. A working
//! connection is assumed.
//!
//! @returns
//!   The return value is an array of the attribute values, which have
//!   been UTF-8 decoded where appropriate.
//!
//!   Don't be destructive on the returned array.
//!
//! @note
//!   This function intentionally does not try to simplify the return
//!   values for single-valued attributes (c.f.
//!   @[Protocols.LDAP.SEARCH_MULTIVAL_ARRAYS_ONLY]). That since (at
//!   least) Microsoft AD has a bunch of attributes in the root DSE
//!   that they don't bother to provide schema entries for. The return
//!   value format wouldn't be reliable if they suddenly change that.
{
  attr = lower_case (attr);

  if (!root_dse || zero_type (root_dse[attr])) {
    PROFILE("get_root_dse_attr", {

	multiset(string) attrs = root_dse ? (<>) :
	  // Get a bunch of attributes in one go.
	  (<
	    // Request all standard operational attributes (RFC 2252,
	    // section 5.1). Some of them are probably not applicable
	    // in the root DSE, but better safe than sorry.
	    "createtimestamp",
	    "modifytimestamp",
	    "creatorsname",
	    "modifiersname",
	    "subschemasubentry",
	    "attributetypes",
	    "objectclasses",
	    "matchingrules",
	    "matchingruleuse",
	    // Request the standard root DSE operational attributes
	    // (RFC 2252, section 5.2).
	    "namingcontexts",
	    "altserver",
	    "supportedextension",
	    "supportedcontrol",
	    "supportedsaslmechanisms",
	    "supportedldapversion",
	  >);
	attrs[attr] = 1;

	mapping(string:array(string)) res =
	  simple_read ("", get_cached_filter ("(objectClass=*)"), indices (attrs));

	foreach (indices (res), string attr)
	  // Microsoft AD has several attributes in its root DSE that
	  // they haven't bothered to include in their schema. Send
	  // the nowarn flag to get_attr_encoder to avoid complaints
	  // about that.
	  if (function(string:string) decoder =
	      get_attr_decoder (attr, DO_IF_DEBUG (1)))
	    res[attr] = map (res[attr], decoder);

	if (root_dse)
	  root_dse[attr] = res[attr];
	else {
	  root_dse = res;
	  if (!root_dse[attr]) root_dse[attr] = 0;
	}
      });
  }

  return root_dse[attr];
}

static object make_control (string control_type, void|string value,
			    void|int critical)
{
  array(object) seq = ({Standards.ASN1.Types.asn1_octet_string (control_type),
			ASN1_BOOLEAN (critical)});
  if (value) seq += ({Standards.ASN1.Types.asn1_octet_string (value)});
  return Standards.ASN1.Types.asn1_sequence (seq);
}

static multiset(string) supported_controls;

multiset(string) get_supported_controls()
//! Returns a multiset containing the controls supported by the
//! server. They are returned as object identifiers on string form.
//! A working connection is assumed.
//!
//! @seealso
//!   @[search]
{
  if (!supported_controls)
    if (array(string) res = get_root_dse_attr ("supportedControl"))
      supported_controls = mkmultiset (res);
  return supported_controls;
}

object make_filter (string filter)
//! Returns the ASN1 object parsed from the given filter. This is a
//! wrapper for @[Protocols.LDAP.make_filter] which parses the filter
//! with the LDAP protocol version currently in use by this
//! connection.
//!
//! @throws
//!   If there's a parse error in the filter then a
//!   @[Protocols.LDAP.FilterError] is thrown as from
//!   @[Protocols.LDAP.make_filter].
{
  return Protocols.LDAP.make_filter (filter, ldap_version);
}

object get_cached_filter (string filter)
//! This is a wrapper for @[Protocols.LDAP.get_cached_filter] which
//! passes the LDAP protocol version currently in use by this
//! connection.
//!
//! @throws
//!   If there's a parse error in the filter then a
//!   @[Protocols.LDAP.FilterError] is thrown as from
//!   @[Protocols.LDAP.make_filter].
{
  return Protocols.LDAP.get_cached_filter (filter, ldap_version);
}

object get_default_filter()
//! Returns the ASN1 object parsed from the filter specified in the
//! LDAP URL, or zero if the URL doesn't specify any filter.
//!
//! @throws
//!   If there's a parse error in the filter then a
//!   @[Protocols.LDAP.FilterError] is thrown as from
//!   @[Protocols.LDAP.make_filter].
{
  if (!default_filter_obj && lauth->filter)
    default_filter_obj = make_filter (lauth->filter);
  return default_filter_obj;
}

  private object|int make_search_op(string basedn, int scope, int deref,
				    int sizelimit, int timelimit,
				    int attrsonly, object filter,
				    void|array(string) attrs)
  {
    // SEARCH
  // limitations: !!! sizelimit and timelimit should be unsigned int !!!

    object msgval;
    array(object) ohlp;

    ohlp = ({filter});
    if (arrayp(attrs)) { //explicitly defined attributes
      array(object) o2 = ({});
      foreach(attrs, string s2)
	o2 += ({Standards.ASN1.Types.asn1_octet_string(s2)});
      ohlp += ({Standards.ASN1.Types.asn1_sequence(o2)});
    } else
      ohlp += ({Standards.ASN1.Types.asn1_sequence(({}))});

    return ASN1_APPLICATION_SEQUENCE(3,
		({ Standards.ASN1.Types.asn1_octet_string(basedn),
		   ASN1_ENUMERATED(scope),
		   ASN1_ENUMERATED(deref),
		   Standards.ASN1.Types.asn1_integer(sizelimit),
		   Standards.ASN1.Types.asn1_integer(timelimit),
		   ASN1_BOOLEAN(attrsonly ? -1 : 0),
		   @ohlp
		})) ;
  }

  //! Search LDAP directory.
  //!
  //! @param filter
  //!   Search filter to override the one from the LDAP URL. It's
  //!   either a string with the format specified in RFC 2254, or an
  //!   object returned by @[Protocols.LDAP.make_filter].
  //!
  //! @param attrs
  //!   The array of attribute names which will be returned by server
  //!   for every entry.
  //!
  //! @param attrsonly
  //!   This flag causes server return only the attribute types (aka
  //!   names) but not their values. The values will instead be empty
  //!   arrays or - if @[Protocols.LDAP.SEARCH_MULTIVAL_ARRAYS_ONLY]
  //!   is given - zeroes for single-valued attributes.
  //!
  //! @param controls
  //!   Extra controls to send in the search query, to modify how the
  //!   server executes the search in various ways. The value is a
  //!   mapping with an entry for each control.
  //!
  //!   @mapping
  //!   @member string object_identifier
  //!     The index is the object identifier in string form for the
  //!     control type. There are constants in @[Protocols.LDAP] for
  //!     the object identifiers for some known controls.
  //! 
  //!     The mapping value is an array of two elements:
  //! 
  //!     @array
  //!     @elem int 0
  //!       The first element is an integer flag that specifies
  //!       whether the control is critical or not. If it is nonzero,
  //!       the server returns an error if it doesn't understand the
  //!       control. If it is zero, the server ignores it instead.
  //! 
  //!     @elem string|int(0..0) 1
  //!       The second element is the string value to pass with the
  //!       control. It may also be zero to not pass any value at all.
  //!     @endarray
  //!   @endmapping
  //!
  //! @param flags
  //!   Bitfield with flags to control various behavior at the client
  //!   side of the search operation. See the
  //!   @expr{Protocol.LDAP.SEARCH_*@} constants for details.
  //!
  //! @returns
  //!   Returns object @[LDAP.client.result] on success, @expr{0@}
  //!	otherwise.
  //!
  //! @note
  //!   The API change: the returning code was changed in Pike 7.3+
  //!	to follow his logic better.
  //!
  //! @seealso
  //!  @[result], @[result.fetch], @[read], @[get_supported_controls],
  //!  @[Protocols.LDAP.quote_filter_value], @[Protocols.LDAP.make_filter]
  result|int search (string|object|void filter, array(string)|void attrs,
		     int|void attrsonly,
		     void|mapping(string:array(int|string)) controls,
		     void|int flags) {

    int id,nv;
    mixed raw;
    array(string) rawarr = ({});

    DWRITE_HI("client.SEARCH: " + (string)filter + "\n");
    if (chk_ver())
      return 0;
    if (chk_binded())
      return 0;

    if (!objectp (filter))
      if (mixed err = catch {
	    if (!filter)
	      filter = get_default_filter();
	    else
	      filter = make_filter (filter);
	  }) {
	if (objectp (err) && err->is_ldap_filter_error) {
	  seterr (LDAP_FILTER_ERROR);
	  return 0;
	}
	else
	  throw (err);
      }

    object|int search_request =
      make_search_op(ldap_basedn, ldap_scope, ldap_deref,
	ldap_sizelimit, ldap_timelimit, attrsonly, filter,
	attrs||lauth->attributes);

    if(intp(search_request)) {
      THROW(({error_string()+"\n",backtrace()}));
      return 0;
    }

    array(object) common_controls;
    if (controls) {
      common_controls = allocate (sizeof (controls));
      int i;
      foreach (controls; string type; array(int|string) data)
	common_controls[i++] =
	  make_control (type, [string] data[1], [int] data[0]);
    }
    else common_controls = ({});

#if 0
    // Microsoft AD stuff that previously was added by default. There
    // doesn't appear to be a good reason for it. It's now possible
    // for the caller to do it, anyway. /mast
    if (get_supported_controls()[LDAP_SERVER_DOMAIN_SCOPE_OID]) {
      // LDAP_SERVER_DOMAIN_SCOPE_OID
      // "Tells server not to generate referrals" (NtLdap.h)
      common_controls += ({make_control (LDAP_SERVER_DOMAIN_SCOPE_OID)});
    }
#endif

#ifdef ENABLE_PAGED_SEARCH
    get_supported_controls();
#endif

    object cookie = Standards.ASN1.Types.asn1_octet_string("");
    rawarr = ({});
    do {
      PROFILE("send_search_op", {
	  array ctrls = common_controls;
	  IF_ELSE_PAGED_SEARCH (
	    if (supported_controls[LDAP_PAGED_RESULT_OID_STRING]) {
	      // LDAP Control Extension for Simple Paged Results Manipulation
	      // RFC 2696.
	      ctrls += ({make_control (
			   LDAP_PAGED_RESULT_OID_STRING,
			   Standards.ASN1.Types.asn1_sequence(
			     ({
			       // size
			       Standards.ASN1.Types.asn1_integer(0x7fffffff),
			       cookie,			// cookie
			     }))->get_der(),
			   sizeof(cookie->value)?0:0xff)});
	    },);
	  object controls;
	  if (sizeof(ctrls)) {
	    controls = .ldap_privates.asn1_sequence(0, ctrls);
	  }

	  if(intp(raw = do_op(search_request, controls))) {
	    THROW(({error_string()+"\n",backtrace()}));
	    return 0;
	  }
	});

      PROFILE("rawarr++", {
	  rawarr += ({raw});
	  while (ASN1_DECODE_RESULTAPP(raw) != 5) {
	    PROFILE("readmsg", raw = readmsg(id));
	    if (intp(raw)) {
	      THROW(({error_string()+"\n",backtrace()}));
	      return 0;
	    }
	    rawarr += ({raw});
	  } // while
	});

      // At this point @[raw] contains a SearchResultDone.
      cookie = 0;
      IF_ELSE_PAGED_SEARCH({
	  if ((ASN1_DECODE_RESULTCODE(raw) != 10) &&
	      (sizeof(.ldap_privates.ldap_der_decode(raw)->elements) > 2)) {
	    object controls = .ldap_privates.ldap_der_decode(raw)->elements[2];
	    foreach(controls->elements, object control) {
	      if (!control->constructed ||
		  !sizeof(control) ||
		  control->elements[0]->type_name != "OCTET STRING") {
		//werror("Protocol error in control %O\n", control);
		// FIXME: Fail?
		continue;
	      }
	      if (control->elements[0]->value !=
		  LDAP_PAGED_RESULT_OID_STRING) {
		//werror("Unknown control %O\n", control->elements[0]->value);
		// FIXME: Should look at criticallity flag.
		continue;
	      }
	      if (sizeof(control) == 1) continue;
	      int pos = 1;
	      if (control->elements[1]->type_name == "BOOLEAN") {
		if (sizeof(control) == 2) continue;
		pos = 2;
	      }
	      if (control->elements[pos]->type_name != "OCTET STRING") {
		// FIXME: Error?
		continue;
	      }
	      object control_info =
		.ldap_privates.ldap_der_decode(control->elements[pos]->value);
	      if (!control_info->constructed ||
		  sizeof(control_info) < 2 ||
		  control_info->elements[1]->type_name != "OCTET STRING") {
		// Unexpected control information.
		continue;
	      }
	      if (sizeof(control_info->elements[1]->value)) {
		cookie = control_info->elements[1];
	      }
	    }
	    if (cookie) {
	      // Remove the extra end marker.
	      rawarr = rawarr[..sizeof(rawarr)-2];
	    }
	  }
	    
	},);
    } while (cookie);

    PROFILE("result", last_rv = result (rawarr, 0, flags));
    if(objectp(last_rv))
      seterr (last_rv->error_number(), last_rv->error_string());
    //if (rv->error_number() || !rv->num_entries())	// if error or entries=0
    //  rv = rv->error_number();

    DWRITE_HI(sprintf("client.SEARCH: %s (entries: %d)\n",
    			last_rv->error_string(), last_rv->num_entries()));
    return last_rv;

  } // search

mapping(string:string|array(string)) read (
  string object_name,
  void|string filter,
  void|array(string) attrs,
  void|int attrsonly,
  void|mapping(string:array(int|string)) controls,
  void|int flags)
//! Reads a specified object in the LDAP server. @[object_name] is the
//! distinguished name for the object. The rest of the arguments are
//! the same as to @[search].
//!
//! The default filter and attributes that might have been set in the
//! LDAP URL doesn't affect this call. If @[filter] isn't set then
//! @expr{"(objectClass=*)"@} is used.
//!
//! @returns
//!   Returns a mapping of the requested attributes. It has the same
//!   form as the response from @[result.fetch].
//!
//! @seealso
//!   @[search]
{
  if (chk_ver())
    return 0;
  if (chk_binded())
    return 0;
  if(ldap_version == 3) {
    object_name = string_to_utf8 (object_name);
    if (filter) filter = string_to_utf8(filter);
  }

  object|int search_request =
    make_search_op (object_name, 0, ldap_deref,
		    ldap_sizelimit, ldap_timelimit, attrsonly,
		    filter || get_cached_filter ("(objectClass=*)"),
		    attrs);

  if(intp(search_request)) {
    THROW(({error_string()+"\n",backtrace()}));
    return 0;
  }

  object ctrls;
  if (controls) {
    array(object) control_list = allocate (sizeof (controls));
    int i;
    foreach (controls; string type; array(int|string) data)
      control_list[i++] =
	make_control (type, [string] data[1], [int] data[0]);
    if (sizeof (control_list))
      ctrls = .ldap_privates.asn1_sequence(0, control_list);
  }

  string|int raw;
  PROFILE ("send_get_op", {
      if(intp(raw = do_op(search_request, ctrls))) {
	THROW(({error_string()+"\n",backtrace()}));
	return 0;
      }
    });

  array(string) rawarr;
  PROFILE("rawarr++", {
      rawarr = ({raw});
      while (ASN1_DECODE_RESULTAPP(raw) != 5) {
	// NB: The msgid stuff is defunct in readmsg, so we can
	// just as well pass a zero there. :P
	PROFILE("readmsg", raw = readmsg(0));
	if (intp(raw)) {
	  THROW(({error_string()+"\n",backtrace()}));
	  return 0;
	}
	rawarr += ({raw});
      } // while
    });

  PROFILE ("result", last_rv = result (rawarr, 0, flags));
  seterr (last_rv->error_number(), last_rv->error_string());

  if (ldap_errno != LDAP_SUCCESS) return 0;
  return last_rv->fetch();
}

string|array(string) read_attr (string object_name,
				string attr,
				void|string filter,
				void|mapping(string:array(int|string)) controls)
//! Reads a specified attribute of a specified object in the LDAP
//! server. @[object_name] is the distinguished name of the object and
//! @[attr] is the attribute. The rest of the arguments are the same
//! as to @[search].
//!
//! The default filter that might have been set in the LDAP URL
//! doesn't affect this call. If @[filter] isn't set then
//! @expr{"(objectClass=*)"@} is used.
//!
//! @returns
//!   For single-valued attributes, the value is returned as a string.
//!   For multivalued attributes, the value is returned as an array of
//!   strings. Returns zero if there was an error.
//!
//! @seealso
//!   @[read], @[get_root_dse_attr]
{
  if (mapping(string:string|array(string)) res =
      read (object_name, filter, ({attr}), 0, controls,
	    SEARCH_MULTIVAL_ARRAYS_ONLY)) {
    m_delete (res, "dn");
    // Get the value regardless of the case of the attribute name that
    // the server used in the response.
    return get_iterator (res)->value();
  }
  return 0;
}

//! Return the LDAP protocol version in use.
int get_protocol_version() {return ldap_version;}

  //! Sets the base DN for searches using @[search] and schema queries
  //! using @[get_attr_type_descr].
  //!
  //! @note
  //!   For compatibility, the old base DN is returned. However, if
  //!   LDAPv3 is used, the value is UTF-8 encoded. Use @[get_basedn]
  //!   separately instead.
  string set_basedn (string base_dn) {

    string old_dn = ldap_basedn;

    if(ldap_version == 3) {
      base_dn = string_to_utf8(base_dn);
    }
    ldap_basedn = base_dn;
    DWRITE_HI("client.SET_BASEDN = " + base_dn + "\n");
    return old_dn;
  }

//! Returns the current base DN for searches using @[search] and
//! schema queries using @[get_attr_type_descr].
string get_basedn() {return utf8_to_string (ldap_basedn);}

//! Returns the bind DN currently in use for the connection. Zero is
//! returned if the connection isn't bound. The empty string is
//! returned if the connection is in use but no bind DN has been given
//! explicitly to @[bind].
string get_bound_dn() {return bound_dn;}

  //!
  //! Sets value of scope for search operation.
  //!
  //! @param scope
  //!  The value can be one of the @expr{SCOPE_*@} constants or a
  //!  string @expr{"base"@}, @expr{"one"@} or @expr{"sub"@}.
  //!
  //! @returns
  //!   Returns the @expr{SCOPE_*@} constant for the old scope.
  int set_scope (int|string scope) {

    int old_scope = ldap_scope;

    // support for string based values
    if(stringp(scope))
      switch (lower_case(scope)) {
	case "sub": scope = SCOPE_SUB; break;
	case "one": scope = SCOPE_ONE; break;
	case "base": scope = SCOPE_BASE; break;
	default: ERROR ("Invalid scope %O.\n", scope);
      }
    else
      if (!(<SCOPE_BASE, SCOPE_ONE, SCOPE_SUB>)[scope])
	ERROR ("Invalid scope %O.\n", scope);
 
    ldap_scope = scope;
    DWRITE_HI("client.SET_SCOPE = " + (string)scope + "\n");
    return old_scope;
  }

//! Return the currently set scope as a string @expr{"base"@},
//! @expr{"one"@}, or @expr{"sub"@}.
string get_scope()
  {return ([SCOPE_BASE: "base", SCOPE_ONE: "one", SCOPE_SUB: "sub"])[ldap_scope];}

  //! @param option_type
  //!   LDAP_OPT_xxx
  //! @param value
  //!   new value for option
  int set_option (int opttype, int value) {

    DWRITE_HI("client.SET_OPTION: " + (string)opttype + " = " + (string)value + "\n");
    switch (opttype) {
	case 1: // LDAP_OPT_DEREF
		  //if (intp(value))
		    ldap_deref = value;
		  //else
		  //  return -1;
		  break;
	case 2: // LDAP_OPT_SIZELIMIT
		  //if (intp(value))
		    ldap_sizelimit = value;
		  //else
		  //  return -1;
		  break;
	case 3: // LDAP_OPT_TIMELIMIT
		  //if (intp(value))
		    ldap_timelimit = value;
		  //else
		  //  return -1;
		  break;
	case 4: // LDAP_OPT_REFERRALS
	default: return -1;
    }
	

    return 0;
  }

  //! @param option_type
  //!   LDAP_OPT_xxx
  int get_option (int opttype) {


    DWRITE_HI("client.GET_OPTION: " + (string)opttype + "\n");
    switch (opttype) {
	case 1: // LDAP_OPT_DEREF
		  return ldap_deref;
	case 2: // LDAP_OPT_SIZELIMIT
		  return ldap_sizelimit;
	case 3: // LDAP_OPT_TIMELIMIT
		  return ldap_timelimit;
	case 4: // LDAP_OPT_REFERRALS
    }

    return -1;
  }

mapping(string:mixed) get_parsed_url() {return lauth;}
//! Returns a mapping containing the data parsed from the LDAP URL
//! passed to @[create]. The mapping has the same format as the return
//! value from @[Protocols.LDAP.parse_ldap_url]. Don't be destructive
//! on the returned value.

  private int|string send_modify_op(string dn,
				    mapping(string:array(mixed)) attropval) {
  // MODIFY

    object o, msgval;
    string atype;
    array(object) oatt = ({}), attrarr;


    foreach(indices(attropval), atype) {
      if(!intp((attropval[atype])[0]))
	return seterr(LDAP_PROTOCOL_ERROR);
      attrarr = ({});
      for(int ix=1; ix<sizeof(attropval[atype]); ix++)
	attrarr += ({Standards.ASN1.Types.asn1_octet_string(
			(attropval[atype])[ix])});
//      if(sizeof(attrarr)) // attributevalue ?
	o = Standards.ASN1.Types.asn1_sequence(
		({Standards.ASN1.Types.asn1_octet_string(atype),
		  Standards.ASN1.Types.asn1_set(attrarr)
		}));
//      else
//	o = Standards.ASN1.Encode.asn1_sequence(
//		Standards.ASN1.Encode.asn1_octet_string(atype));
      oatt += ({Standards.ASN1.Types.asn1_sequence(
		  ({ASN1_ENUMERATED((attropval[atype])[0]),
		    o
		  }))});
    } //foreach

    msgval = ASN1_APPLICATION_SEQUENCE(6, 
		({ Standards.ASN1.Types.asn1_octet_string(dn),
		   Standards.ASN1.Types.asn1_sequence(oatt)
		}));

    return do_op(msgval);
  }

  private int|string send_modifydn_op(string dn, string newrdn,
    int deleteoldrdn, string newsuperior) {

    object msgval;
    array seq=({ Standards.ASN1.Types.asn1_octet_string(dn),
               Standards.ASN1.Types.asn1_octet_string(newrdn),
               Standards.ASN1.Types.asn1_boolean(deleteoldrdn)
         });
    if(newsuperior)
          seq+=({Standards.ASN1.Types.asn1_octet_string(newsuperior)});

    msgval = ASN1_APPLICATION_SEQUENCE(12, seq);

    return do_op(msgval);
  }

  //!  The Modify DN Operation allows a client to change the leftmost
  //!  (least significant) component of the name of an entry in the directory,
  //!  or to move a subtree of entries to a new location in the directory.
  //!
  //! @param dn
  //!  DN of source object
  //!
  //! @param newrdn
  //!  RDN of destination
  //!
  //! @param deleteoldrdn
  //!  The parameter controls whether the old RDN attribute values
  //!  are to be retained as attributes of the entry, or deleted
  //!  from the entry.
  //!
  //! @param newsuperior
  //!  If present, this is the Distinguished Name of the entry
  //!  which becomes the immediate superior of the existing entry.
  //!
  //! @returns
  //!  Returns @expr{1@} on success, @expr{0@} otherwise.
  //!
  //! @note
  //!   The API change: the returning code was changed in Pike 7.3+
  //!	to follow his logic better.
  int modifydn (string dn, string newrdn, int deleteoldrdn,
                string|void newsuperior) {

    mixed raw;

    if (chk_ver())
      return 0;
    if (chk_binded())
      return 0;
    if (chk_dn(dn))
      return 0;
    if(ldap_version == 3) {
      dn = string_to_utf8(dn);
      newrdn = string_to_utf8(newrdn);
      if(newsuperior) newsuperior = string_to_utf8(newsuperior);
    }
    if(intp(raw = send_modifydn_op(dn, newrdn, deleteoldrdn, newsuperior))) {
      THROW(({error_string()+"\n",backtrace()}));
      return 0;
    }

    last_rv = result(({raw}));
    DWRITE_HI(sprintf("client.MODIFYDN: %s\n", last_rv->error_string()));
    seterr (last_rv->error_number(), last_rv->error_string());
    return !last_rv->error_number();

  }  //modifydn

  //!  The Modify Operation allows a client to request that a modification
  //!  of an entry be performed on its behalf by a server.
  //!
  //! @param dn
  //!  The distinguished name of modified entry.
  //!
  //! @param attropval
  //!  The mapping of attributes with requested operation and attribute's
  //!  values.
  //!
  //!  @code
  //!    attropval=([ attribute: ({operation, value1, value2, ...}) ])
  //!  @endcode
  //!
  //!  Where operation is one of the following:
  //!
  //!  @dl
  //!  @item Protocols.LDAP.MODIFY_ADD
  //!    Add values listed to the given attribute, creating the
  //!    attribute if necessary.
  //!  @item Protocols.LDAP.MODIFY_DELETE
  //!    Delete values listed from the given attribute, removing the
  //!    entire attribute if no values are listed, or if all current
  //!    values of the attribute are listed for deletion.
  //!  @item Protocols.LDAP.MODIFY_REPLACE
  //!    Replace all existing values of the given attribute with the
  //!    new values listed, creating the attribute if it did not
  //!    already exist. A replace with no value will delete the entire
  //!    attribute if it exists, and is ignored if the attribute does
  //!    not exist.
  //!  @enddl
  //!
  //!  Values that are sent UTF-8 encoded according the the attribute
  //!  syntaxes are encoded automatically.
  //!
  //! @returns
  //!  Returns @expr{1@} on success, @expr{0@} otherwise.
  //!
  //! @note
  //!   The API change: the returning code was changed in Pike 7.3+
  //!	to follow his logic better.
  int modify (string dn, mapping(string:array(int(0..2)|string)) attropval) {

    int id;
    mixed raw;

    if (chk_ver())
      return 0;
    if (chk_binded())
      return 0;
    if (chk_dn(dn))
      return 0;
    if(ldap_version == 3) {
      dn = string_to_utf8(dn);
      attropval += ([]);
      // No need to UTF-8 encode the attribute names themselves since
      // only ascii chars are allowed in them.
      foreach (indices (attropval), string attr)
	if (function(string:string) encoder = get_attr_encoder (attr)) {
	  array(int(0..2)|string) op = attropval[attr] + ({});
	  for (int i = sizeof (op); --i;) // Skips first element.
	    op[i] = encoder (op[i]);
	  attropval[attr] = op;
	}
    }
    if(intp(raw = send_modify_op(dn, attropval))) {
      THROW(({error_string()+"\n",backtrace()}));
      return 0;
    }

    last_rv = result(({raw}));
    DWRITE_HI(sprintf("client.MODIFY: %s\n", last_rv->error_string()));
    seterr (last_rv->error_number(), last_rv->error_string());
    return !last_rv->error_number();

  } // modify

  //! Gets referrals.
  //!
  //! @returns
  //!   Returns array of referrals or @expr{0@}.
  array|int get_referrals() {
    if(last_rv->referrals)
      return last_rv->referrals;
    return 0;
  }

  //! Compatibility alias for @[Protocols.LDAP.parse_ldap_url].
  mapping(string:mixed) parse_url (string ldapuri)
  {
    return parse_ldap_url (ldapuri);
  }


// Schema handling.

static mapping(string:array(string)) query_subschema (string dn,
						      array(string) attrs)
// Queries the server for the specified attributes in the subschema
// applicable for the specified object. The return value is on the
// same form as from simple_read (specifically there's no UTF-8
// decoding of the values).
//
// If dn == "" then the attribute values might be joined from several
// schemas. (Might change since I'm not sure whether that's really
// useful or not - haven't got a good grasp on how multiple schemas
// interact in the same server. /mast)
{
  mapping(string:array(string)) subschema_response;
  int utf8_decode_dns;

  if (dn == "" && root_dse)
    subschema_response = root_dse;
  else {
    subschema_response =
      simple_read (dn, get_cached_filter ("(objectClass=*)"), ({"subschemaSubentry"}));
    utf8_decode_dns = 1;
  }

  if (subschema_response)
    if (array(string) subschema_dns = subschema_response->subschemasubentry) {
      if (sizeof (subschema_dns) == 1)
	return simple_read (
	  utf8_decode_dns ? utf8_to_string (subschema_dns[0]) : subschema_dns[0],
	  get_cached_filter ("(objectClass=subschema)"), attrs);

      else {
	// This should afaics only occur for the root DSE, but it's a
	// bit confusing: RFC 2252 section 5.1.5 specifies that
	// subschemaSubentry is single valued, while RFC 2251 section
	// 3.4 says that it can contain zero or more values in the
	// root DSE. /mast
	mapping(string:array(string)) res = ([]);
	foreach (subschema_dns, string subschema_dn) {
	  if (mapping(string:array(string)) subres = simple_read (
		utf8_decode_dns ? utf8_to_string (subschema_dn) : subschema_dn,
		get_cached_filter ("(objectClass=subschema)"), attrs))
	    foreach (indices (subres), string attr)
	      res[attr] += subres[attr];
	}
	return res;
      }
    }

  return 0;
}

static mapping(string:mixed) parse_schema_terms (
  string str,
  mapping(string:string|multiset(string)) known_terms,
  string errmsg_prefix)
// Parses a string containing a parenthesized list of terms as used in
// several schema related attributes. The known_terms mapping
// specifies the syntax of the known terms. If there's an entry "" in
// it it's used for all other encountered terms.
{
  string orig_str = str, oid;

  // Doin a slightly lax check of the oid syntax here.
  if (!sscanf (str, "(%*[ ]%[0-9.]%*[ ]%s", oid, str))
    ERROR ("%sExpected '(' at beginning: %O\n",
	   errmsg_prefix, orig_str);
  if (!sizeof (oid))
    ERROR ("%sNumeric object identifier missing at start: %O\n",
	   errmsg_prefix, orig_str);

  mapping(string:mixed) res = (["oid": oid]);

  do {
    int pos = sizeof (str);

    // Note: RFC 2252 is not clear on what chars are allowed in term
    // identifier. We assume the same set as for attribute names.
    sscanf (str, "%[-;a-zA-Z0-9]%*[ ]%s", string term_id, str);
    if (!sizeof (term_id))
      ERROR ("%sTerm identifier expected at pos %d: %O\n",
	     errmsg_prefix, sizeof (orig_str) - pos, orig_str);

    string|multiset(string) term_syntax = known_terms[term_id] || known_terms[""];
    switch (term_syntax) {
      case 0:
	ERROR ("%sUnknown term %O at pos %d: %O\n",
	       errmsg_prefix, term_id, sizeof (orig_str) - pos, orig_str);

      case "flag":		// Existence only - no value.
	res[term_id] = 1;
	break;

      case "oid": {		// Numeric oid or name.
	// No strict syntax check here.
	sscanf (str, "%[-;a-zA-Z0-9.]%*[ ]%s", string oid, str);
	if (!sizeof (oid))
	  ERROR ("%sExpected oid after term %O at pos %d: %O\n",
		 errmsg_prefix, term_id, sizeof (orig_str) - pos, orig_str);
	res[term_id] = oid;
	break;
      }

      case "oidlen": {		// OID with optional length.
	// Cope with Microsoft AD which incorrectly quotes this field
	// and allows a name (RFC 2252 section 4.3.2 specifies a
	// numeric oid only).

	int ms_kludge;
	string oid;
	if (has_prefix (str, "'")) {
	  ms_kludge = 1;
	  // No strict syntax check here.
	  sscanf (str, "'%[-;a-zA-Z0-9.]%s", oid, str);
	}
	else {
	  // No strict syntax check here.
	  sscanf (str, "%[0-9.]%s", oid, str);
	}

	if (!sizeof (oid))
	  ERROR ("%sExpected numeric oid after term %O at pos %d: %O\n",
		 errmsg_prefix, term_id, sizeof (orig_str) - pos, orig_str);
	term_id = lower_case (term_id);
	res[term_id + "_oid"] = oid;
	if (sscanf (str, "{%d}%s", int len, str))
	  res[term_id + "_len"] = len;
	if (ms_kludge) {
	  if (!sscanf (str, "'%*[ ]%s", str))
	    ERROR ("%sUnterminated quoted oid after term %O at pos %d: %O\n",
		   errmsg_prefix, term_id, sizeof (orig_str) - pos, orig_str);
	}
	else
	  sscanf (str, "%*[ ]%s", str);
	break;
      }

      case "qdstring":		// Quoted UTF-8 string.
	string parse_qdstring (string what)
	{
	  string qstr;
	  switch (sscanf (str, "'%[^']'%*[ ]%s", qstr, str)) {
	    case 0:
	      ERROR ("%sExpected %s after term %O at pos %d: %O\n",
		     errmsg_prefix, what, term_id, sizeof (orig_str) - pos, orig_str);
	    case 1:
	      ERROR ("%sUnterminated %s after term %O at pos %d: %O\n",
		     errmsg_prefix, what, term_id, sizeof (orig_str) - pos, orig_str);
	  }
	  if (catch (qstr = utf8_to_string (qstr)))
	    ERROR ("%sMalformed UTF-8 in %s after term %O at pos %d: %O\n",
		   term_id, what, sizeof (orig_str) - pos, orig_str);
	  return ldap_decode_string (qstr);
	};
	res[term_id] = parse_qdstring ("quoted string");
	break;

      case "qdstrings":		// One or more quoted UTF-8 strings.
	if (sscanf (str, "(%*[ ]%s", str)) {
	  array(string) list = ({});
	  do {
	    if (str == "")
	      ERROR ("%sUnterminated parenthesis after term %O at pos %d: %O\n",
		     errmsg_prefix, term_id, sizeof (orig_str) - pos, orig_str);
	    list += ({parse_qdstring ("quoted string in list")});
	  } while (!sscanf (str, ")%*[ ]%s", str));
	  res[term_id] = list;
	}
	else
	  res[term_id] = ({parse_qdstring ("quoted string")});
	break;

      case "qdescr":		// Quoted name.
	string parse_qdescr (string what)
	{
	  string name;
	  // No strict syntax check here.
	  switch (sscanf (str, "'%[-;a-zA-Z0-9]'%*[ ]%s", name, str)) {
	    case 0:
	      ERROR ("%sExpected %s after term %O at pos %d: %O\n",
		     errmsg_prefix, what, term_id, sizeof (orig_str) - pos, orig_str);
	    case 1:
	      ERROR ("%sInvalid chars in %s after term %O at pos %d: %O\n",
		     errmsg_prefix, what, term_id, sizeof (orig_str) - pos, orig_str);
	  }
	  return name;
	};
	res[term_id] = parse_qdescr ("quoted descr");
	break;

      case "qdescrs":		// One or more quoted names.
	if (sscanf (str, "(%*[ ]%s", str)) {
	  array(string) list = ({});
	  do {
	    if (str == "")
	      ERROR ("%sUnterminated parenthesis after term %O at pos %d: %O\n",
		     errmsg_prefix, term_id, sizeof (orig_str) - pos, orig_str);
	    list += ({parse_qdescr ("quoted descr in list")});
	  } while (!sscanf (str, ")%*[ ]%s", str));
	  res[term_id] = list;
	}
	else
	  res[term_id] = ({parse_qdescr ("quoted descr")});
	break;

      default:
	if (multisetp (term_syntax)) { // One of a set.
	  sscanf (str, "%[-;a-zA-Z0-9.]%*[ ]%s", string choice, str);
	  if (!sizeof (choice))
	    ERROR ("%sExpected keyword after term %O at pos %d: %O\n",
		   errmsg_prefix, term_id, sizeof (orig_str) - pos, orig_str);
	  if (!term_syntax[choice])
	    ERROR ("%sUnknown keyword after term %O at pos %d: %O\n",
		   errmsg_prefix, term_id, sizeof (orig_str) - pos, orig_str);
	  res[term_id] = choice;
	  break;
	}

	ERROR ("Unknown syntax %O in known_perms.\n", term_syntax);
    }
  } while (!sscanf (str, ")%s", str));

  if (str != "")
    ERROR ("%sUnexpected data after ending ')' at pos %d: %O\n",
	   errmsg_prefix, sizeof (orig_str) - sizeof (str) - 1, orig_str);

  return res;
}

static mapping(string:mapping(string:mixed)) attr_type_descrs;

mapping(string:mixed) get_attr_type_descr (string attr, void|int standard_attrs)
//! Returns the attribute type description for the given attribute,
//! which includes the name, object identifier, syntax, etc (see
//! section 4.2 in RFC 2252 for details).
//!
//! This might do a query to the server, but results are cached.
//!
//! @param attr
//!   The name of the attribute. Might also be the object identifier
//!   on string form.
//!
//! @param standard_attrs
//!   Flag that controls how the known standard attributes stored in
//!   @[Protocols.LDAP] are to be used:
//!
//!   @int
//!   @value 0
//!     Check the known standard attributes first. If the attribute
//!     isn't found there, query the server. This is the default.
//!   @value 1
//!     Don't check the known standard attributes, i.e. always use the
//!     schema from the server.
//!   @value 2
//!     Only check the known standard attributes. The server is never
//!     contacted.
//!   @endint
//!
//! @returns
//!   Returns a mapping where the indices are the terms that the
//!   server has returned and the values are their values on string
//!   form (dequoted and converted from UTF-8 as appropriate). Terms
//!   without values get @expr{1@} as value in the mapping.
//!
//!   The mapping might contain the following members (all except
//!   @expr{"oid"@} are optional):
//!
//!   @mapping
//!   @member string "oid"
//!     The object identifier on string form (i.e. a dotted decimal
//!     string).
//!   @member string "NAME"
//!     Array with one or more names used for the attribute.
//!   @member string "DESC"
//!     Description.
//!   @member string "OBSOLETE"
//!     Flag.
//!   @member string "SUP"
//!     Derived from this other attribute. The value is the name or
//!     oid of it. Note that the attribute description from the
//!     referenced type always is merged with the current one to make
//!     the returned description complete.
//!   @member string "EQUALITY"
//!     The value is the name or oid of a matching rule.
//!   @member string "ORDERING"
//!     The value is the name or oid of a matching rule.
//!   @member string "SUBSTR"
//!     The value is the name or oid of a matching rule.
//!   @member string "syntax_oid"
//!     The value is the oid of the syntax (RFC 2252, section 4.3.2).
//!     (This is extracted from the @expr{"SYNTAX"@} term.)
//!   @member string "syntax_len"
//!     Optional suggested minimum upper bound of the number of
//!     characters in the attribute (or bytes if the attribute is
//!     binary). (This is extracted from the @expr{"SYNTAX"@} term.)
//!   @member string "SINGLE-VALUE"
//!     Flag. Default multi-valued.
//!   @member string "COLLECTIVE"
//!     Flag. Default not collective.
//!   @member string "NO-USER-MODIFICATION"
//!     Flag. Default user modifiable.
//!   @member string "USAGE"
//!     The value is any of the following:
//!     @string
//!     @value "userApplications"
//!     @value "directoryOperation"
//!       Self-explanatory.
//!     @value "distributedOperation"
//!       DSA-shared.
//!     @value "dSAOperation"
//!       DSA-specific, value depends on server.
//!     @endstring
//!   @endmapping
//!
//!   There might be more fields for server extensions.
//!
//!   Zero is returned if the server didn't provide any attribute type
//!   description for @[attr].
//!
//! @note
//!   It's the schema applicable at the base DN that is queried.
//!
//! @note
//!   LDAPv3 is assumed.
{
  // Don't bother lowercasing numeric oids. Names never start with a digit.
  if (!(<'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'>)[attr[0]])
    attr = lower_case (attr);

  if (mapping(string:mixed) descr = standard_attrs != 1 &&
      _standard_attr_type_descrs[attr])
    return descr;
  if (standard_attrs == 2)
    return 0;

  if (!attr_type_descrs) {
    attr_type_descrs = ([]);
    if (mapping(string:array(string)) subschema =
	query_subschema (ldap_basedn, ({"attributeTypes"})))
      if (array(string) attr_types = subschema->attributetypes) {

	// Note partial code dup with
	// Protocols.LDAP._standard_attr_type_descrs init.
	array(mapping(string:mixed)) incomplete = ({});

	foreach (attr_types, string attr_type) {
	  mapping(string:mixed) descr = parse_schema_terms (
	    utf8_to_string (attr_type),
	    (["NAME":			"qdescrs",
	      "DESC":			"qdstring",
	      "OBSOLETE":		"flag",
	      "SUP":			"oid",
	      "EQUALITY":		"oid",
	      "ORDERING":		"oid",
	      "SUBSTR":			"oid",
	      "SYNTAX":			"oidlen",
	      "SINGLE-VALUE":		"flag",
	      "COLLECTIVE":		"flag",
	      "NO-USER-MODIFICATION":	"flag",
	      "USAGE": (<"userApplications", "directoryOperation",
			 "distributedOperation", "dSAOperation">),
	      "":			"qdstrings"]),
	    "Error in attributeTypes when querying schema: ");
	  if (descr->SUP) incomplete += ({descr});
	  attr_type_descrs[descr->oid] = descr;
	  foreach (descr->NAME, string name)
	    attr_type_descrs[lower_case (name)] = descr;
	}

	void complete (mapping(string:mixed) descr) {
	  string sup = lower_case (descr->SUP);
	  mapping(string:mixed) sup_descr =
	    attr_type_descrs[sup] ||
	    (standard_attrs != 1 && _standard_attr_type_descrs[sup]);
	  if (!sup_descr)
	    ERROR ("Inconsistency in schema: "
		   "Got SUP reference to unknown attribute: %O\n", descr);
	  if (sup_descr->SUP)
	    complete (sup_descr);
	  foreach (indices (sup_descr), string term)
	    if (zero_type (descr[term]))
	      descr[term] = sup_descr[term];
	};
	foreach (incomplete, mapping(string:mixed) descr)
	  complete (descr);
      }
  }

  return attr_type_descrs[attr];
}

#ifdef PARSE_RFCS

int main (int argc, array(string) argv)
{
  // Try to parse a bit of RFC text to generate _ATD_ constants for
  // Protocols.LDAP.

  array(array(string)) sections = ({});

  {
    // Split on section headers.
    array(string) cont;
    while (string line = Stdio.stdin->gets()) {
      if (sscanf (line, "%[0-9.]%*[ \t]%s", string sno, string shdr) == 3 &&
	  sno != "" && shdr != "") {
	if (cont) sections += ({cont});
	if (has_suffix (sno, ".")) sno = sno[..<1];
	cont = ({sno, shdr});
      }
      else if (cont)
	cont += ({line});
    }
    if (cont) sections += ({cont});
  }

  foreach (sections, array(string) cont)
    for (int n = 0; n < sizeof (cont); n++) {
      if (sscanf (cont[n], "%*[ \t](%*s") == 2 &&
	  (n == 0 || String.trim_whites (cont[n-1]) == "")) {
	string expr = String.trim_whites (cont[n]), s;
	for (n++; n < sizeof (cont) && (s = String.trim_whites (cont[n])) != ""; n++)
	  expr += " " + s;

	mapping descr;
	if (mixed err =
	    catch (descr = parse_schema_terms (
		     expr,
		     (["NAME":			"qdescrs",
		       "DESC":			"qdstring",
		       "OBSOLETE":		"flag",
		       "SUP":			"oid",
		       "EQUALITY":		"oid",
		       "ORDERING":		"oid",
		       "SUBSTR":		"oid",
		       "SYNTAX":		"oidlen",
		       "SINGLE-VALUE":		"flag",
		       "COLLECTIVE":		"flag",
		       "NO-USER-MODIFICATION":	"flag",
		       "USAGE": (<"userApplications", "directoryOperation",
				  "distributedOperation", "dSAOperation">),
		       "":			"qdstrings"]),
		     "")))
	  werror (describe_error (err));

	write ("constant ATD_%s = ([ // %s, %s\n",
	       replace (cont[1], " ", "_"), argv[1], cont[0]);
	foreach (({"oid", "NAME", "DESC", "OBSOLETE", "SUP", "EQUALITY", "ORDERING",
		   "SUBSTR", "syntax_oid", "syntax_len", "SINGLE-VALUE", "COLLECTIVE",
		   "NO-USER-MODIFICATION", "USAGE"}), string term) {
	  if (mixed val = descr[term]) {
	    if (arrayp (val))
	      write ("  %O: ({%s}),\n", term,
		     map (val, lambda (string s) {return sprintf ("%O", s);}) * ", ");
	    else {
	      if (string sym = (<"oid", "syntax_oid">)[term] &&
		  get_constant_name (val))
		write ("  %O: %s,\n", term, sym);
	      else
		write ("  %O: %O,\n", term, val);
	    }
	  }
	}
	write ("]);\n");
      }
    }
}

#endif

#else
constant this_program_does_not_exist=1;
#endif
