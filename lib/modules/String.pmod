#pike __REAL_VERSION__

#define BEGIN 32


constant Buffer = __builtin.Buffer;

//! @decl string count(string haystack, string needle)
//!
//! This function counts the number of times the @[needle]
//! can be found in @[haystack]. 
//!
//! @note
//! Intersections between needles are not counted, ie
//! @tt{count("....","..")@} is @tt{2@}.
//!
constant count=__builtin.string_count;

//! @decl int width(string s)
//!
//! Returns the width in bits (8, 16 or 32) of the widest character
//! in @[s].
//!
constant width=__builtin.string_width;

/*
 * Implode an array of strings to an english 'list'
 * ie. ({"foo","bar","gazonk"}) becomes "foo, bar and gazonk"
 */

//! This function implodes a list of words to a readable string.
//! If the separator is omitted, the default is <tt>"and"</tt>.
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

//! This function multiplies @[s] by @[num]. The return value is the same
//! as appending @[s] to an empty string @[num] times.
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

class String_buffer {
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
int fuzzymatch(string a, string b)
{
  int fuzz;

  if(a == b)
  {
    fuzz = 100;
  } else {
    fuzz = low_fuzzymatch(a, b);
    fuzz += low_fuzzymatch(b, a);
    fuzz = fuzz*100/(strlen(a)+strlen(b));
  }

  return fuzz;
}

//! Trim leading and trailing spaces and tabs from the string @[s].
//!
string trim_whites(string s)
{
  if (stringp(s)) {
    sscanf(s, "%*[ \t]%s", s);
    string rev = reverse(s);
    sscanf(rev, "%*[ \t]%s", rev);
    return s[..strlen(rev) - 1];
  }

  return s;
}

//! Trim leading and trailing white spaces characters (@tt{" \t\r\n"@}) from
//! the string @[s].
//!
string trim_all_whites(string s)
{
  if (stringp(s)) {
    sscanf(s, "%*[ \t\r\n]%s", s);
    string rev = reverse(s);
    sscanf(rev, "%*[ \t\r\n]%s", rev);
    return s[..strlen(rev) - 1];
  }

  return s;
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
