// $Id: RSA.pmod,v 1.18 2003/01/27 02:54:02 nilsson Exp $

//! RSA operations and types as described in PKCS-1.

#pike __REAL_VERSION__
// #pragma strict_types

#if 0
#define WERROR werror
#else
#define WERROR(x)
#endif

#if constant(Standards.ASN1.Decode.simple_der_decode)

import Standards.ASN1.Types;

//! Create a DER-coded RSAPublicKey structure
//! @param rsa
//!   @[Crypto.rsa] object
//! @returns
//!   ASN1 coded RSAPublicKey structure
string public_key(Crypto.rsa rsa)
{
  return Sequence(map( ({ rsa->get_n(), rsa->get_e() }),
		       Integer))->get_der();
}

//! Create a DER-coded RSAPrivateKey structure
//! @param rsa
//!   @[Crypto.rsa] object
//! @returns
//!   ASN1 coded RSAPrivateKey structure
string private_key(Crypto.rsa rsa)
{
  Gmp.mpz n = rsa->get_n();
  Gmp.mpz e = rsa->get_e();
  Gmp.mpz d = rsa->get_d();
  Gmp.mpz p = rsa->get_p();
  Gmp.mpz q = rsa->get_q();

  return Sequence(map(
    ({ 0, n, e, d,
       p, q,
       d % (p - 1), d % (q - 1),
       q->invert(p) % p
    }),
    Integer))->get_der();
}

//! Decode a DER-coded RSAPublicKey structure
//! @param key
//!   RSAPublicKey provided in ASN1 encoded format
//! @returns
//!   @[Crypto.rsa] object
Crypto.rsa parse_public_key(string key)
{
  Object a = Standards.ASN1.Decode.simple_der_decode(key);

  if (!a
      || (a->type_name != "SEQUENCE")
      || (sizeof(a->elements) != 2)
      || (sizeof(a->elements->type_name - ({ "INTEGER" }))) )
    return 0;

  Crypto.rsa rsa = Crypto.rsa();
  rsa->set_public_key(a->elements[0]->value, a->elements[1]->value);
  return rsa;
}

//! Decode a DER-coded RSAPrivateKey structure
//! @param key
//!   RSAPrivateKey provided in ASN1 encoded format
//! @returns
//!   @[Crypto.rsa] object
Crypto.rsa parse_private_key(string key)
{
  WERROR(sprintf("rsa->parse_private_key: '%s'\n", key));
  Object a = Standards.ASN1.Decode.simple_der_decode(key);
  
  WERROR(sprintf("rsa->parse_private_key: asn1 = %O\n", a));
  if (!a
      || (a->type_name != "SEQUENCE")
      || (sizeof(a->elements) != 9)
      || (sizeof(a->elements->type_name - ({ "INTEGER" })))
      || a->elements[0]->value)
    return 0;
  
  Crypto.rsa rsa = Crypto.rsa();
  rsa->set_public_key(a->elements[1]->value, a->elements[2]->value);
  rsa->set_private_key(a->elements[3]->value, a->elements[4..]->value);
  return rsa;
}

Sequence build_rsa_public_key(object rsa)
{
  return Sequence( ({
    Sequence(
      ({ .Identifiers.rsa_id, Null() }) ),
    BitString(Sequence(
      ({ Integer(rsa->n), Integer(rsa->e) }) )->get_der()) }) );
}

#endif
