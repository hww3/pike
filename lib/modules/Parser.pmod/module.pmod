/*
 * $Id: module.pmod,v 1.3 2000/09/28 03:38:48 hubbe Exp $
 *
 */

#pike __REAL_VERSION__

inherit Parser._parser;

//! module Parser

class SGML
//! class SGML
//!  	This is a handy simple parser of SGML-like
//!	syntax like HTML. It doesn't do anything advanced,
//!	but finding the corresponding end-tags.
//!	
//!	It's used like this:
//!	<pre>array res=Parser.SGML()->feed(string)->finish()->result();</pre>
//!
//!	The resulting structure is an array of atoms,
//!	where the atom can be a string or a tag.
//!	A tag contains a similar array, as data. 
//!     
//!	Example:
//!	A string
//!     <tt>"&lt;gat>&nbsp;&lt;gurka>&nbsp;&lt;/gurka>&nbsp;&lt;banan>&nbsp;&lt;kiwi>&nbsp;&lt;/gat>"</tt>
//!     results in 
//!	<pre>
//!	({
//!	    tag "gat" object with data:
//!	    ({
//!	        tag "gurka" object with data:
//!		({
//!                 " "
//!             })
//!	        tag "banan" object with data:
//!		({
//!                 " "
//!	            tag "kiwi" object with data:
//!		    ({
//!                    " "
//!                 })
//!             })
//!         })
//!     })
//!	</pre>
//!             
//!	ie, simple "tags" (not containers) are not detected,
//!	but containers are ended implicitely by a surrounding
//!	container _with_ an end tag.
//!
//! 	The 'tag' is an object with the following variables:
//!	<pre>
//!	 string name;           - name of tag
//!	 mapping args;          - argument to tag
//!	 int line,char,column;  - position of tag
//!	 string file;           - filename (see <ref>create</ref>)
//!	 array(SGMLatom) data;  - contained data
//!     </pre>
//!
{
   string file;

   class SGMLatom
   {
      string name;
      mapping args;
      int line,char,column;
      string file;
      array(SGMLatom) data;
   }

   static array(array(object(SGMLatom)|string)) res=({({})});
   static array(SGMLatom) tagstack=({});
   static array(object) errors;

   array(object(SGMLatom)|string) data;

   static private array(string) got_tag(object g)
   {
      string name=g->tag_name();

      if (name!="" && name[0]=='/')
      {
	 int i=search(tagstack->name,name[1..]);
	 if (i!=-1) 
	 {
	    i++;
	    while (i--)
	    {
	       tagstack[0]->data=res[0];
	       res=res[1..];
	       tagstack=tagstack[1..];
	    }
	    return ({});
	 }
      }

      object t=SGMLatom();
      t->name=name;
      t->args=g->tag_args();
      [t->line,t->char,t->column]=g->at();
      t->file=file;
      res[0]+=({t});
      tagstack=({t})+tagstack;
      res=({({})})+res;
      
      return ({}); // don't care
   }

   void debug(array|void arr,void|int level)
   {
      level+=2;
      if (!arr) arr=data;
      foreach (arr,string|object(SGMLatom) t)
	 if (stringp(t))
	    write("%*s%-=*s\n",level,"",79-level,sprintf("%O",t));
	 else
	 {
	    write("%*stag %O\n",level,"",t->name,);
	    if (sizeof(t->args))
	       write("%*s%-=*s\n",level+4,"",75-level,sprintf("%O",t->args));
	    debug(t->data,level);
	 }
   }


   private static object p=HTML();

//! method void create()
//! method void create(string filename)
//!	This object is created with this filename.
//!	It's passed to all created tags, for debug and trace purposes.
//! note:
//! 	No, it doesn't read the file itself. See <ref>feed</ref>.

   int i;

   void create(void|string _file)
   {
      file=_file;

      p->_set_tag_callback(got_tag);
//       p->_set_tag_callback(lambda() { i++; sleep(0); return ({}); });
      p->_set_data_callback(lambda(object g,string data) 
			    { if (data!="") res[0]+=({data}); return ({}); });
   }

//! method object feed(string s)
//! method array finish()
//! method array result(string s)
//!	Feed new data to the object, or finish the stream.
//!	No result can be used until finish() is called.
//!
//! 	Both finish() and result() returns the computed data.
//!
//!	feed() returns the called object.

   object feed(string s)
   {
      p->feed(s);
      return this_object();
   }

   array finish()
   {
      p->finish();
      data=res[0];
      res=0;
      werror("i=%O\n",i);
      return data;
   }

   array(object(SGMLatom)|string) result()
   {
      return data;
   }
}


