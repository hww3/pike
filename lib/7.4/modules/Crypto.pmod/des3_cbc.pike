
#pike __REAL_VERSION__

//! Use @expr{Crypto.CBC(Crypto.DES3)@} instead.
//! @deprecated

inherit Nettle.CBC;
void create() { ::create(Crypto.DES3()); }
string crypt_block(string data) { return crypt(data); }
int query_key_length() { return key_size(); }
int query_block_size() { return block_size(); }
