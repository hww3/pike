//========================================================================
// EXTRACTION OF DOCUMENTATION FROM PIKE SOURCE FILES
//========================================================================

#pragma strict_types

static inherit .PikeObjects;
static inherit .DocParser;

#include "./debug.h"

//========================================================================
// DOC EXTRACTION
//========================================================================

static private class Extractor {
  static constant WITH_NL = .PikeParser.WITH_NL;

  //  static private string filename;
  static private .PikeParser parser;

  static void create(string s, string filename) {
    parser = .PikeParser();
    array(object/*. PikeParser.Token */) a = parser->tokenize(s, filename, 1);
    array(object) tokens = ({});
    // the source positions of the @ignore directives.
    array(SourcePosition) ignores = ({});
    foreach(a, object token) {
      string s = token->text;
      SourcePosition pos = token->position;
      int ignoreline = 0;
      if (has_prefix(s, DOC_COMMENT)) {
        s = String.trim_all_whites(s[strlen(DOC_COMMENT) .. ]);
        ignoreline = 1;
        if (s == "@ignore")
          ignores = ({ pos }) + ignores;
        else if (s == "@endignore")
          if (sizeof(ignores))
            ignores = ignores[1 .. ];
          else
            throw (AutoDocError(pos, "PikeExtractor",
                                "@endignore without matching @ignore"));
        else
          ignoreline = 0;
      }
      if (sizeof(ignores) == 0 && !ignoreline)
        tokens += ({ token });
    }
    if (sizeof(ignores))
      throw (AutoDocError(ignores[0], "PikeExtractor",
                          "@ignore without matchin @endignore"));
    parser->setTokens(tokens);
  }

  static void extractorError(string message, mixed ... args) {
    message = sprintf(message, @args);
    throw (AutoDocError(parser->currentPosition, "PikeExtractor", message));
  }

  static int isDocComment(string s) {
    return (strlen(s) >= 3 && s[0..2] == DOC_COMMENT);
  }

  static string stripDocMarker(string s) {
    if (isDocComment(s))
      return s[3..];
    throw("OOPS");
  }

  // readAdjacentDocLines consumes the doc lines AND the "\n" after the last one
  static Documentation readAdjacentDocLines() {
    Documentation res = Documentation();
    string s = parser->peekToken();
    string text = "";
    SourcePosition pos = parser->currentPosition->copy();
    while (isDocComment(s)) {
      text += stripDocMarker(s) + "\n";
      parser->readToken();              // Grok the doc comment string
      s = parser->peekToken(WITH_NL);
      if (s == "\n")
        parser->readToken(WITH_NL);     // Skip the corresp. NL
      else
        break;             // Ought to be EOF
      s = parser->peekToken(WITH_NL);   // NL will break the loop
    }
    res->text = text;
    res->position = pos;
    return res;
  }

  // check if the token is one that can not start a declaration
  // adjacent to the last one
  static int isDelimiter(string token) {
    return (< "\n", "}", "EOF" >) [token];
  }

  // consumes the "\n" after the last constant too...
  static array(EnumConstant) parseAdjacentEnumConstants() {
    array(EnumConstant) result = ({ });
    parser->skipNewlines();
    while (isIdent(parser->peekToken(WITH_NL))) {
      EnumConstant c = EnumConstant();
      c->position = parser->currentPosition->copy();
      c->name = parser->readToken(); // read the identifier
      result += ({ c });
      parser->skipUntil((<",", "}">)); // don't care about any init expr
      if (parser->peekToken() == ",")
        parser->eat(",");
      else
        break;
      // allow at most ONE intervening newline character
      if (parser->peekToken(WITH_NL) == "\n")
        parser->readToken(WITH_NL);
    }
    return result;
  }

