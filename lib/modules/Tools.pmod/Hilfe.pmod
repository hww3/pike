#pike __REAL_VERSION__

// Incremental Pike Evaluator
//
// $Id: Hilfe.pmod,v 1.57 2002/03/26 23:46:54 nilsson Exp $

constant hilfe_todo = #"List of known Hilfe bugs/room for improvements:

- Hilfe can not handle sscanf statements like
  int a = sscanf(\"12\", \"%d\", int b);
- Hilfe can not handle enums.
- Hilfe can not handle typedefs.
- Hilfe can not handle generated types, e.g.
  constant boolean = typeof(0)|typeof(1); boolean flag = 1;
- Hilfe should possibly handle imports better, e.g. overwrite the
  local variables/constants/functions/programs.
- Some preprocessor stuff works. Some doesn't. They should be
  reviewed and fixed where possible.
- Filter exit/quit from history. Could be done by adding a 'pop'
  method to Readline.History and calling it from StdinHilfe's
  destroy.
- Add some better multiline edit support.
- Tab completion of variable and module names.
";

//! Abstract class for Hilfe commands.
class Command {

  //! Returns a one line description of the command. This help should
  //! be shorter than 54 characters.
  string help();

  //! A more elaborate documentation of the command. This should be
  //! less than 68 characters per line.
  string doc(string with) { return help(); }

  //! The actual command callback. Messages to the user should be
  //! written out by using the write method in the @[Evaluator] object.
  void exec(Evaluator e, string line, array(string) words, array(string) tokens);
}

//! Variable reset command. Put ___Hilfe->commands->reset = Tools.Hilfe.CommandReset();
//! in your .hilferc to have this command defined when you open Hilfe.
class CommandReset {
  inherit Command;
  string help() { return "Undefines the given symbol."; }
  string doc() {
    return "Undefines any variable, constant, function or program, specified by\n"
      "name. Example: \"reset tmp\"\n";
  }

  void exec(Evaluator e, string line, array(string) words) {
    string n = sizeof(words)>1 && words[1];
    if(!n) {
      e->write("No symbol given as argument to reset.\n");
      return;
    }
    if(zero_type(e->variables[n]) && zero_type(e->constants[n]) &&
       zero_type(e->functions[n]) && zero_type(e->programs[n])) {
      e->write("Symbol %O not defined.\n", n);
      return;
    }

    m_delete(e->variables, n);
    m_delete(e->types, n);
    m_delete(e->constants, n);
    m_delete(e->functions, n);
    m_delete(e->programs, n);
  }
}

//! Helper function that formats a time span in nanoseconds to
//! something more human readable (ns, ms or s).
string format_hr_time(int i) {
  if(i<1000) return i+"ns";
  if(i<1000000) return sprintf("%.2fms", i/1000.0);
  return sprintf("%.2fs", i/1000000.0);
}


//
// Built in commands
//

private class CommandSet {
  inherit Command;

  string help() { return "Change Hilfe settings."; }
  string doc(string with) {
    if(with=="format")
      return documentation_set_format;
    return documentation_set;
  }

  private void bench_reswrite(function w, string sres, int num, mixed res,
			      int last_compile_time, int last_eval_time) {
    w( "Result %d: %s\nCompilation: %s, Execution: %s\n",
       num, replace(sres, "\n", "\n        "+(" "*sizeof(""+num))),
       format_hr_time(last_compile_time),
       format_hr_time(last_eval_time) );
  }

  private class Reswriter (string format) {
    void `()(function w, string sres, int num, mixed res,
	     int last_compile_time, int last_eval_time) {
      mixed err = catch {
	w(format, sres, num, res,
	  format_hr_time(last_compile_time),
	  format_hr_time(last_eval_time),
	  last_compile_time, last_eval_time);
      };
      if(err)
	w("Hilfe Error: Could not format result.\n%s\n",
	  describe_backtrace(err));
    }
  }

  void exec(Evaluator e, string line, array(string) words, array(string) tokens) {

    line = sizeof(words)>1 && words[1];
    function write = e->write;

    if(!line) {
      write("No setting to change given.\n");
      return;
    }

    int(0..1) arg_check(string arg) {
      if(line!=arg) return 0;
      if(sizeof(words)<3) {
	write("Insufficient number of arguments to set %s.\n", line);
	line = "";
	return 0;
      }
      return 1;
    };

    if(arg_check("trace")) {
      e->trace_level = (int)words[2];
      return;
    }

    if(arg_check("assembler_debug")) {
#if constant(_assembler_debug)
      e->assembler_debug_level = (int)words[2];
#else
      write("Assembler debug not available.\n");
#endif
      return;
    }

    if(arg_check("compiler_trace")) {
#if constant(_compiler_trace)
      e->compiler_trace_level = (int)words[2];
#else
      write("Compiler trace not available.\n");
#endif
      return;
    }

    if(arg_check("debug")) {
#if constant(_debug)
      e->debug_level = (int)words[2];
#else
      write("Debug not available.\n");
#endif
      return;
    }

    if(arg_check("history")) {
      e->history->set_maxsize((int)words[2]);
      return;
    }

    if(arg_check("format")) {
      switch(words[2]) {
      case "default":
	e->reswrite = e->std_reswrite;
	return;
      case "bench":
	e->reswrite = bench_reswrite;
	return;
      case "sprintf":
	string f;
	foreach(tokens, string token)
	  if(token[0]=='"') f=token;
	if(!f)
	  write("No formatting string given.\n");
	else {
	  // FIXME: We should do a real string compilation.
	  f = replace(f, ([ "\\n":"\n", "\\\"":"\"" ]) );
	  e->reswrite = Reswriter(f[1..sizeof(f)-2]);
	}
	return;
      }
      write("No result presentation format %O defined.\n", words[2]);
      return;
    }

    if(line=="") return;
    write("No setting named %O exists.\n", line);
    // Prompt (>/>>)
  }
}

private class CommandExit {
  inherit Command;
  string help() { return "Exit Hilfe."; }

  void exec(Evaluator e) {
    e->write("Exiting.\n");
    destruct(e);
    exit(0);
  }
}

private class CommandHelp {
  inherit Command;
  string help() { return "Show help text."; }

  void exec(Evaluator e, string line, array(string) words) {
    line = words[1..]*" ";
    function write = e->write;

    if(line == "me more") {
      write( documentation_help_me_more );
      return;
    }

    if(line == "hilfe todo") {
      write(hilfe_todo);
      return;
    }

    if(sizeof(words)>1 && e->commands[words[1]]) {
      string ret = e->commands[words[1]]->doc(words[2..]*"");
      if(ret) write(ret);
      return;
    }

    write("\n");
    e->print_version();
    write( #"Hilfe is a tool to evaluate Pike code interactively and
incrementally. Any Pike function, expression or variable declaration
can be entered at the command line. There are also a few extra
commands:

");

    array err = ({});
    foreach(sort(indices(e->commands)), string cmd) {
      string ret;
      err += ({ catch( ret = e->commands[cmd]->help() ) });
      if(ret)
	write(" %-10s - %s\n", cmd, ret);
    }

    write( #" .          - Abort current input batch.

Enter \"help me more\" for further Hilfe help.
");

    foreach(err-({0}), mixed err)
      write(describe_backtrace(err)+"\n\n");
  }
}

private class CommandDot {
  inherit Command;
  string help() { return 0; }

  private constant usr_vector_a = ({
    89, 111, 117, 32, 97, 114, 101, 32, 105, 110, 115, 105, 100, 101, 32, 97, 32,
    72, 105, 108, 102, 101, 46, 32, 73, 116, 32, 115, 109, 101, 108, 108, 115, 32,
    103, 111, 111, 100, 32, 104, 101, 114, 101, 46, 32, 89, 111, 117, 32, 115, 101, 101, 32 });
  private constant usr_vector_b = ({
    32, 89, 111, 117, 32, 99, 97, 110, 32, 103, 111, 32, 105, 110, 32, 97, 110, 121,
    32, 100, 105, 114, 101, 99, 116, 105, 111, 110, 32, 102, 114, 111, 109, 32, 104,
    101, 114, 101, 46 });
  private constant usr_vector_c = ({
    32, 89, 111, 117, 32, 97, 114, 101, 32, 99, 97, 114, 114, 121, 105, 110, 103, 32 });
  private constant usr_vector_d = usr_vector_c[..8] + ({
    101, 109, 112, 116, 121, 32, 104, 97, 110, 100, 101, 100, 46 });

  private array(string) thing(mixed thing, string what, void|string a, void|string b) {
    if(!sizeof(thing)) return ({});
    return ({ sizeof(thing)+" "+what+(sizeof(thing)==1?(a||""):(b||"s")) });
  }

  void exec(Evaluator e) {
    string ret = (string)usr_vector_a;

    array tmp = ({});
    tmp += thing(e->imports, "import");
    tmp += thing(e->inherits, "inherit");
    tmp += thing(e->history, "history entr", "y", "ies");
    if(sizeof(tmp))
      ret += String.implode_nicely(tmp) + ".";
    else
      ret += "nothing.";

    tmp = ({});
    tmp += thing(e->variables, "variable");
    tmp += thing(e->constants, "constant");
    tmp += thing(e->functions, "function");
    tmp += thing(e->programs, "program");
    if(sizeof(tmp))
      ret += (string)usr_vector_c + String.implode_nicely(tmp) + ".";
    else
      ret += (string)usr_vector_d;

    ret += (string)usr_vector_b;
    e->write("%-=67s\n", ret);
  }
}

class CommandDump {
  inherit Command;

  private function write;

  string help() { return "Dump variables and other info."; }
  string doc() { return documentation_dump; }

  private void wrapper(Evaluator e) {
    if(!e->last_compiled_expr) {
      write("No wrapper compiled so far.\n");
      return;
    }
    write("Last compiled wrapper:\n");
    int i;
    string w = map(e->last_compiled_expr/"\n",
		   lambda(string in) {
		     return sprintf("%03d: %s", ++i, in);
		   })*"\n";
    write(w+"\n");
  }

  private string print_mapping(array(string) ind, array val) {
    int m = max( @filter(map(ind, sizeof), `<, 20), 8 );
    int i;
    foreach(ind, string name)
      write("%-*s : %s\n", m, name, replace(sprintf("%O", val[i++]), "\n", "\n"+(" "*m)+"   "));
  }

  private void dump(Evaluator e) {
    if(sizeof(e->constants)) {
      write("\nConstants:\n");
      array a=indices(e->constants), b=values(e->constants);
      sort(a,b);
      print_mapping(a,b);
    }
    if(sizeof(e->variables)) {
      write("\nVariables:\n");
      array a=indices(e->variables), b=values(e->variables);
      sort(a,b);
      a = map(a, lambda(string in) { return e->types[in] + " " + in; } );
      print_mapping(a,b);
    }
    if(sizeof(e->functions)) {
      write("\nFunctions:\n");
      foreach(sort(indices(e->functions)), string name)
	write("%s\n", name);
    }
    if(sizeof(e->programs)) {
      write("\nPrograms:\n");
      foreach(sort(indices(e->programs)), string name)
	write("%s\n", name);
    }
    if(sizeof(e->inherits)) {
      write("\nInherits:\n" + e->inherits*"\n" + "\n");
    }
    if(sizeof(e->imports)) {
      write("\nImports:\n" + e->imports*"\n" + "\n");
    }
  }

  void exec(Evaluator e, string line, array(string) words) {
    write = e->write;

    line = words[1..]*"";
    switch( line ) {
    case "wrapper":
      wrapper(e);
      return;
    case "state":
      write(e->state->status());
      return;
    case "history":
      write(e->history->status());
      return;
    case "":
      dump(e);
      return;
    }
    write("Unknown dump specifier.\n");
    write(doc()+"\n");
  }
}

