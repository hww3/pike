#pike __REAL_VERSION__

//! MD5 is a message digest function constructed by Ronald Rivest, and
//! is described in RFC 1321. It outputs message digests of 128 bits,
//! or 16 octets.

#if constant(Nettle.MD5_Info)

// NOTE: Depends on the order of INIT invocations.
inherit Nettle.MD5_Info;
inherit .Hash;

.HashState `()() { return Nettle.MD5_State(); }

#endif
