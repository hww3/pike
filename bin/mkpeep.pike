#!/usr/local/bin/pike

#pragma strict_types

/* $Id: mkpeep.pike,v 1.25 2002/11/01 16:58:38 grubba Exp $ */

#define JUMPBACK 3

string skipwhite(string s)
{
#if DEBUG > 9
  werror("skipwhite("+s+")\n");
#endif

  sscanf(s,"%*[ \t\n]%s",s);
  return s;
}

string stripwhite(string s)
{
  sscanf(s,"%*[ \t\n]%s",s);
  s=reverse(s);
  sscanf(s,"%*[ \t\n]%s",s);
  return reverse(s);
}

/* Find the matching parenthesis */
int find_end(string s)
{
  int e,parlvl=1;

#if DEBUG > 8
  werror("find_end("+s+")\n");
#endif
  
  for(e=1;e<strlen(s);e++)
  {
    switch(s[e])
    {
    case '(': case '{': case '[':
      parlvl++; break;
    case ')': case '}': case ']':
      parlvl--;
      if(!parlvl) return e;
      break;
    case '"':
      while(s[e]!='"') e+=1+(s[e]=='\\');
      break;
    }
  }
  werror("Syntax error (1).\n");
  exit(1);
}

array(string) explode_comma_expr(string s)
{
#if DEBUG>4
  werror("Exploding %O\n",s);
#endif
  int parlvl;
  array(string) ret=({});
  int begin=0;

  for(int e=0;e<strlen(s);e++)
  {
    switch(s[e])
    {
    case '(': case '{': case '[':
      parlvl++; break;
    case ')': case '}': case ']':
      parlvl--; break;
    case '"':
      while(s[e]!='"') e+=1+(s[e]=='\\');
      break;

    case ',':
      if(!parlvl)
      {
	ret+=({ stripwhite(s[begin..e-1]) });
	begin=e+1;
      }
    }
  }

  /* Ignore empty last arguments */
  if(strlen(stripwhite(s[begin..])))
    ret+=({ stripwhite(s[begin..]) });
#if DEBUG>4
  werror("RESULT: %O\n",ret);
#endif
  return ret;
}


/* Splitline into components */
array(int|string|array(string)) split(string s)
{
  array(string) a, b;
  string tmp;
  int e,opcodes;
  string line=s;
  opcodes=0;

#ifdef DEBUG
  werror("split("+s+")\n");
#endif

  b=({});

  s=skipwhite(s);
  
  /* First, we tokenize */
  while(strlen(s))
  {
    switch(s[0])
    {
      /* Source / Target separator */
    case ':':
      b+=({":"});
      s=s[1..];
      break;

    case '!':
      b+=({"!"});
      s=s[1..];
      break;

      // Any identifier (i.e. not eof).
    case '?':
      b+=({"?"});
      s=s[1..];
      break;

      /* Identifier */
    case 'A'..'Z':
    case 'a'..'z':
    case '0'..'9':
    case '_':
      sscanf(s,"%[a-zA-Z0-9_]%s",tmp,s);
      b+=({"F_"+tmp});
      break;

      /* argument */
    case '(':
      {
	int i=find_end(s);
	b+=({ s[0..i] });
	s=s[i+1..strlen(s)];
      }
      break;

      /* condition */
    case '[':
      {
	int i=find_end(s);
	b+=({ s[0..i] });
	s=s[i+1..strlen(s)];
      }
      break;
    }

    s=skipwhite(s);
  }

  /* Find the source/dest separator */
  int i=search(b, ":");
  if(i==-1)
  {
    werror("Syntax error (%O).\n",b);
    return 0;
  }

  /* a=source, b=dest */
  a=b[..i-1];
  b=b[i+1..];

  /* Count 'steps' in source */
  for(e=0;e<sizeof(a);e++)
    if((<'F', '?'>)[a[e][0]])
      opcodes++;

#if 0
  /* It was a good idea, but it doesn't work */
  mixed qqqq=copy_value(b);
  i=0;
  while(sizeof(b))
  {
    if(b[0] != a[i])
      break;
    
    if(sizeof(b)>1 && b[1][0]!='F')
    {
      if(b[1] != sprintf("($%da)",i+1))
	break;
      b=b[2..];
    }else{
      b=b[1..];
    }
    i++;
    opcodes--;
  }
  if(i)
    werror("----------------------\n%d\n%O\n%O\n%O\n",opcodes,a,b,qqqq);
#endif

  i=0;
  array(string) newa=({});
  for(e=0;e<sizeof(a);e++)
  {
    switch(a[e][0])
    {
      case '(':
	array(string) tmp=explode_comma_expr(a[e][1..strlen(a[e])-2]);
	for(int x=0;x<sizeof(tmp);x++)
	{
	  string arg=sprintf("$%d%c", i, 'a'+x);
	  if(arg != tmp[x] && strlen(tmp[x]))
	    newa+=({ sprintf("(%s)==%s",tmp[x], arg) });
	}
      break;

    case '[':
      newa+=({ a[e][1..strlen(a[e])-2] });
      break;

    case 'F':
      i++;
      newa+=({ a[e]+"==$"+i+"o" });
      break;

    case '?':
      i++;
      newa += ({"$" + i + "o != -1"});
      break;

      default: newa+=({a[e]});
    }
  }
  a=newa;

  // Magic for the '!' operator
  for(e=0;e<sizeof(a);e++)
  {
    if(a[e]=="!")
    {
      sscanf(a[e+1],"%s==%s",string op, string arg);
      a[e+1]=sprintf("%s != %s && %s !=-1",op,arg,arg);
      a=a[..e-1]+a[e+1..];
      e--;
    }
  }

  return ({a,b,opcodes, line,a});
}