private class CommandHej {
  inherit Command;
  string help() { return 0; }
  void exec(Evaluator e, string line) {
    if(line[0]=='.') e->write( (string)({ 84,106,97,98,97,33,10 }) );
  }
}

private class CommandNew {
  inherit Command;
  string help() { return "Clears the Hilfe state."; }
  string doc() { return documentation_new; }

 void exec(Evaluator e, string line, array(string) words) {

   line = sizeof(words)>1 && words[1];
   switch(line) {
   case "variables":
     e->variables = ([]);
     e->types = ([]);
     return;
   case "constants":
     e->constants = ([]);
     return;
   case "functions":
     e->functions = ([]);
     return;
   case "programs":
     e->programs = ([]);
     return;
   case "imports":
     e->imports = ({});
     return;
   case "inherits":
     e->inherits = ({});
     return;
   case "history":
     e->history->flush();
     return;
   }
   if(line) {
     e->write("Unknown specifier %O.\n", line);
     return;
   }
   e->reset_evaluator();
 }
}


//
// Support stuff..
//

private constant whitespace = (< ' ', '\n' ,'\r', '\t' >);
private constant termblock = (< "catch", "do", "gauge", "lambda", "class stop" >);
private constant modifier = (< "extern", "final", "inline", "local", "nomask",
			       "optional", "private", "protected", "public",
			       "static", "variant" >);

