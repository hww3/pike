#pike __REAL_VERSION__
#pragma strict_types

#define BEGIN 32

constant Buffer = __builtin.Buffer;

constant count=__builtin.string_count;
constant width=__builtin.string_width;
constant trim_whites = __builtin.string_trim_whites;
constant trim_all_whites = __builtin.string_trim_all_whites;
constant Iterator = __builtin.string_iterator;
constant SplitIterator = __builtin.string_split_iterator;
constant Replace = __builtin.multi_string_replace;
constant SingleReplace = __builtin.single_string_replace;
constant int2char = int2char;
constant int2hex = int2hex;

//! This function implodes a list of words to a readable string, e.g.
//! ({"straw","berry","pie"}) becomes "straw, berry and pie".
//! If the separator is omitted, the default is @tt{"and"@}.
//! If the words are numbers they are converted to strings first.
//!
//! @seealso
//! @[`*()]
//!
string implode_nicely(array(string|int|float) foo, string|void separator)
{
  if(!separator) separator="and";
  foo=(array(string))foo;
  switch(sizeof(foo))
  {
  case 0: return "";
  case 1: return ([array(string)]foo)[0];
  default: return foo[0..sizeof(foo)-2]*", "+" "+separator+" "+foo[-1];
  }
}

//! Convert the first character in @[str] to upper case, and return the
//! new string.
//!
//! @seealso
//! @[lower_case()], @[upper_case()]
//!
string capitalize(string str)
{
  return upper_case(str[0..0])+str[1..sizeof(str)];
}

//! Convert the first character in each word (separated by spaces) in
//! @[str] to upper case, and return the new string.
//!
string sillycaps(string str)
{
  return Array.map(str/" ",capitalize)*" ";
}

//! This function multiplies @[str] by @[num]. The return value is the same
//! as appending @[str] to an empty string @[num] times.
//!
//! @note
//! This function is obsolete, since this functionality has been incorporated
//! into @[`*()].
//!
//! @seealso
//! @[`*()]
//!
string strmult(string str, int num)
{
#if 1
  num*=strlen(str);
  while(strlen(str) < num) str+=str;
  return str[0..num-1];
#endif
#if 0
  return sprintf("%~n",str,strlen(str)*num);
#endif
}

/*
 * string common_prefix(array(string) strs)
 * {
 *   if(!sizeof(strs))
 *     return "";
 *  
 *   for(int n = 0; n < sizeof(strs[0]); n++)
 *     for(int i = 1; i < sizeof(strs); i++)
 * 	if(sizeof(strs[i]) <= n || strs[i][n] != strs[0][n])
 * 	  return strs[0][0..n-1];
 *
 *   return strs[0];
 * }
 *
 * This function is a slightly optimised version based on the code
 * above (which is far more suitable for an implementation in C).
 */

//! Find the longest common prefix from an array of strings.
//!
string common_prefix(array(string) strs)
{
  if(!sizeof(strs))
    return "";

  string strs0 = strs[0];
  int n, i;
  
  catch
  {
    for(n = 0; n < sizeof(strs0); n++)
      for(i = 1; i < sizeof(strs); i++)
	if(strs[i][n] != strs0[n])
	  return strs0[0..n-1];
  };

  return strs0[0..n-1];
}

// Deprecated. Use String.Buffer instead.
class String_buffer
{
  array(string) buffer=allocate(BEGIN);
  int ptr=0;

  static void fix()
    {
      string tmp=buffer*"";
      buffer=allocate(strlen(tmp)/128+BEGIN);
      buffer[0]=tmp;
      ptr=1;
    }

  string get_buffer()
    {
      if(ptr != 1) fix();
      return buffer[0];
    }

  void append(string s)
    {
      if(ptr==sizeof(buffer)) fix();
      buffer[ptr++]=s;
    }

  mixed cast(string to)
    {
      if(to=="string") return get_buffer();
      return 0;
    }

  void flush()
    {
      buffer=allocate(BEGIN);
      ptr=0;
    }
};


// Do a fuzzy matching between two different strings and return a
// "similarity index". The higher, the closer the strings match.
static int low_fuzzymatch(string str1, string str2)
{
  string tmp1, tmp2;
  int offset, length;
  int fuzz;
  fuzz = 0;
  while(strlen(str1) && strlen(str2))
  {
    /* Now we will look for the first character of tmp1 in tmp2 */
    if((offset = search(str2, str1[0..0])) != -1)
    {
      tmp2 = str2[offset..];
      /* Ok, so we have found one character, let's check how many more */
      tmp1 = str1;
      length = 1;
      while(1)
      {
        //*(++tmp1)==*(++tmp2) && *tmp1
        if(length < strlen(tmp1) && length < strlen(tmp2) &&
           tmp1[length] == tmp2[length])
          length++;
        else
          break;
      }
      if(length >= offset)
      {
        fuzz += length;
        str1 = str1[length..];
        str2 = str2[length + offset..];
        continue;
      }
    }
    if(strlen(str1))
      str1 = str1[1..];
  }
  return fuzz;
}

