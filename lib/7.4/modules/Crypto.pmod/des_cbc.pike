
#pike 7.5

//! Use @expr{Crypto.CBC(Crypto.DES)@} instead.
//! @deprecated

#if constant(Nettle.CBC)

inherit Nettle.CBC;
void create() { ::create(Crypto.DES()); }
string crypt_block(string data) { return crypt(data); }
int query_key_length() { return key_size(); }
int query_block_size() { return block_size(); }

#endif