private class Expression {
  private array(string) tokens;
  private mapping(int:int) positions;

  void create(array(string) t) {
    tokens = t;
    positions = ([]);
    int pos;
    for(int i; i<sizeof(t); i++) {
      if(whitespace[t[i][0]]) continue;
      positions[pos++] = i;
    }
  }

  int _sizeof() {
    return sizeof(positions);
  }

  // We do not test for out of boundary indexing here...
  // Returns a token or a token range without whitespaces.
  string `[](int f, void|int t) {
    if(!t)
      return tokens[positions[f]];
    if(t>=sizeof(positions))
      t = sizeof(positions)-1;

    return tokens[positions[f]..positions[t]]*"";
  }

  string `[]= (int f, string v) {
    return tokens[positions[f]] = v;
  }

  // See if there are any forbidden modifiers used in the expression,
  // e.g. "private int x;" is not valid inside Hilfe.
  string check_modifiers() {
    foreach(sort(values(positions)), int pos)
      if(modifier[tokens[pos]])
	return "Hilfe Error: Modifier \"" + tokens[pos] +
	  "\" not allowed on top level in Hilfe.\n";
      else
	return 0;
    return 0;
  }

  // Returns the expression verbatim.
  string code() {
    return tokens*"";
  }

  // Returns the first complex entity in the expression,
  // e.g. ({ "Stdio", ".", "File" }) is returned from
  // ({ "Stdio", ".", "File", " ", "f", ";" }).
  array(string) first_complex() {
    int p = search(tokens, " ");
    if(p==-1) p = sizeof(tokens)-1;
    return tokens[..p];
  }

  string _sprintf(int t) {
    if(t=='O') return sprintf("Expression(%O)", tokens);
    if(t=='t') return sprintf("object(Expression)");
    error("Can't display Expression as %c\n", t);
  }
}

//! In every Hilfe object (@[Evaluator]) there is a ParserState object
//! that manages the current state of the parser. Essentially tokens are
//! entered in one end and complete expressions are outputted in the other.
//! The parser object is accessible as ___Hilfe->state from Hilfe expressions.
private class ParserState {
  private ADT.Stack pstack = ADT.Stack();
  private constant starts = ([ ")":"(", "}":"{", "]":"[",
			       ">)":"(<", "})":"({", "])":"([" ]);
  private array(string) pipeline = ({ });
  private array(Expression) ready = ({ });
  private string last;
  private string block;

  private mapping low_state = ([]);

  //! Feed more tokens into the state.
  void feed(array(string) tokens) {
    foreach(tokens, string token) {
      if(sizeof(token)>1 && (token[0..1]=="//" || token[0..1]=="/*")) continue; // comments

      // If we start a block at the uppermost level, what kind of block is it?
      if(token=="{" && !pstack->ptr && !block)
	block = last;
      if(token=="lambda" && !pstack->ptr)
	block = token;
      if(token=="class" && !pstack->ptr) {
	if(sizeof(pipeline))
	  block = "class stop"; // Kludge to get "object foo=class{}();" past the kludge below.
	else
	  block = token; // Kludge to get "class A {}" to work without semicolon.
      }

      // Do we begin any kind of parenthesis level?
      if(token=="(" || token=="{" || token=="[" ||
	 token=="(<" || token=="({" || token=="([")
	pstack->push(token);

      // Do we end any kind of parenthesis level?
      if(token==")" || token=="}" || token=="]" ||
	 token==">)" || token=="})" || token=="])" ) {
	if(!pstack->ptr)
	   throw(sprintf("%O end parenthesis without start parenthesis.", token));
	if(pstack->top()!=starts[token])
	   throw(sprintf("%O end parenthesis does not match closest start parenthesis %O.",
			 token, pstack->top()));
	pstack->pop();
      }

      pipeline += ({ token });

      // expressions
      if(token==";" && !pstack->ptr) {
	ready += ({ Expression(pipeline) });
	pipeline = ({});
      }

      // If we end a block at the uppermost level, and it doesn't need a ";",
      // then we can move out that block of the pipeline.
      if(token=="}" && !pstack->ptr && !termblock[block]) {
	ready += ({ Expression(pipeline) });
	pipeline = ({});
	block = 0;
      }

      if(!whitespace[token[0]])
	last = token;
    }
  }

  //! Read out completed expressions. Returns an array where every element
  //! is an expression represented as an array of tokens.
  array(Expression) read() {
    array(Expression) ret = ({});

    foreach(ready, Expression expr)
      if(expr[0]!=";")
	ret += ({ expr });

    ready = ({});
    return ret;
  }

  private string caught_error;

  //! Prints out any error that might have occured while
  //! @[push_string] was executed. The error will be
  //! printed with the print function @[w].
  void show_error(function w) {
    if(!error) return;
    w("Hilfe Error: %s", caught_error);
    caught_error = 0;
  }