//! This function compares two strings using a fuzzy matching
//! routine. The higher the resulting value, the better the strings match.
//!
//! @seealso
//! @[Array.diff()], @[Array.diff_compare_table()]
//! @[Array.diff_longest_sequence()]
//!
int(0..100) fuzzymatch(string a, string b)
{
  int fuzz;

  if(a == b)
    return 100;

  fuzz = low_fuzzymatch(a, b);
  fuzz += low_fuzzymatch(b, a);
  fuzz = fuzz*100/(strlen(a)+strlen(b));

  return [int(0..100)]fuzz;
}

//! Returns the soundex value of @[word] according to
//! the original Soundex algorithm, patented by Margaret O�Dell
//! and Robert C. Russel in 1918. The method is based on the phonetic
//! classification of sounds by how they are made.
string soundex(string word) {
  word = upper_case(word);
  string first = word[0..0];
  word = word[1..] - "A" - "E" - "H" - "I" - "O" - "U" - "W" - "Y";
  word = replace(word, ([ "B":"1", "F":"1", "P":"1", "V":"1",
			  "C":"2", "G":"2", "J":"2", "K":"2",
			  "Q":"2", "S":"2", "X":"2", "Z":"2",
			  "D":"3", "T":"3",
			  "L":"4",
			  "M":"5", "N":"5",
			  "R":"6" ]) );
  word = replace(word, ({"11", "22", "33", "44", "55", "66" }),
		 ({"", "", "", "", "", "", }));
  word+="000";
  return first + word[..2];
}

//! Converts the provided integer to a roman integer (i.e. a string).
string int2roman(int m)
{
  string res="";
  if (m>10000000||m<0) return "que";
  while (m>999) { res+="M"; m-=1000; }
  if (m>899) { res+="CM"; m-=900; }
  else if (m>499) { res+="D"; m-=500; }
  else if (m>399) { res+="CD"; m-=400; }
  while (m>99) { res+="C"; m-=100; }
  if (m>89) { res+="XC"; m-=90; }
  else if (m>49) { res+="L"; m-=50; }
  else if (m>39) { res+="XL"; m-=40; }
  while (m>9) { res+="X"; m-=10; }
  if (m>8) return res+"IX";
  else if (m>4) { res+="V"; m-=5; }
  else if (m>3) return res+"IV";
  while (m) { res+="I"; m--; }
  return res;
}

static constant prefix = ({ "bytes", "kb", "Mb", "Gb", "Tb", "Pb", "Eb", "Zb", "Yb" });

//! Returns the size as a memory size string with suffix,
//! e.g. 43210 is converted into "42.2 kb". To be correct
//! to the latest standards it should really read "42.2 KiB",
//! but we have chosen to keep the old notation for a while.
//! The function knows about the quantifiers kilo, mega, giga,
//! tera, peta, exa, zetta and yotta.
string int2size( int size )
{
  if(size<0) return "--------";
  float s = (float)size;
  size=0;

  if(s<1025.0) return (int)s+" bytes";
  while( s > 1024.0 && size<sizeof(prefix)-1)
  {
    s /= 1024.0;
    size ++;
  }
  return sprintf("%.1f %s", s, prefix[ size ]);
}

//! Expands tabs in a string to ordinary spaces, according to common
//! tabulation rules.
string expand_tabs(string s, int|void tab_width,
		   string|void substitute_tab,
		   string|void substitute_space,
		   string|void substitute_newline)
{
  string tab = substitute_tab || " ",
	 space = substitute_space || " ",
	 newline = substitute_newline || "\n";
  return map(s/"\n", line_expand_tab, tab_width||8, space, tab) * newline;
}

// the \n splitting is done in our caller for speed improvement
static string line_expand_tab(string line, int tab_width,
			      string space, string tab)
{
  string ws, chunk, result = "";
  int col, next_tab_stop, i;
  while(sizeof(line))
  {
    sscanf(line, "%[ \t\n]%[^ \t\n]%s", ws, chunk, line);
    for(i=0; i<sizeof(ws); i++)
      switch(ws[i])
      {
	case '\t':
	  next_tab_stop = col + tab_width - (col % tab_width);
	  result += tab * (next_tab_stop - col);
	  col = next_tab_stop;
	  break;

	case ' ':
	  result += space;
	  col++;
	  break;
      }
    result += chunk;
    col += sizeof(chunk);
  }
  return result;
}