  // Returns nothing, instead adds the enumconstants as children to
  // the Enum parent
  static void parseEnumBody(Enum parent) {
    for (;;) {
      parser->skipNewlines();
      Documentation doc = 0;
      array(EnumConstant) consts = ({});
      if (isIdent(parser->peekToken())) {
        // The case with one or more constants, optionally followed by
        // a doc comment.
        consts = parseAdjacentEnumConstants();
        if (!sizeof(consts)) // well, could never happen, but ...
          extractorError("expected enum constant");

        // read the optional doc comment
        if (isDocComment(parser->peekToken(WITH_NL)))
          doc = readAdjacentDocLines();
        if (isIdent(parser->peekToken()))
          extractorError("constant + doc + constant not allowed");
      }
      else if (isDocComment(parser->peekToken())) {
        // The case with a doc comment, that must be followed by
        // one or more constants.
        doc = readAdjacentDocLines(); // consumes the \n after //... too!
        if (!isIdent(parser->peekToken(WITH_NL)))
          extractorError("expected enum constant");
        consts = parseAdjacentEnumConstants();
        // consumes the "\n" after them too...
        if (isDocComment(parser->peekToken(WITH_NL)))
          extractorError("cant have doc both before and after enum constant");
      }
      else if (parser->peekToken() == "}")
        // reached the end of the enum { ... }.
        return;
      else
        extractorError("expected doc comment or enum constant, got %O",
                       parser->peekToken());

      if (doc) {
        .DocParser.Parse parse = .DocParser.Parse(doc->text, doc->position);
        .DocParser.MetaData metadata = parse->metadata();
        if (metadata->appears || metadata->belongs)
          extractorError("@appears or @belongs not allowed in "
                         "doc for enum constant");
        if (metadata->type)
          extractorError("@%s not allowed in doc for enum constant",
                         metadata->type);
        doc->xml = parse->doc("_enumconstant");

        parent->addChild(DocGroup(consts, doc));
      }
      // ignore constants without any adjacent doc comment...
    }
  }