  //! Sends the input @[line] to @[Parser.Pike] for tokenization,
  //! but keeps a state between each call to handle multiline
  //! /**/ comments and multiline #"" strings.
  array(string) push_string(string line) {
    array(string) tokens;
    mixed err;
    if(err = catch( tokens = Parser.Pike.split(line, low_state) )) {
      caught_error = err[0];
      return 0;
    }
    return tokens;
  }

  //! Returns true if there is any waiting expression that can be fetched
  //! with @[read].
  int datap() {
    if(sizeof(pipeline)==1 && whitespace[pipeline[0][0]])
      pipeline = ({});
    return sizeof(ready);
  }

  //! Are we in the middle of an expression. Used e.g. for changing the
  //! Hilfe prompt when entering multiline expressions.
  int(0..1) finishedp() {
    if(pstack->ptr) return 0;
    if(low_state->in_token) return 0;
    if(!sizeof(pipeline)) return 1;
    if(sizeof(pipeline)==1 && whitespace[pipeline[0][0]]) {
      pipeline = ({});
      return 1;
    }
    return 0;
  }

  //! Clear the current state.
  void flush() {
    pstack->reset();
    pipeline = ({});
    ready = ({});
    last = 0;
    block = 0;
    low_state = ([]);
  }

  //! Returns the current parser state. Used by "dump state".
  string status() {
    string ret = "Current parser state\n";
    ret += sprintf("Parenthesis stack: %s\n", pstack->arr[..pstack->ptr]*" ");
    ret += sprintf("Current pipeline: %O\n", pipeline);
    ret += sprintf("Last token: %O\n", last);
    ret += sprintf("Current block: %O\n", block);
    return ret;
  }

  string _sprintf(int type) {
    if(type=='O' || type=='t') return "HilfeParserState";
  }
}

//! In every Hilfe object (@[Evaluator]) there is a HilfeHistory
//! object that manages the result history. That history object is
//! accessible both from __ and ___Hilfe->history in Hilfe expressions.
private class HilfeHistory {

  inherit ADT.History;