/* Replace $[0-9]+(o|a|b) with something a C compiler can understand */
string treat(string expr)
{
  int e;
  array(string) tmp;
  tmp=expr/"$";
  for(e=1;e<sizeof(tmp);e++)
  {
    string num, rest;
    int type;
    if(sscanf(tmp[e],"%d%c%s",num,type,rest)!=3)
    {
      werror("Syntax error (3).\n");
      exit(2);
    }
    num--;
    switch(type)
    {
    case 'a': tmp[e]="argument("+num+")"+rest; break;
    case 'b': tmp[e]="argument2("+num+")"+rest; break;
    case 'o': tmp[e]="opcode("+num+")"+rest; break;
    }
  }
  return tmp*"";
}

/* Dump C co(d|r)e */
void dump2(array(array(array(string))) data,int ind)
{
  int e,i,max,maxe;
  mixed tmp;
  string test;
  mapping(string:mapping(string:array(array(array(string))))) foo;
  mixed cons, var;

  foo=([]);

  while(1)
  {
    foo=([]);

    /* First we create a mapping:
     * foo [ meta variable ] [ condition ] = ({ lines });
     */
    foreach(data, array(array(string)) d)
    {
      array(string) a = d[0];
      array(string) b = d[1];
      for(e=0;e<sizeof(a);e++)
      {
	if(sscanf(a[e],"F_%[A-Z0-9_]==%s",cons,var)==2 ||
	   sscanf(a[e],"(%d)==%s",cons,var)==2 ||
	   sscanf(a[e],"%d==%s",cons,var)==2)
	{
	  if(!foo[var]) foo[var]=([]);
	  if(!foo[var][a[e]]) foo[var][a[e]]=({});
	  foo[var][a[e]]+=({d});
	}
      }
    }

    /* Check what variable has most values */
    max=maxe=e=0; 
    foreach(values(foo), mapping(string:array(array(array(string)))) d)
    {
      if(sizeof(d)>max)
      {
	max=sizeof(d);
	maxe=e;
      }
      e++;
    }

    /* If zero, done */
    if(max <= 1) break;

    test=indices(foo)[maxe];
    
    write(sprintf("%*nswitch(%s)\n",ind,treat(test)));
    write(sprintf("%*n{\n",ind));

    mapping(string:array(array(array(string)))) d = values(foo)[maxe];
    array(string) a = indices(d);
    array(array(array(array(string)))) b = values(d);


    /* foo: variable
     * a[x] : condition
     * b[x] : line
     */

    for(e=0;e<sizeof(a);e++)
    {
      /* The lines b[e] are removed from data as they
       * will be treated below
       */
      data-=b[e];

      if(sscanf(a[e],"(%s)==%s",cons,var)!=2)
	sscanf(a[e],"%s==%s",cons,var);
      
      write(sprintf("%*ncase %s:\n",ind,cons+""));

      foreach(b[e], array(array(string)) d) d[0]-=({a[e]});
      dump2(b[e],ind+2);
      write(sprintf("%*n  break;\n",ind));
      write("\n");
    }

    write(sprintf("%*n}\n",ind));
  }
  
  /* Take care of whatever is left */
  if(sizeof(data))
  {
    foreach(data, array(array(string)) d)
    {
      write(sprintf("%*n/* %s */\n",ind,d[3]));
      
      if(sizeof(d[0]))
      {
	string test;
	test=treat(d[0]*" && ");
	write(sprintf("%*nif(%s)\n",ind,test));
      }
      write(sprintf("%*n{\n",ind));
      ind+=2;
      write("%*ndo_optimization(%d,\n",ind,d[2]);

      for(i=0;i<sizeof(d[1]);i++)
      {
	array args=({});
	string fcode=d[1][i];
	if(i+1<sizeof(d[1]) && d[1][i+1][0]=='(')
	{
	  string tmp=d[1][i+1];
	  args=explode_comma_expr(tmp[1..strlen(tmp)-2]);
	  i++;
	}
	write("%*n                %d,%s,%{(%s), %}\n",
	      ind,
	      sizeof(args)+1,
	      fcode,
	      Array.map(args,treat));

      }
      write("%*n                0);\n",ind);

      write(sprintf("%*ncontinue;\n",ind));
      ind-=2;
      write(sprintf("%*n}\n",ind,test));
    }
  }
}
  


