#pike __REAL_VERSION__

//! SERPENT is one of the AES finalists, designed by Ross Anderson,
//! Eli Biham and Lars Knudsen. Thus, the interface and properties are
//! similar to @[AES]'. One peculiarity is that it is quite pointless to
//! use it with anything but the maximum key size, smaller keys are
//! just padded to larger ones.

#if constant(Nettle.Serpent_Info)

// NOTE: Depends on the order of INIT invocations.
inherit Nettle.Serpent_Info;
inherit .Cipher;

.CipherState `()() { return Nettle.Serpent_State(); }

#endif
