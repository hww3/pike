#pike 7.5

#if constant(Nettle.MD4_State)
inherit Nettle.MD4_State;

string identifier() { return "*\206H\206\367\r\2\4"; }
string name() { return "MD4"; }
#elif constant(Crypto.md4)
inherit Crypto.md4;
#endif
