#pike __REAL_VERSION__

#charset iso-2022

//! Japanese language locale.

// $Id: jpn.pmod,v 1.1 2002/10/01 10:57:57 nilsson Exp $

inherit "abstract";

constant name = "$BF|K\8l(B";
constant english_name = "japanese";
constant iso_639_1 = "ja";
constant iso_639_2 = "jpn";
constant iso_639_2B = "jpn";

constant aliases =  ({ "ja", "jp", "jpn", "japanese", "nihongo",
			"$BF|K\8l(B" });

constant required_charset = "iso-2022";

/* The following function is correct for -10**12 < n < 10**12 (I think...) */

string mknumber(int n)
{
  array(string) digit;
  string r;
  digit = ({ "", "$B0l(B", "$BFs(B", "$B;0(B", "$B;M(B", "$B8^(B", "$BO;(B", "$B<7(B", "$BH,(B", "$B6e(B" });

  if(!n) return "$B%<%m(B";

  if(n<0) return "$BIi(B"+mknumber(-n);

  if(n>=200000000)
    return mknumber(n/100000000)+"$B2/(B"+mknumber(n%100000000);
  else if(n>100000000)
    return "$B2/(B"+mknumber(n%100000000);
  else if(n==100000000)
    return "$B2/(B";

  if(n>=20000)
    return mknumber(n/10000)+"$BK|(B"+mknumber(n%10000);
  else if(n>10000)
    return "$BK|(B"+mknumber(n%10000);
  else if(n==10000)
    return "$BK|(B";

  r = "";

  if(n>=2000)
    r += digit[n/1000]+"$B@i(B";
  else if(n>=1000)
    r += "$B@i(B";

  n %= 1000;
  if(n>=200)
    r += digit[n/100]+"$BI4(B";
  else if(n>=100)
    r += "$BI4(B";

  n %= 100;
  if(n>=20)
    r += digit[n/10]+"$B==(B";
  else if(n>=10)
    r += "$B==(B";

  return r + digit[n%10];
}


string ordered(int i)
{
  return mknumber(i)+"$BHV(B";
}

string date(int timestamp, string|void m)
{
  mapping t1=localtime(timestamp);
  mapping t2=localtime(time(0));

  if(m=="full")
    return ctime(timestamp)[11..15]+
      mknumber(t1["year"]+1900)+"$BG/(B"+mknumber(t1["mon"]+1)+
       "$B7n(B"+mknumber(t1["mday"])+"$BF|(B";

  if(m=="date")
    return mknumber(t1["year"]+1900)+"$BG/(B"+mknumber(t1["mon"]+1)+
       "$B7n(B"+mknumber(t1["mday"])+"$BF|(B";

  if(m=="time")
    return ctime(timestamp)[11..15];

  // !m

  if(t1["yday"] == t2["yday"] && t1["year"] == t2["year"])
    return "$B:#F|(B" + ctime(timestamp)[11..15];

  if(t1["yday"]+1 == t2["yday"] && t1["year"] == t2["year"])
    return "$B:rF|(B" + ctime(timestamp)[11..15];

  if(t1["yday"]-1 == t2["yday"] && t1["year"] == t2["year"])
    return "$BL@F|(B" + ctime(timestamp)[11..15];

  if(t1["year"] == t2["year"])
    return mknumber(t1["mon"]+1)+"$B7n(B" + mknumber(t1["mday"])+"$BF|(B";
  if(t1["year"]+1 == t2["year"])
    return "$B5lG/(B" + mknumber(t1["mon"]+1)+"$B7n(B" + mknumber(t1["mday"])+"$BF|(B";
  if(t1["year"]-1 == t2["year"])
    return "$B<!G/(B" + mknumber(t1["mon"]+1)+"$B7n(B" + mknumber(t1["mday"])+"$BF|(B";
  return mknumber(t1["year"]+1900)+"$BG/(B" + mknumber(t1["mon"]+1)+"$B7n(B" +
    mknumber(t1["mday"])+"$BF|(B";
}


string number(int num)
{
  return mknumber(num);
}

string month(int num)
{
  return mknumber(num)+"$B7n(B";
}

string day(int num)
{
  return ({ "$BF|(B", "$B7n(B", "$B2P(B", "$B?e(B", "$BLZ(B", "$B6b(B", "$BEZ(B" })[ num - 1 ]+
	    "$BMKF|(B";
}