int main(int argc, array(string) argv)
{
  int e,max,maxe;
  string f;
  mapping foo=([]);
  array(array(array(string))) data=({});

  mapping tests=([]);

  /* Read input file */
  f=cpp(Stdio.read_bytes(argv[1]),argv[1]);
  foreach(f/"\n",f)
  {
    array(string) a, b;
    mapping tmp;

    sscanf(f,"%s#",f);

    /* Parse expressions */
    foreach(f/";",f)
      {
	f=skipwhite(f);
	if(!strlen(f)) continue;
	data+=({split(f)});
      }
  }

//  write(sprintf("%O\n",data));

  write("  len=instrbuf.s.len/sizeof(p_instr);\n"
	"  instructions=(p_instr *)instrbuf.s.str;\n"
	"  instrbuf.s.str=0;\n"
	"  fifo_len=0;\n"
	"  init_bytecode();\n\n"
	"  for(eye=0;eye<len || fifo_len;)\n  {\n"
	"\n"
	"#ifdef PIKE_DEBUG\n"
	"    if(a_flag>6) {\n"
	"      int e;\n"
	"      fprintf(stderr, \"#%ld,%d:\",\n"
	"              DO_NOT_WARN((long)eye),\n"
	"              fifo_len);\n"
	"      for(e=0;e<4;e++) {\n"
	"        fprintf(stderr,\" \");\n"
	"        dump_instr(instr(e));\n"
	"      }\n"
	"      fprintf(stderr,\"\\n\");\n"
	"    }\n"
	"#endif\n\n");

  dump2(data,4);

  write("    advance();\n");
  write("  }\n");
  write("  for(eye=0;eye<len;eye++) free_string(instructions[eye].file);\n");
  write("  free((char *)instructions);\n");

  return 0;
}

