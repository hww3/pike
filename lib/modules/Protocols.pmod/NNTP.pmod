// Not finished - Fredrik Hubinette

#pike __REAL_VERSION__

//! NNTP protocol
class protocol
{
  inherit Stdio.FILE : sock;

  string rest;

  //! reads the server result code for last request
  //!  used internally by command().
  int readreturncode()
  {
    int space, code;
    string r;
    rest=0;

    do {
      space=' ';
      string tmp=sock::gets();
      if(!tmp) return 0;
      sscanf(tmp,"%d%c%s",code,space,r);
      rest+=r;
    } while(space == '-');
    return code;
  }

  //! reads the message from the server as an array of lines
  array(string) read_body_lines()
  {
    array(string) ret=({});
    string s;
    while(s = sock::gets())
    {
      if(s=="." || s==".\r") return ret;
      sscanf(s,".%s",s);
      ret+=({s});
    }
    error("Connection closed by server.\n");
  }

  //! reads the message from the server
  string readreturnbody()
  {
    array(string) tmp=read_body_lines();
    return tmp*"\n"+"\n";
  }

  //! send the body of a message to the server.
  void writebody(string s)
  {
    s=replace(s,"\r","");
    foreach(s/"\n",string line)
      {
	if(sizeof(line) && line[0]=='.')
	  line="."+line+"\r\n";
	else
	  line=line+"\r\n";
	if(sock::write(line) != sizeof(line))
	  error("Failed to write body.\n");
      }
    sock::write(".\r\n");
  }

  //! send a command to the server
  //! @returns
  //!   the result code sent by the server
  int command(string cmd)
  {
    sock::write(cmd+"\r\n");
    return readreturncode();
  }

  //! gets the result message supplied by the server for the last response
  string get_response_message()
  {
    return rest;
  }

  //! send a command and require an ok response (200 series).
  //! throws an error if the command result was not success.
  int failsafe_command(string cmd)
  {
    if(command(cmd)/100 != 2)
      error(cmd+" failed.\n");
  }

  //! send a command that should return a message body.
  //!
  //! @returns
  //!  the message body
  string do_cmd_with_body(string cmd)
  {
    failsafe_command(cmd);
    return readreturnbody();
  }
}


//! An NNTP client
class client
{
  inherit protocol;

  class Group
  {
    string group;
    int min;
    int max;
  }

  //! Returns a list of all active groups.
  array(Group) list_groups()
  {
    array(Group) ret=({});
    failsafe_command("list active");
    foreach(read_body_lines(),string line)
    {
      Group o=Group();
      if(sscanf(line,"%s %d %d",o->group,o->max,o->min)==3)
	ret+=({o});
    }

    return ret;
  }

  //! The current news group.
  Group current_group;

  //! Sets the current news group to @[o].
  void set_group(Group o)
  {
    if(current_group==o)
      return;
    failsafe_command("group "+o->group);
    current_group=o;
  }

  //! Sets the current group to @[group].
  Group go_to_group(string group)
  {
    failsafe_command("group "+group);
    Group o=Group();
    o->group=group;
    sscanf(rest,"%d %d %d",int num,o->min,o->max);
    current_group=o;
    return o;
  }

  //!
  string head(void|int|string x)
  {
    failsafe_command("head"+(x?" "+x:""));
    return readreturnbody();
  }

  //!
  string body(void|int|string x)
  {
    failsafe_command("body"+(x?" "+x:""));
    return readreturnbody();
  }

  //!
  string article(void|int|string x)
  {
    failsafe_command("article"+(x?" "+x:""));
    return readreturnbody();
  }

  //!
  void create(string|void server)
  {
    if(!server)
    {
      server=getenv("NNTPSERVER");

      if(!server)
      {
	// Check /etc/nntpserver here 
      }
    }

    if(!connect(server,119))
      error("Failed to connect to news server.\n");

    if(readreturncode()/100 != 2)
      error("Connection refused by NNTP server.\n");

    if(command("mode reader")/100 !=2)
      error("Mode reader failed.\n");
  }
}