  // Add content overview
  string status() {
    string ret = "";
    int abs_num = get_first_entry_num();
    int rel_num = -_sizeof();
    for(abs_num; abs_num<get_latest_entry_num()+1; abs_num++)
      ret += sprintf(" %2d (%2d) : %s\n", abs_num, rel_num++,
		     replace(sprintf("%O", `[](abs_num)), "\n",
			     "\n           "));
    ret += sprintf("%d out of %d possible entries used.\n",
		   _sizeof(), get_maxsize());
    return ret;
  }

  // Give better names in backtraces.
  mixed `[](int i) {
    mixed ret;
    array err = catch( ret = ::`[](i) );
    if(err)
      error(err[0]);
    return ret;
  }

  // Give the object a better name.
  string _sprintf(int t) {
    if(t=='O') return "HilfeHistory("+_sizeof()+"/"+get_maxsize()+")";
    if(t=='t') return "HilfeHistory";
    error("Can't print History object as '%c'.\n", t);
  }
}


//
// The actual Hilfe
//

//! This class implements the actual Hilfe interpreter. It is accessible
//! as ___Hilfe from Hilfe expressions.
class Evaluator {

  //! This mapping contains the available Hilfe commands, including the
  //! built in ones (dump, exit, help, new, quit), so it is possible to
  //! replace or remove them. The name of a command should be 10
  //! characters or less.
  mapping(string:Command) commands = ([]);

  //! Keeps the state, e.g. multiline input in process etc.
  ParserState state = ParserState();

  //! The locally defined variables (name:value).
  mapping(string:mixed) variables;

  //! The types of the locally defined variables (name:type).
  mapping(string:string) types;

  //! The locally defined constants (name:value).
  mapping(string:mixed) constants;

  //! The locally defined functions (name:value).
  mapping(string:function) functions;

  //! The locally defined programs (name:value).
  mapping(string:program) programs;

  //! The current imports.
  array(string) imports;

  //! The current inherits.
  array(string) inherits;

  //! The current result history.
  HilfeHistory history = HilfeHistory(10);

  //! The function to use when writing this to the user.
  function write;

  //!
  void create()
  {
    if(write) print_version();
    commands->set = CommandSet();
    commands->exit = CommandExit();
    commands->quit = CommandExit();
    commands->help = CommandHelp();
    commands->dump = CommandDump();
    commands->new = CommandNew();
    commands->hej = CommandHej();
    commands->look = CommandDot();
    reset_evaluator();
  }

  //! Displays the current version of Hilfe.
  void print_version()
  {
    write(version()+
	  " running Hilfe v3.2 (Incremental Pike Frontend)\n");
  }

  //! Clears the current state, history and removes all locally
  //! defined variables, constants, functions and programs. Removes
  //! all imports and inherits. It does not reset the command mapping
  //! nor reevaluate the .hilferc file.
  void reset_evaluator() {
    state->flush();
    history->flush();
    variables = ([]);
    types = ([]);
    constants = ([]);
    functions = ([]);
    programs = ([]);
    imports = ({});
    inherits = ({});
  }

  //! Input a line of text into Hilfe. It checks if @[s] is
  //! ".", in which case it calls state->flush(). Otherwise
  //! just calls add_buffer.
  void add_input_line(string s)
  {
    if(s==".")
    {
      state->flush();
      return;
    }

    add_buffer(s);
  }

  //! Add buffer tokenizes the input string and determines if the
  //! new line is a Hilfe command. If not, it updates the current
  //! state with the new tokens and sends any and all complete
  //! expressions to evaluation in @[parse_expression].
  void add_buffer(string s)
  {
    // Tokenize the input
    array tokens = state->push_string(s);
    array words = s/" ";

    // See if first token is a command and not a defined entity.
    if(commands[words[0]] && zero_type(constants[words[0]]) &&
       zero_type(variables[words[0]]) && zero_type(functions[words[0]]) &&
       (sizeof(words)==1 || words[1]!=";")) {
      commands[words[0]]->exec(this_object(), s, words, tokens);
      return;
    }

    // See if the command is executed in overridden mode.
    if(commands[words[0][1..]]) {
      commands[words[0][1..]]->exec(this_object(), s, words, tokens);
      return;
    }

    if(!tokens) {
      state->show_error(write);
      return;
    }

    // Push new tokens into our state.
    string err = catch( state->feed(tokens) );
    if(err) {
      if(stringp(err))
	write("Hilfe Error: %s\n", err);
      else
	write(describe_backtrace(err));
      state->flush();
    }

    // See if any complete expressions came out on the other side.
    if(state->datap())
      foreach(state->read(), Expression expression) {
	string|int ret = parse_expression(expression);
	if(ret) write(ret);
      }
  }


  //
  //
  // Parser code
  //
  //

  private int(0..1) hilfe_error(mixed err) {
    if(!err) return 1;
    mixed err2 = catch {
      if( (arrayp(err) && sizeof(err)==2 && stringp(err[0])) ||
	  (objectp(err) && err->backtrace) ) {
	array files = map(reverse(err[1]), lambda(mixed in) {
					  if(in) return in[0];
					  return 0;
					});
	int pos = search(files, "HilfeInput");
	write(describe_backtrace( ({ err[0], err[1][sizeof(err[1])-pos..] }) ));
     } else
	write("Hilfe Error: Unknown format of thrown error (not backtrace).\n(%O)\n", err);
    };
    if(err2)
      write("Hilfe Error: Error while printing backtrace.\n");
    return 0;
  }

  private void add_hilfe_constant(string code, string var) {
    if(object o = hilfe_compile("constant " + code +
				";\nmixed ___HilfeWrapper() { return " +
				var + "; }", var)) {
      hilfe_error( catch( constants[var] = o->___HilfeWrapper() ) );
    }
  }

  private void add_hilfe_variable(string type, string code, string var) {
    int(0..1) existed;
    mixed old_value;
    if(!zero_type(variables[var])) {
      old_value = m_delete(variables, var);
      existed = 1;
    }

    object o = hilfe_compile(type + " " + code +
			     ";\nmixed ___HilfeWrapper() { return " +
			     var + "; }", var);

    if(	o && hilfe_error( catch( variables[var] =
				 o->___HilfeWrapper() ) ) ) {
      types[var] = type;
    }
    else if(existed)
      variables[var] = old_value;
  }

  private void add_hilfe_entity(string type, string code,
				string var, mapping vtype) {
    int(0..1) existed;
    mixed old_value;
    if(vtype[var]) {
      old_value = m_delete(vtype, var);
      existed = 1;
    }

    object o = hilfe_compile(type + " " + code +
			     ";\nmixed ___HilfeWrapper() { return " +
			     var + "; }", var);

    if(	o && hilfe_error( catch( vtype[var] = o->___HilfeWrapper() ) ) )
      return;

    if(existed)
      vtype[var] = old_value;
  }

  private constant object_ops = (< ";", "->", "[",
				   "+", "-", "/", "*",
				   "&", "|", "^", "<<", ">>",
				   "%", "~", "==", "<", ">" >);


  // Rewrites "dangerous" tokens (int/string/float-variables) to
  // operate directly in the variable mapping. It rewrites all other
  // variables as well, but we didn't have to. Note that this
  // throws the object orientation out the door, since we apply
  // knowledge of the internals of the wrapper here.
  private int relocate( Expression expr, int p, multiset(string) symbols ) {
    multiset next_symbols = (<>);
    int plevel;
    int scanf;
    for( ; p<sizeof(expr); p++) {

      // Ignore whspc tokens
      if(expr[p][0]==' ')
	continue;

      if(expr[p]=="(") {
	plevel++;
	if(p && expr[p-1]=="sscanf") scanf=1;
	continue;
      }
      if(expr[p]==")") {
	plevel--;
	scanf=0;
	continue;
      }

      // Rewrite variable
      // FIXME: Possibly soft cast value to declare variable type.
      if(symbols[expr[p]]) {
	expr[p] = "(___hilfe->"+expr[p]+")";
	continue;
      }

      // Skip tokens preceded by . or ->
      if( (< ".", "->" >)[expr[p]] ) {
	p++;
	continue;
      }

      // Clobber variables
      // FIXME: Doesn't handle variable clobber typed as program name.
      if( (< "int", "float", "string", "mapping", "array", "multiset",
	     "program", "object" >)[expr[p]] ) {
	p++;
	// Skip type declaration
	while(expr[p]=="." || expr[p]=="|" || expr[p]=="(") {
	  if(expr[p]=="." || expr[p]=="|")
	    p += 2;
	  else
	    do {
	      if(expr[p]=="(") plevel++;
	      else if(expr[p]==")") plevel--;
	      p++;
	    } while(plevel);
	}

	if(plevel && !scanf)
	  next_symbols[expr[p]] = 1;
	else
	  symbols[expr[p]] = 0;

	// FIXME: comma-seperated list of variables.
      }

      // Handle scopes
      if(expr[p]=="}")
	return p;
      if(expr[p]=="{") {
	p = relocate(expr, p+1, symbols-next_symbols);
	next_symbols = (<>);
      }
    }
  }

  //! Parses a Pike expression. Returns 0 if everything went well,
  //! or a string with an error message otherwise.
  int(0..0)|string parse_expression(Expression expr)
  {
    // Check for modifiers
    expr->check_modifiers();

    // Rewrite variables for the Hilfe wrapper
    relocate(expr, 0, (multiset)(indices(variables)) );

    // Identify the type of statement so that we can intercept
    // variable declarations and store them locally.
    string type = expr[0];
    if( has_value(expr->first_complex(), ".") &&
	type!="(" ) type=".object";
    if(programs[expr[0]] && expr[1]!="(") type=".local";

    switch(type)
    {
      case "if":
      case "for":
      case "do":
      case "while":
      case "foreach":
	// Parse loops.
	evaluate(expr->code(), 0);
	return 0;

      case "inherit":
      {
	inherits += ({ expr[1..sizeof(expr)-2] });
	if(!hilfe_compile(""))
	  inherits = inherits[..sizeof(inherits)-2];
	return 0;
      }

      case "import":
      {
	imports += ({ expr[1..sizeof(expr)-2] });
	if(!hilfe_compile(""))
	  imports = imports[..sizeof(imports)-2];
	return 0;
      }

      case "constant":
      {
	int pos = 1;

	while(pos<sizeof(expr)) {
	  int from = pos;
	  int plevel;
	  while((expr[pos]!="," && expr[pos]!=";") || plevel) {
	    if(expr[pos]=="(") plevel++;
	    else if(expr[pos]==")") plevel--;
	    pos++;
	    if(pos==sizeof(expr))
	      return "Hilfe Error: Bug in constant handling. Please report this!\n";
	  }
	  add_hilfe_constant([string]expr[from..pos-1], expr[from]);
	  pos++;
	}

	return 0;
      }

      case "class":
	add_hilfe_entity(expr[0], [string]expr[1..], expr[1], programs);
	return 0;

      case "int":
      case "void":
      case "object":
      case ".object":
      case ".local":
      case "array":
      case "mapping":
      case "string":
      case "multiset":
      case "float":
      case "mixed":
      case "program":
      case "function":
      {
	// This is either a variable declaration or a new function.

	string type = expr[0];
	int pos=1;

	// Find out the whole type, e.g. Stdio.File|mapping(string:int(0..3))
	while(expr[pos]=="." || expr[pos]=="|" || expr[pos]=="(") {
	  if(expr[pos]=="." || expr[pos]=="|")
	    type += expr[pos++] + expr[pos++];
	  else {
	    int plevel=0;
	    do {
	      type += expr[pos];
	      if(expr[pos]==",") type += " ";
	      else if(expr[pos]=="(") plevel++;
	      else if(expr[pos]==")") plevel--;
	      pos++;
	    } while(plevel);
	  }
	}

	if(object_ops[expr[pos]])
	  break;

	// This is a new function
	if(expr[pos+1]=="(") {
	  if(constants[expr[pos]])
	    return "Hilfe Error: \"" + expr[pos] + "\" already defined as constant.\n";
	  add_hilfe_entity(type, [string]expr[pos..], expr[pos], functions);
	  return 0;
	}

	while(pos<sizeof(expr)) {
	  int from = pos;
	  int plevel;
	  while((expr[pos]!="," && expr[pos]!=";") || plevel) {
	    if(expr[pos]=="(" || expr[pos]=="{") plevel++;
	    else if(expr[pos]==")" || expr[pos]=="}") plevel--;
	    pos++;
	    if(pos==sizeof(expr))
	      return "Hilfe Error: Bug in variable handling or error in variable assignment.\n";
	  }
	  add_hilfe_variable(type, [string]expr[from..pos-1], expr[from]);
	  pos++;
	}

	return 0;
      }
    }

    // parse expressions
    evaluate("return " + expr->code(), 1);
    return 0;
  }


  //
  //
  // Compilation/Evaluation code
  //
  //

  //! The last created wrapper in which an expression was evaluated.
  string last_compiled_expr;

  //! The last compile time;
  int(0..) last_compile_time;

  //! The last evaluation time;
  int(0..) last_eval_time;

  //! The current trace level.
  int trace_level;
#if constant(_assembler_debug)
  //! The current assembler debug level.
  //! Only available if Pike is compiled with RTL debug.
  int assembler_debug_level;
#endif
#if constant(_compiler_trace)
  //! The current compiler trace level.
  //! Only available if Pike is compiled with RTL debug.
  int compiler_trace_level;
#endif
#if constant(_debug)
  //! The current debug level.
  //! Only available if Pike is compiled with RTL debug.
  int debug_level;
#endif

  void std_reswrite(function w, string sres, int num, mixed res) {
    w( "Result %d: %s\n", num,
       replace(sres, "\n", "\n         "+(" "*sizeof(""+num))) );
  }

  function reswrite = std_reswrite;

  private class HilfeCompileHandler (int stack_level) {
    mapping(string:mixed) hilfe_symbols;

    mapping(string:mixed) get_default_module() {
      return all_constants() + hilfe_symbols;
    }

    string errors = "";
    string warnings = "";

    string format(string file, int line, string err) {
      if(file=="HilfeInput")
	file = "";
      else
	file += ":";
      if(err[-1]!='\n') err += "\n";
      string linestr = line?(string)line:"-";
      return sprintf(": %s%s:%s", file, linestr, err);
    }

    void compile_error(string file, int line, string err) {
      errors += "Compiler Error" + format(file, line, err);
    }

    void compile_warning(string file, int line, string warn) {
      warnings += "Compiler Warning" + format(file, line, warn);
    }

    int compile_exception (object|array trace)
    {
      if (!objectp (trace) ||
	  !trace->is_cpp_error && !trace->is_compilation_error) {
	// Errors thrown directly by cpp() and compile() are normally not
	// interesting; they've already been reported to compile_error.
	catch {
	  trace = ({trace[0], trace[1][stack_level + 1..]});
	  if (trace[1][0][0] == "Optimizer")
	    // When the compiler evaluates constants there's a
	    // somewhat odd frame "Optimizer:0 0()" at the top.
	    trace[1] = trace[1][1..];
	};
	errors += "Compiler Exception: " + describe_backtrace (trace);
      }
      return 1;
    }

    void show_errors() {
      write(errors);
    }

    void show_warnings() {
      write(warnings);
    }

    string _sprintf(int type) {
      if(type=='O' || type=='t') return "HilfeCompileHandler";
    }
  };

  //! Creates a wrapper and compiles the pike code @[f] in it.
  //! If a new variable is compiled to be tested, its name
  //! should be given in @[new_var] so that magically defined
  //! entities can be undefined and a warning printed.
  object hilfe_compile(string f, void|string new_var)
  {
    if(new_var && commands[new_var])
      write("Hilfe Warning: Command %O no longer reachable. Use %O instead.\n",
	    new_var, "."+new_var);

    if(new_var=="___hilfe" || new_var=="___Hilfe" || new_var=="___HilfeWrapper" ) {
      write("Hilfe Error: Symbol %O must not be defined.\n"
	    "             It is used internally by Hilfe.\n", new_var);
      return 0;
    }

    if(new_var=="=") {
      write("Hilfe Error: No variable name specified.\n");
      return 0;
    }

    mapping symbols = constants + functions + programs;

    if(new_var=="__")
      write("Hilfe Warning: History variable __ is no longer reachable.\n");
    else if(zero_type(symbols["__"]) && zero_type(variables["__"])) {
      symbols["__"] = history;
    }

    if(new_var=="_")
      write("Hilfe Warning: History variable _ is no longer reachable.\n");
    else if(zero_type(symbols["_"]) && zero_type(variables["__"])
	    && sizeof(history)) {
      symbols["_"] = history[-1];
    }

    string prog =
      ("#pragma unpragma_strict_types\n" +

       map(inherits, lambda(string f) { return "inherit "+f+";\n"; }) * "" +

       map(imports, lambda(string f) { return "import "+f+";\n"; }) * "" +

       "mapping(string:mixed) ___hilfe = ___Hilfe->variables;\n# 1\n" + f + "\n");

    HilfeCompileHandler handler = HilfeCompileHandler (sizeof (backtrace()));

    handler->hilfe_symbols = symbols;
    handler->hilfe_symbols->___Hilfe = this_object();
    handler->hilfe_symbols->write = write;

    last_compiled_expr = prog;
    program p;
    mixed err;

    last_compile_time = gethrtime();
    err = catch(p=compile_string(prog, "HilfeInput", handler));
    last_compile_time = gethrtime()-last_compile_time;

    if(err) {
      handler->show_warnings();
      handler->show_errors();
      return 0;
    }

    object o;
    if(hilfe_error( catch(o=clone(p)) ))
      return o;

    return 0;
  }

  //! Compiles the Pike code @[a] and evaluates it by
  //! calling ___HilfeWrapper in the generated object.
  //! If @[show_result] is set the result will be displayed
  //! and the result buffer updated with its value.
  void evaluate(string a, int(0..1) show_result)
  {
    if(trace_level)
      a = "\ntrace("+trace_level+");\n" + a;
#if constant(_assembler_debug)
    if(assembler_debug_level)
      a = "\n_assembler_debug("+assembler_debug_level+");\n" + a;
#endif
#if constant(_compiler_trace)
    if(compiler_trace_level)
      a = "\n_compiler_trace("+compiler_trace_level+");\n" + a;
#endif
#if constant(_debug)
    if(debug_level)
      a = "\n_debug("+debug_level+");\n" + a;
#endif
    a = "mixed ___HilfeWrapper() { " + a + " ; }";

    object o;
    if( o=hilfe_compile(a) )
    {
      mixed res;
      last_eval_time = gethrtime();
      mixed err = catch{
	res = o->___HilfeWrapper();
	trace(0);
#if constant(_assembler_debug)
	_assembler_debug(0);
#endif
#if constant(_compiler_trace)
	_compiler_trace(0);
#endif
#if constant(_debug)
	_debug(0);
#endif
      };
      last_eval_time = gethrtime()-last_eval_time;

      if( err || (err=catch(a=sprintf("%O", res))) )
      {
	if(objectp(err) && err->is_generic_error)
	  catch { err = ({ err[0], err[1] }); };

	if(arrayp(err) && sizeof(err)==2 && arrayp(err[1]))
	{
	  err[1]=err[1][sizeof(backtrace())..];
	  write(describe_backtrace(err));
	}
	else
	  write("Hilfe Error: Error in evaluation: %O\n",err);
      }
      else {
	if(show_result) {
	  history->push(res);
	  reswrite( write, a, history->get_latest_entry_num(), res,
		    last_compile_time, last_eval_time );
	}
	else
	  write("Ok.\n");

      }
    }
  }

  string _sprintf(int type) {
    if(type=='O' || type=='t') return "HilfeEvaluator";
  }
}


//
// Different wrappers that give the Hilfe a user interface
//

//! This is a wrapper containing a user interface to the Hilfe @[Evaluator]
//! so that it can actually be used. This wrapper uses the @[Stdio.Readline]
//! module to interface with the user. All input history is handled by
//! that module, and as a consequence loading and saving .hilfe_history is
//! handled in this class. Also .hilferc is handled by this class.
class StdinHilfe
{
  inherit Evaluator;

  //! The readline object,
  Stdio.Readline readline;

  private int(0..1) unsaved_history;

  void destroy() {
    //    readline->get_history()->pop();
    save_history();
  }

  //! Saves the user input history, if possible, when called.
  void save_history()
  {
    if(!unsaved_history) return;
    unsaved_history = 0;
    catch {
      if(string home=getenv("HOME")||getenv("USERPROFILE"))
      {
	rm(home+"/.hilfe_history~");
	if(object f=Stdio.File(home+"/.hilfe_history~","wct"))
	{
	  f->write(readline->get_history()->encode());
	  f->close();
	}
	rm(home+"/.hilfe_history");
	mv(home+"/.hilfe_history~",home+"/.hilfe_history");
#if constant(chmod)
	chmod(home+"/.hilfe_history", 0600);
#endif
      }
    };
  }

  void signal_trap(int s)
  {
    save_history();
    exit(1);
  }

  //!
  void create()
  {
    write=predef::write;
    ::create();

    if(string home=getenv("HOME")||getenv("USERPROFILE"))
    {
      if(string s=Stdio.read_file(home+"/.hilferc"))
	map(s/"\n", add_buffer);
    }

    readline = Stdio.Readline();
    array(string) hist;
    catch{
      if(string home=getenv("HOME")||getenv("USERPROFILE"))
      {
	if(Stdio.File f=Stdio.File(home+"/.hilfe_history","r"))
	{
	  string s=f->read()||"";
	  hist=s/"\n";
	  readline->enable_history(hist);
	}
      }
    };
    if(!hist)
      readline->enable_history(512);
    signal(signum("SIGINT"),signal_trap);

    for(;;)
    {
      readline->set_prompt(state->finishedp() ? "> " : ">> ");
      string s=readline->read();

      if(!s)
	break;

      unsaved_history = 1;
      add_input_line(s);
    }
    save_history();
    destruct(readline);
    write("Terminal closed.\n");
  }
}

//!
class GenericHilfe
{
  inherit Evaluator;

  //!
  void create(Stdio.FILE in, Stdio.File out)
  {
    write=out->write;
    ::create();

    while(1)
    {
      write(state->finishedp() ? "> " : ">> ");
      if(string s=in->gets())
	add_input_line(s);
      else {
	write("Terminal closed.\n");
	return;
      }
    }
  }
}

//!
class GenericAsyncHilfe
{
  inherit Evaluator;
  Stdio.File infile, outfile;

  string outbuffer="";

  void write_callback()
  {
    int i=outfile->write(outbuffer);
    outbuffer=outbuffer[i..];
  }

  void send_output(string s, mixed ... args)
  {
    outbuffer+=sprintf(s,@args);
    write_callback();
  }

  string inbuffer="";
  void read_callback(mixed id, string s)
  {
    inbuffer+=s;
    foreach(inbuffer/"\n",string s)
      {
	add_input_line(s);
	write(state->finishedp() ? "> " : ">> ");
      }
  }

  void close_callback()
  {
    write("Terminal closed.\n");
    destruct(this_object());
    destruct(infile);
    if(outfile) destruct(outfile);
  }

  //!
  void create(Stdio.File in, Stdio.File out)
  {
    infile=in;
    outfile=out;
    in->set_nonblocking(read_callback, 0, close_callback);
    out->set_write_callback(write_callback);

    write=send_output;
    ::create();
    write(state->finishedp() ? "> " : ">> ");
  }
}

int main() {
  StdinHilfe();
  return 0;
}


//
// Help texts for the built in commands.
//

constant documentation_set =
#"Change Hilfe settings. Used as \"set <setting> <parameter>\".
Available parameters:

assembler_debug
    Changes the level of assembler debug used when evaluating
    expressions in Pike. Requires that Pike is compiled with
    RTL debug.

compiler_trace
    Changes the level of compiler trace used when evaluating
    expressions in Pike. Requires that Pike is compiled with
    RTL debug.

debug
    Changes the level of debug used when evaluating expressions in
    Pike. Requires that Pike is compiled with RTL debug.

format
    Changes the formatting of the result values from evaluated
    Pike expressions. Enter \"help set format\" for more
    information.

history
    Change the maximum number of entries that are kept in the
    result history. Default is 10.

trace
     Changes the level of trace used when evaluating expressions
     in Pike. Possible values are:
       0 Off
       1 Calls to Pike functions are printed.
       2 Calls to buitin functions are printed.
       3 Every opcode interpreted is printed.
       4 Arguments to these opcodes are printed as well.
";

constant documentation_set_format =
#"\"set format\" changes the formatting of the result values from
evaluated Pike expressions. Currently the following set format
parameters are available:

default  The normal result formatting.
bench    A result formatting extended with compilation and
         evaluation times.
sprintf  The result formatting will be decided by the succeeding
         Pike string. The sprintf will be given the following
         arguments:

           0  The result as a string.
           1  The result number in the history.
           2  The result in its native type.
           3  The compilation time as a string.
           4  The evaluation time as a string.
           5  The compilation time in nanoseconds as an int.
           6  The evaluation time in nanoseconds as an int.

         Usage examples:
           set format sprintf \"%s (%[2]t)\\n\"
           set format sprintf \"%s (%d/%[3]s/%[4]s)\\n\"
";

constant documentation_help_me_more =
#"Some commands have extended help available. This can be displayed by
typing help followed by the name of the command, e.g. \"help dump\".
Commands clobbered by e.g. variable declarations can be reached by
prefixing a dot to the command, e.g. \".exit\".

A history of the last returned results is kept and can be accessed
from your hilfe expressions with the variable __. You can either
\"address\" your results with absolute addresses, e.g. __[2] to get
the second result ever, or with relative addresses, e.g. __[-1] to
get the last result. The last result is also available in the
variable _, thus _==__[-1] is true. The magic _ and __ variable can
be clobbered with local definitions to disable them, e.g. by typing
\"int _;\". The result history is currently ten entries long.

A history of the 512 last entered lines is kept in Hilfe. You can
browse this list with your arrow keys up/down. When you exit Hilfe
your history will be saved in .hilfe_history in the directory set
in environment variable $HOME or $USERPROFILE. Next time hilfe is
started the history is imported.

You can put a .hilferc file in the directory set in your
environment variable $HOME or $USERPROFILE. The contents of this
file will be evaluated in hilfe during each startup.

Note that there are a few symbols that you can not define, since
they are used by Hilfe. They are:

___hilfe         A mapping containing all defined symbols.
___Hilfe         The Hilfe object.
___HilfeWrapper  A wrapper around the entered expression.


Type \"help hilfe todo\" to get a list of known Hilfe bugs/lackings.
";

constant documentation_dump =
#"dump
      Shows the currently defined constants, variables, functions
      and programs. It also lists all active inherits and imports.

dump history
      Shows all items in the history queue.

dump state
      Shows the current parser state. Only useful for debugging
      Hilfe.

dump wrapper
      Show the latest Hilfe wrapper that the last expression was
      evaluated in. Useful when debugging Hilfe (i.e. investigating
      why valid Pike expressions doesn't compile).
";

constant documentation_new =
#"new
      Clears the current Hilfe state. This includes the parser
      state, variables, constants, functions, programs, inherits,
      imports and the history. It does not include the currently
      installed commands. Note that code in your .hilferc will not
      be reevaluated.

new history
      Remove all history entries from the result history.

new constants
new functions
new programs
new variables
      Clears all locally defined symbols of the given type.

new imports
new inherits
      Removes all imports/inherits made.
";