  // parseAdjacentDecls consumes the "\n" that may follow the last decl
  static array(PikeObject) parseAdjacentDecls(Class|Module c) {
    array(PikeObject) res = ({ });
    for (;;) {
      // To get the correct line# :
      parser->skipNewlines();
      SourcePosition pos = parser->currentPosition->copy();

      object(PikeObject)|array(PikeObject) p = parser->parseDecl();

      multiset(string) allowSqueeze = (<
        "method",
        "class",
        "enum"
      >);

      if (isDocComment(parser->peekToken())) {
        if (!objectp(p) || !allowSqueeze[p->objtype])
          extractorError("sqeezed in doc only allowed for %s",
                         String.implode_nicely(indices(allowSqueeze)));
        Documentation doc = readAdjacentDocLines();
        p->squeezedInDoc = doc;
      }
      if (objectp(p) && p->objtype == "class" && parser->peekToken() == "{") {
        parser->eat("{");
        parseClassBody([object(Class)] p, 0);
        parser->eat("}");
      }
      else if (objectp(p) && p->objtype == "modifier" &&
	       parser->peekToken() == "{") {
	parser->eat("{");
        parseClassBody(c, p->modifiers);
	parser->eat("}");
      }
      else if (objectp(p) && p->objtype == "enum") {
        parser->eat("{"); // after ("enum" opt_id) must come "{"
        parseEnumBody([object(Enum)] p);
        parser->eat("}");
      }
      else if (parser->peekToken() == "{") {
        int mark1 = parser->getReadDocComments();
        parser->skipBlock();
        int mark2 = parser->getReadDocComments();
        if (mark2 != mark1)
          extractorError("%d illegal doc comment lines inside block",
                         mark2 - mark1);
      }
      else
        parser->eat(";");
      while (parser->peekToken(WITH_NL) == ";")
        parser->readToken();
      foreach(arrayp(p) ? [array(object(PikeObject))]p :
	      ({ [object(PikeObject)]p }), PikeObject obj)
        obj->position = obj->position || pos;

      res += arrayp(p) ? p : ({ p });   // int x,y;  =>  array of PikeObject
      if (parser->peekToken(WITH_NL) == "\n")   // we allow ONE "\n" inbetween
        parser->readToken(WITH_NL);
      string s = parser->peekToken(WITH_NL);
      if (isDelimiter(s) || isDocComment(s))
        return res;
    }
  }
  // parseClassBody does the main work and scans the stream looking for:
  // 1.   doclines + decls, no blank line in between
  // 2.   decls + doclines,    -----  "  " -----
  // 3.   doclines                        (stand-alone doc)
  // 4.   decls                           (undocumented decls, discarded)
  //
  // If 'filename' is supplied, it will look for standalone doc comments
  // at the beginning of the file, and then the return value is that
  // Documentation for the file.
  Documentation parseClassBody(Class|Module c,
                               array(string) defModifiers,
                               void|string filename,
                               void|string inAt) {
    Documentation filedoc = 0;
  mainloop:
    for (;;) {
      Documentation doc = 0;
      array(PikeObject) decls = ({ });

      int docsMark = parser->getReadDocComments();

      string s = parser->peekToken();

      if (s == ";") {      // allow a semi-colon at the top level
        parser->eat(";");
        continue;
      }

      if (s == EOF || s == "}")         // end of class body reached
        return filedoc;
      if (isDocComment(s)) {
        doc = readAdjacentDocLines();    // read the doc comment lines
	if (has_value(doc->text, "@skip")) {
	  while(1) {
	    s = parser->peekToken(WITH_NL);
	    if (s == EOF) {
	      extractorError("EOF in @skip segment.");
	      break;
	    }
	    if (isDocComment(s)) {
	      doc = readAdjacentDocLines();
	      if (has_value(doc->text, "@endskip")) break;
	    } else {
	      parser->readToken(WITH_NL);	// Skip to the next token.
	    }
	  }
	  continue;	// Restart the parsing.
	}
        s = parser->peekToken(WITH_NL);
        if (!isDelimiter(s)) {           // and decls that may follow
          decls = parseAdjacentDecls(c);
          s = parser->peekToken(WITH_NL);
          if (isDocComment(s))
            extractorError("doc + decl + doc  is forbidden!");
        }
      }
      else {
        decls = parseAdjacentDecls(c);
        s = parser->peekToken(WITH_NL);
        if (isDocComment(s))
          doc = readAdjacentDocLines();
        else if ( !isDelimiter(s) )
          extractorError("decl + doc + decl  is forbidden!");
      }

      foreach (decls, PikeObject obj)
        if (obj->squeezedInDoc) {
          if (sizeof(decls) > 1)
            extractorError(
              "declaration with sqeezed in documentation must stand alone"
            );
          if (doc)
            extractorError("duplicate documentation");
          doc = obj->squeezedInDoc;
        }
      array(PikeObject) docDecls = ({ });

      object(.DocParser.Parse) parse = 0;
      string appears = 0;
      string belongs = 0;
      if (doc) {
        parse = .DocParser.Parse(doc->text, doc->position);
        MetaData meta = parse->metadata();
        if (meta->type && meta->type != "decl") {
          string what = meta->type;
          switch(what) {
            case "module":
              if (c->objtype == "class" && what == "module")
                extractorError("@module not allowed in class files");
              // fall through
            case "class":
              if (sizeof(decls))
                extractorError("@module doc comment must stand alone");
              object(Class)|object(Module) alreadyChild =
                c->findChild(meta->name);
              object(Class)|object(Module) m;
              if (alreadyChild) {
                m = alreadyChild;
                if (m->objtype != what)
                  extractorError("found @%s %s, but %s has "
                                 "previously been defined as %s",
                                 what, m->name, m->name, m->objtype);
              }
              else {
                m = what == "class" ? Class() : Module();
                m->appears = meta->appears;
                m->belongs = meta->belongs;
                m->name = meta->name;
              }
              doc->xml = parse->doc("_" + what);
              m->documentation = doc;
              if (alreadyChild)
                if (doc->xml && doc->xml != "")
                  extractorError("doc not allowed on reentrance into '%s %s'",
                                 m->objtype, m->name);
              if (!alreadyChild)
                c->AddChild(m);
              parseClassBody(m, 0, 0, what);
              continue mainloop;
            case "endclass":
            case "endmodule":
              if (sizeof(decls))
                extractorError("@%s doc comment must stand alone", meta->type);
              if (inAt != what - "end")
                extractorError("@%s has no matching %s",
                               meta->type, meta->type - "end");
              if (meta->name && meta->name != c->name)
                extractorError("'@%s %s' doesn't match '@%s %s'",
                               meta->type, meta->name, c->objtype, c->name || "");
              return 0;  // no filedoc possible
            default:
              extractorError("@%s is not allowed in Pike files", meta->type);
          }
        }
        docDecls = meta->decls;
        appears = meta->appears;
        belongs = meta->belongs;
      } else
        foreach (decls, PikeObject obj)
          if (obj->objtype == "class" &&
	      ([object(Class)]obj)->containsDoc())
            extractorError("undocumented class contains doc comments");

      // Objects added by @decl replace the true objects:
      if (sizeof(docDecls)) {
        if (sizeof(decls)) {
          if (sizeof(decls) != 1)
            extractorError("only one pike declaration can be combined with @decl");
          foreach(docDecls, PikeObject d)
            if (decls[0]->objtype != d->objtype)
              extractorError("@decl of %s mismatches %s in pike code",
                            d->objtype, decls[0]->objtype);
          foreach(docDecls, PikeObject d)
            if (decls[0]->name != d->name)
              extractorError("@decl'd %s %s mismatches %s %s in pike code",
                             d->objtype, d->name, d->objtype, decls[0]->name);
        }
        decls = docDecls;
      }

      int wasNonGroupable = 0;
      multiset(string) nonGroupable = (<"class","module","enum">);
      foreach (decls, PikeObject obj)
        if (nonGroupable[obj->objtype]) {
          wasNonGroupable = 1;
          if (sizeof(decls) > 1 && doc)
            extractorError("%s are not groupable",
                           String.implode_nicely(indices(nonGroupable)));
        }

      if (doc && !sizeof(decls))
        if (!filename || filedoc)
          extractorError("documentation comment without destination");
        else {
          // the first stand-alone comment is allowed and is interpreted
          // as documentation for the class or module (foo.pike or bar.pmod)
	  // _itself_.
          doc->xml = parse->doc((["class" : "_class",
                                  "module" : "_module"])[c->objtype]);
          filedoc = doc;
          doc = 0;
        }

      if (defModifiers && sizeof(defModifiers))
        foreach(decls, PikeObject obj)
          obj->modifiers |= defModifiers;

      mapping(string:int) contexts = ([]);

      // Make sure that all inherits are added:
      foreach(decls, PikeObject obj)
        switch (obj->objtype) {
          case "inherit":
            c->AddInherit(obj);
            // fall through
          default:
            contexts[obj->objtype] = 1;
        }

      if (doc) {
        if (wasNonGroupable) {
          object(PikeObject) d = [object(PikeObject)] decls[0];
          d->documentation = doc;
          d->appears = appears;
          d->belongs = belongs;
          c->AddChild(d);
        }
        else {
          DocGroup d = DocGroup(decls, doc);
          d->appears = appears;
          d->belongs = belongs;
          c->AddGroup(d);
        }

        string context;
        if (sizeof(indices(contexts)) == 1)
          context = "_" + indices(contexts)[0];
        else
          context = "_general";
        doc->xml = parse->doc(context);
      } // if (doc)
    } // for (;;)
  }

} // static private class Extractor

Module extractModule(string s, void|string filename, void|string moduleName) {
  Extractor e = Extractor(s, filename);
  Module m = Module();
  m->name = moduleName || filename;
  Documentation doc = e->parseClassBody(m, 0, filename);
  m->documentation = doc;
  // if there was no documentation in the file whatsoever
  if (!doc && !sizeof(m->docGroups) && !sizeof(m->children))
    return 0;
  return m;
}

Class extractClass(string s, void|string filename, void|string className) {
  Extractor e = Extractor(s, filename);
  Class c = Class();
  c->name = className || filename;
  Documentation doc = e->parseClassBody(c, 0, filename);
  c->documentation = doc;
  // if there was no documentation in the file...
  if (!doc && !sizeof(c->docGroups) && !sizeof(c->children))
    return 0;
  return c;
}
