#pike 7.5

//! @deprecated Crypto.DES3

#if constant(Nettle.DES3_State)
inherit Nettle.DES3_State;

string name() { return "DES"; } // Yep, it doesn't say DES3

array(int) query_key_length() { return ({ 8, 8, 8 }); }
int query_block_size() { return 8; }
int query_key_size() { return 16; }
string crypt_block(string p) { return crypt(p); }
#endif
