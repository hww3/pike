//
// $Id: TELNET.pmod,v 1.31 2008/12/06 17:07:31 grubba Exp $
//
// The TELNET protocol as described by RFC 764 and others.
//
// Henrik Grubbstr�m <grubba@roxen.com> 1998-04-04
//

#pike __REAL_VERSION__

// #define TELNET_DEBUG

#ifdef TELNET_DEBUG
#define DWRITE(X)	werror(X)
#else
#define DWRITE(X)
#endif /* TELNET_DEBUG */

//! Implements TELNET as described by RFC 764 and RFC 854
//!
//! Also implements the Q method of TELNET option negotiation
//! as specified by RFC 1143.

/* Extract from RFC 1143:
 *
 * EXAMPLE STATE MACHINE FOR THE Q METHOD OF IMPLEMENTING
 * TELNET OPTION NEGOTIATION 
 *
 * There are two sides, we (us) and he (him).  We keep four variables:
 *
 *    us: state of option on our side (NO/WANTNO/WANTYES/YES)
 *    usq: a queue bit (EMPTY/OPPOSITE) if us is WANTNO or WANTYES
 *    him: state of option on his side
 *    himq: a queue bit if him is WANTNO or WANTYES
 *
 * An option is enabled if and only if its state is YES.  Note that
 * us/usq and him/himq could be combined into two six-choice states.
 *
 * "Error" below means that producing diagnostic information may be a
 * good idea, though it isn't required.
 *
 * Upon receipt of WILL, we choose based upon him and himq:
 *    NO            If we agree that he should enable, him=YES, send
 *                  DO; otherwise, send DONT.
 *    YES           Ignore.
 *    WANTNO  EMPTY Error: DONT answered by WILL. him=NO.
 *         OPPOSITE Error: DONT answered by WILL. him=YES*,
 *                  himq=EMPTY.
 *    WANTYES EMPTY him=YES.
 *         OPPOSITE him=WANTNO, himq=EMPTY, send DONT.
 *
 * * This behavior is debatable; DONT will never be answered by WILL
 *   over a reliable connection between TELNETs compliant with this
 *   RFC, so this was chosen (1) not to generate further messages,
 *   because if we know we're dealing with a noncompliant TELNET we
 *   shouldn't trust it to be sensible; (2) to empty the queue
 *   sensibly.
 *
 * Upon receipt of WONT, we choose based upon him and himq:
 *    NO            Ignore.
 *    YES           him=NO, send DONT.
 *    WANTNO  EMPTY him=NO.
 *         OPPOSITE him=WANTYES, himq=NONE, send DO.
 *    WANTYES EMPTY him=NO.*
 *         OPPOSITE him=NO, himq=NONE.**
 *
 * * Here is the only spot a length-two queue could be useful; after
 *   a WILL negotiation was refused, a queue of WONT WILL would mean
 *   to request the option again. This seems of too little utility
 *   and too much potential waste; there is little chance that the
 *   other side will change its mind immediately.
 *
 * ** Here we don't have to generate another request because we've
 *    been "refused into" the correct state anyway.
 *
 * If we decide to ask him to enable:
 *    NO            him=WANTYES, send DO.
 *    YES           Error: Already enabled.
 *    WANTNO  EMPTY If we are queueing requests, himq=OPPOSITE;
 *                  otherwise, Error: Cannot initiate new request
 *                  in the middle of negotiation.
 *         OPPOSITE Error: Already queued an enable request.
 *    WANTYES EMPTY Error: Already negotiating for enable.
 *         OPPOSITE himq=EMPTY.
 *
 * If we decide to ask him to disable:
 *    NO            Error: Already disabled.
 *    YES           him=WANTNO, send DONT.
 *    WANTNO  EMPTY Error: Already negotiating for disable.
 *         OPPOSITE himq=EMPTY.
 *    WANTYES EMPTY If we are queueing requests, himq=OPPOSITE;
 *                  otherwise, Error: Cannot initiate new request
 *                  in the middle of negotiation.
 *         OPPOSITE Error: Already queued a disable request.
 *
 * We handle the option on our side by the same procedures, with DO-
 * WILL, DONT-WONT, him-us, himq-usq swapped.
 */


//! Table of IAC-codes.
class TelnetCodes {
  constant IAC = 255;		//! interpret as command.
  constant DONT = 254;		//! you are not to use option
  constant DO = 253;		//! please, you use option
  constant WONT = 252;		//! I won't use option
  constant WILL = 251;		//! I will use option
  constant SB = 250;		//! interpret as subnegotiation
  constant GA = 249;		//! you may reverse the line
  constant EL = 248;		//! erase the current line
  constant EC = 247;		//! erase the current character
  constant AYT = 246;		//! are you there
  constant AO = 245;		//! abort output--but let prog finish
  constant IP = 244;		//! interrupt process--permanently
  constant BREAK = 243;		//! break
  constant DM = 242;		//! data mark--for connect. cleaning
  constant NOP = 241;		//! nop
  constant SE = 240;		//! end sub negotiation
  constant EOR = 239;           //! end of record (transparent mode)
  constant ABORT = 238;		//! Abort process
  constant SUSP = 237;		//! Suspend process
  constant xEOF = 236;		//! End of file: EOF is already used...

  constant SYNCH = 242;		//! for telfunc calls
};

inherit TelnetCodes;


//! Table of TELNET options.
class Telopts {
  constant TELOPT_BINARY = 0;	//! 8-bit data path
  constant TELOPT_ECHO = 1;	//! echo
  constant TELOPT_RCP = 2;	//! prepare to reconnect
  constant TELOPT_SGA = 3;	//! suppress go ahead
  constant TELOPT_NAMS = 4;	//! approximate message size
  constant TELOPT_STATUS = 5;	//! give status
  constant TELOPT_TM = 6;	//! timing mark
  constant TELOPT_RCTE = 7;	//! remote controlled transmission and echo
  constant TELOPT_NAOL = 8;	//! negotiate about output line width
  constant TELOPT_NAOP = 9;	//! negotiate about output page size
  constant TELOPT_NAOCRD = 10;	//! negotiate about CR disposition
  constant TELOPT_NAOHTS = 11;	//! negotiate about horizontal tabstops
  constant TELOPT_NAOHTD = 12;	//! negotiate about horizontal tab disposition
  constant TELOPT_NAOFFD = 13;	//! negotiate about formfeed disposition
  constant TELOPT_NAOVTS = 14;	//! negotiate about vertical tab stops
  constant TELOPT_NAOVTD = 15;	//! negotiate about vertical tab disposition
  constant TELOPT_NAOLFD = 16;	//! negotiate about output LF disposition
  constant TELOPT_XASCII = 17;	//! extended ascic character set
  constant TELOPT_LOGOUT = 18;	//! force logout
  constant TELOPT_BM = 19;	//! byte macro
  constant TELOPT_DET = 20;	//! data entry terminal
  constant TELOPT_SUPDUP = 21;	//! supdup protocol
  constant TELOPT_SUPDUPOUTPUT = 22;	//! supdup output
  constant TELOPT_SNDLOC = 23;		//! send location
  constant TELOPT_TTYPE = 24;		//! terminal type
  constant TELOPT_EOR = 25;		//! end or record
  constant TELOPT_TUID = 26;		//! TACACS user identification
  constant TELOPT_OUTMRK = 27;		//! output marking
  constant TELOPT_TTYLOC = 28;		//! terminal location number
  constant TELOPT_3270REGIME = 29;	//! 3270 regime
  constant TELOPT_X3PAD = 30;		//! X.3 PAD
  constant TELOPT_NAWS = 31;		//! window size
  constant TELOPT_TSPEED = 32;		//! terminal speed
  constant TELOPT_LFLOW = 33;		//! remote flow control
  constant TELOPT_LINEMODE = 34;	//! Linemode option
  constant TELOPT_XDISPLOC = 35;	//! X Display Location
  constant TELOPT_OLD_ENVIRON = 36;	//! Old - Environment variables
  constant TELOPT_AUTHENTICATION = 37;	//! Authenticate
  constant TELOPT_ENCRYPT = 38;		//! Encryption option
  constant TELOPT_NEW_ENVIRON = 39;	//! New - Environment variables
  constant TELOPT_EXOPL = 255;		//! extended-options-list
};

inherit Telopts;

mapping lookup_telopt=mkmapping(values(Telopts()),indices(Telopts()));
mapping lookup_telnetcodes=mkmapping(values(TelnetCodes()),indices(TelnetCodes()));


/* sub-option qualifiers */
constant TELQUAL_IS=	0;	//! option is...
constant TELQUAL_SEND=	1;	//! send option
constant TELQUAL_INFO=	2;	//! ENVIRON: informational version of IS
constant TELQUAL_REPLY=	2;	//! AUTHENTICATION: client version of IS
constant TELQUAL_NAME=	3;	//! AUTHENTICATION: client version of IS

constant LFLOW_OFF=		0;	//! Disable remote flow control
constant LFLOW_ON=		1;	//! Enable remote flow control
constant LFLOW_RESTART_ANY=	2;	//! Restart output on any char
constant LFLOW_RESTART_XON=	3;	//! Restart output only on XON


constant	LM_MODE	= 1;
constant	LM_FORWARDMASK = 2;
constant	LM_SLC = 3;

constant	MODE_EDIT =     0x01;
constant	MODE_TRAPSIG =  0x02;
constant	MODE_ACK =	0x04;
constant	MODE_SOFT_TAB =	0x08;
constant	MODE_LIT_ECHO =	0x10;

constant	MODE_MASK =	0x1f;

/* Not part of protocol, but needed to simplify things... */
constant	MODE_FLOW =	0x0100;
constant	MODE_ECHO =	0x0200;
constant	MODE_INBIN =	0x0400;
constant	MODE_OUTBIN =	0x0800;
constant	MODE_FORCE =	0x1000;

constant	SLC_SYNCH=	1;
constant	SLC_BRK=	2;
constant	SLC_IP=		3;
constant	SLC_AO=		4;
constant	SLC_AYT=	5;
constant	SLC_EOR=	6;
constant	SLC_ABORT=	7;
constant	SLC_EOF=	8;
constant	SLC_SUSP=	9;
constant	SLC_EC=		10;
constant	SLC_EL=		11;
constant	SLC_EW=		12;
constant	SLC_RP=		13;
constant	SLC_LNEXT=	14;
constant	SLC_XON=	15;
constant	SLC_XOFF=	16;
constant	SLC_FORW1=	17;
constant	SLC_FORW2=	18;

constant	NSLC=		18;

constant	SLC_NOSUPPORT=	0;
constant	SLC_CANTCHANGE=	1;
constant	SLC_VARIABLE=	2;
constant	SLC_DEFAULT=	3;
constant	SLC_LEVELBITS=	0x03;

constant	SLC_FUNC=	0;
constant	SLC_FLAGS=	1;
constant	SLC_VALUE=	2;

constant	SLC_ACK=	0x80;
constant	SLC_FLUSHIN=	0x40;
constant	SLC_FLUSHOUT=	0x20;

constant	OLD_ENV_VAR=	1;
constant	OLD_ENV_VALUE=	0;
constant	NEW_ENV_VAR=	0;
constant	NEW_ENV_VALUE=	1;
constant	ENV_ESC=	2;
constant	ENV_USERVAR=	3;

/*
 * AUTHENTICATION suboptions
 */

/*
 * Who is authenticating who ...
 */
constant	AUTH_WHO_CLIENT= 0;	//! Client authenticating server
constant	AUTH_WHO_SERVER= 1;	//! Server authenticating client
constant	AUTH_WHO_MASK=	 1;

/*
 * amount of authentication done
 */
constant	AUTH_HOW_ONE_WAY=	0;
constant	AUTH_HOW_MUTUAL=	2;
constant	AUTH_HOW_MASK=		2;

constant	AUTHTYPE_NULL=		0;
constant	AUTHTYPE_KERBEROS_V4=	1;
constant	AUTHTYPE_KERBEROS_V5=	2;
constant	AUTHTYPE_SPX=		3;
constant	AUTHTYPE_MINK=		4;
constant	AUTHTYPE_CNT=		5;

constant	AUTHTYPE_TEST=		99;


/*
 * ENCRYPTion suboptions
 */
constant ENCRYPT_IS=		0;	/* I pick encryption type ... */
constant ENCRYPT_SUPPORT=	1;	/* I support encryption types ... */
constant ENCRYPT_REPLY=		2;	/* Initial setup response */
constant ENCRYPT_START=		3;	/* Am starting to send encrypted */
constant ENCRYPT_END=		4;	/* Am ending encrypted */
constant ENCRYPT_REQSTART=	5;	/* Request you start encrypting */
constant ENCRYPT_REQEND=	6;	/* Request you send encrypting */
constant ENCRYPT_ENC_KEYID=	7;
constant ENCRYPT_DEC_KEYID=	8;
constant ENCRYPT_CNT=		9;

constant ENCTYPE_ANY=		0;
constant ENCTYPE_DES_CFB64=	1;
constant ENCTYPE_DES_OFB64=	2;
constant ENCTYPE_CNT=		3;

#define C(X) sprintf("%c",X)
#define C2(X,Y) sprintf("%c%c",X,Y)
#define C3(X,Y,Z) sprintf("%c%c%c",X,Y,Z)

//! Implementation of the TELNET protocol.
class protocol
{
  //! The connection.
  protected object fd;

  //! Mapping containing extra callbacks.
  protected mapping cb;

  //! Value to send to the callbacks.
  protected mixed id;

  //! Write callback.
  protected function(mixed|void:string) write_cb;

  //! Read callback.
  protected function(mixed,string:void) read_cb;

  //! Close callback.
  protected function(mixed|void:void) close_cb;

  // See RFC 1143 for the use and meaning of these.

  protected constant UNKNOWN = 0;
  protected constant YES = 1;
  protected constant NO = 2;
  protected constant WANT = 4;
  protected constant OPPOSITE = 8;

  //! Negotiation states of all WILL/WON'T options.
  //! See RFC 1143 for a description of the states.
  protected array(int) remote_options = allocate(256,NO);
  protected array(int) local_options = allocate(256,NO);


  //! Data queued to be sent.
  protected string to_send = "";

  //! Indicates that connection should be closed
  protected int done;

  //! Tells if we have set the nonblocking write callback or not.
  protected int nonblocking_write;

  //! Turns on the write callback if apropriate.
  protected void enable_write()
  {
    DWRITE("TELNET: enable_write()\n");
    if (!nonblocking_write && (write_cb || sizeof(to_send) || done)) {
      DWRITE("TELNET: enable_write(): Enabling non-blocking() in enable_write()\n");
      fd->set_nonblocking(got_data, send_data, close_cb, got_oob);
      nonblocking_write = 1;
    } else {
      DWRITE("TELNET: enable_write(): Calling send_data()\n");
      send_data();
    }
  }

  //! Turns off the write callback if apropriate.
  protected void disable_write()
  {
    DWRITE("TELNET: disable_write()\n");
    if (!write_cb && !sizeof(to_send) && !done && nonblocking_write) {
      DWRITE("TELNET: disable_write(): Calling fd->set_nonblocking()\n");
      fd->set_nonblocking(got_data, 0, close_cb, got_oob);
      nonblocking_write = 0;
    }
  }

  //! Queues data to be sent to the other end of the connection.
  //!
  //! @param s
  //!   String to send.
  void write(string s)
  {
    DWRITE(sprintf("TELNET: writing :%O\n",s));
    to_send += replace(s, C(IAC), C2(IAC,IAC));
    enable_write();
  }

  //! Queues raw data to be sent to the other end of the connection.
  //!
  //! @param s
  //!   String with raw telnet data to send.
  void write_raw(string s)
  {
    to_send += s;
    enable_write();
  }

  //! Closes the connetion neatly
  void close()
  {
    done=1;
    enable_write();
  }

  //! Callback called when it is clear to send data over the connection.
  //! This function does the actual sending.
  protected void send_data()
  {
    DWRITE("TELNET: Entering send_data()\n");
    if (!sizeof(to_send)) {
      DWRITE("TELNET: Nothing to send!\n");
      if (write_cb) {
	DWRITE("TELNET: We have a write callback!\n");
	// FIXME: What if the callback calls something that calls
	//        write() or write_raw() above?
	if(!(to_send = write_cb(id)))
	{
	  DWRITE("TELNET: Write callback did not write anything!\n");
	  done=1;
	  to_send="";
	}
      }
    }

    if (sizeof(to_send))
    {
      DWRITE(sprintf("TELNET: We now have data to send! (%d bytes)\n",
		     sizeof(to_send)));
      if (to_send[0] == 242) {
	// DataMark needs extra quoting... Stupid.
	DWRITE("TELNET: Found datamark @ offset 0!\n");
	to_send = C2(IAC,NOP) + to_send;
      }

      int n = fd->write(to_send);

      to_send = to_send[n..];
    } else if(done) {
      DWRITE("TELNET: Closing fd!\n");
      fd->close();
      fd=0;
      nonblocking_write=0;
      return;
    }
    disable_write();
  }

  //! Sends a TELNET synch command.
  void send_synch()
  {
    // Clear send-queue.
    to_send = "";

    if (fd->write_oob) {
      fd->write_oob(C(IAC));

      fd->write(C(DM));
    } else {
      // Fallback...
      fd->write(C2(IAC,DM));
    }
  }

  //! @decl void send_DO(int option)
  //!
  //! Starts negotiation to enable a TELNET option.
  //!
  //! @param option
  //!   The option to enable.

  //! @decl void send_DONT(int option)
  //!
  //! Starts negotiation to disable a TELNET option.
  //!
  //! @param option
  //!   The option to disable.

#define CONTROL(OPTIONS,DO,DONT,WILL,WONT,YES,NO)			\
  void send_##DO(int option)						\
  {									\
    if ((option < 0) || (option > 255)) {				\
      error( "Bad TELNET option #%d\n", option);			\
    }									\
    DWRITE(sprintf("TELNET: send_" #DO "(%s) state is %d\n",		\
		    lookup_telopt[option] || (string)option,		\
		    OPTIONS##_options[option]));			\
    switch(OPTIONS##_options[option]) {					\
    case NO:								\
    case UNKNOWN:							\
	OPTIONS##_options[option]= WANT | YES;				\
        DWRITE(sprintf("TELNET: => " #DO " %s\n",			\
		       lookup_telopt[option] || (string)option));	\
	to_send += sprintf("%c%c%c",IAC,DO,option);			\
	break;								\
									\
    case YES: /* Already enabled */					\
    case WANT | YES: /* Will be enabled soon */				\
    case WANT | NO | OPPOSITE: /* Already queued an enable request. */	\
      break;								\
      									\
    case WANT | NO:							\
    case WANT | YES | OPPOSITE:						\
      OPTIONS##_options[option]^=OPPOSITE;				\
      break;								\
      									\
    default:								\
      error("TELNET: Strange remote_options[%d]=%d\n",			\
	    option, remote_options[option]);				\
      /* ERROR: weird state! */						\
      break;								\
    }									\
    enable_write();							\
  }

  //! @ignore
  CONTROL(remote,DO,DONT,WILL,WONT,YES,NO)

  CONTROL(remote,DONT,DO,WONT,WILL,NO,YES)

  CONTROL(local,WILL,WONT,DO,DONT,YES,NO)

  CONTROL(local,WONT,WILL,DONT,DO,NO,YES)
  //! @endignore

  void remote_option_callback(int opt, int onoff)
  {
    call_callback("Remote "+ lookup_telopt[opt], onoff);
    call_callback("Remote option", onoff);
  }

  void local_option_callback(int opt, int onoff)
  {
    call_callback("Local "+ lookup_telopt[opt], onoff);
    call_callback("Local option", onoff);
  }

  void set_remote_option(int opt, int state)
  {
    remote_options[opt]=state;
    switch(state)
    {
      case YES: remote_option_callback(opt,1); break;
      case NO:  remote_option_callback(opt,0); break;
    }
  }

  void set_local_option(int opt, int state)
  {
    local_options[opt]=state;
    switch(state)
    {
      case YES: local_option_callback(opt,1); break;
      case NO:  local_option_callback(opt,0); break;
    }
  }

  int WILL_callback(int opt)
  {
    return call_callback(WILL,opt);
  }

  int DO_callback(int opt)
  {
    return call_callback(DO,opt);
  }

  void send_SB(int|string ... args)
  {
    to_send+=
      C2(IAC,SB)+
      replace(Array.map(args,lambda(int|string s)
			{
			  return sprintf(intp(s)?"%c":"%s",s);
			})*"",C(IAC),C2(IAC,IAC))+
	  C2(IAC,SE);
    enable_write();
  }
  

  //! Indicates whether we are in synch-mode or not.
  protected int synch = 0;

  protected mixed call_callback(mixed what, mixed ... args)
  {
    if(mixed cb=cb[what])
    {
      mixed err=catch {
	return cb(@args);
      };
      if(err)
      {
	throw(err);
      }else{
	if(!this) return 0;
	throw(err);
      }
    }
    switch(what)
    {
    case BREAK:
      destruct(this);
      throw(0);
      break;

    case AYT:
      to_send += C2(IAC,NOP);	// NOP
      enable_write();
      break;
    }
  }

  //! Callback called when Out-Of-Band data has been received.
  //!
  //! @param ignored
  //!   The id from the connection.
  //!
  //! @param s
  //!   The Out-Of-Band data received.
  //!
  protected void got_oob(mixed ignored, string s)
  {
#ifdef TELNET_DEBUG
  werror("TELNET: got_oob(\"%s\")\n",Array.map(values(s),lambda(int s) { 
    switch(s)
    {
      case ' '..'z':
	return sprintf("%c",s);
	
      default:
	return sprintf("\\0x%02x",s);
    }
  })*"");
#endif
    synch = synch || (s == C(IAC));
    call_callback("URG",id,s);
  }

  //! Calls @[read_cb()].
  //!
  //! Specifically provided for overloading
  //!
  protected void call_read_cb(string data)
  {
    DWRITE("TELNET: Fnurgel!\n");
    if(read_cb && sizeof(data)) read_cb(id,data);
  }

  //! Callback called when normal data has been received.
  //! This function also does most of the TELNET protocol parsing.
  //!
  //! @param ignored
  //!   The id from the connection.
  //! @param s
  //!   The received data.
  //!
  protected void got_data(mixed ignored, string line)
  {
#ifdef TELNET_DEBUG
    werror("TELNET: got_data(%O)\n",line);
#endif

    if (sizeof(line) && (line[0] == DM)) {
      DWRITE("TELNET: Data Mark\n");
      // Data Mark handing.
      line = line[1..];
      synch = 0;
    }

    if (has_value(line, C(IAC))) {
      array a = line / C(IAC);

      string parsed_line = a[0];
      int i;
      for (i=1; i < sizeof(a); i++) {
	string part = a[i];
	if (sizeof(part)) {

	  DWRITE(sprintf("TELNET: Code %s\n",
			 lookup_telnetcodes[part[0]] || (string)part[0]));

	  switch (part[0]) {
	  default:
	    call_callback(part[0]);
	    a[i] = a[i][1..];
	    break;

	    // FIXME, find true end of subnegotiation!
	  case SB:
	    call_callback(SB,part[1..]);
	    a[i] = "";
	    break;

	  case EC:	// Erase Character
	    for (int j=i; j--;) {
	      if (sizeof(a[j])) {
		a[j] = a[j][..<1];
		break;
	      }
	    }
	    a[i] = a[i][1..];
	    break;

#if 0
	  case EL:	// Erase Line
	    for (int j=0; j < i; j++) {
	      a[j] = "";
	    }
	    a[i] = a[i][1..];
	    break;
#endif

#define HANDLE(OPTIONS,WILL,WONT,DO,DONT)				\
	  case WILL:							\
	  {								\
	    int option = a[i][1];					\
	    int state = OPTIONS##_options[option];			\
	    a[i] = a[i][2..];						\
	  								\
	    DWRITE(sprintf(#WILL " %s, state 0x%04x\n",			\
	  		     lookup_telopt[option], state));		\
	    								\
	    switch(state) {						\
	    case NO:							\
	    case UNKNOWN:						\
	  	if (WILL##_callback(option))				\
	  	{							\
	  	  /* Agree about enabling */				\
	  	  state=YES;						\
	  	  send_##DO(option);					\
	  	} else {						\
	  	  state=NO;						\
	  	  send_##DONT(option);					\
	  	}							\
	  	break;							\
	  	  							\
	    case YES:							\
	  	/* Ignore */						\
	  	break;							\
	  								\
	    case WANT | NO:						\
	  	state=NO;						\
	  	break;							\
	  								\
	    case WANT | YES:						\
	    case WANT | NO | OPPOSITE:					\
	  	state=YES;						\
	  	break;							\
	  								\
	    case WANT | YES | OPPOSITE:					\
	  	state=WANT | NO;					\
	  	send_##DONT(option);					\
	  	break;							\
	  								\
	    default:							\
	  	error("TELNET: Strange remote_options[%d]=%d\n",	\
	  	      option,remote_options[option]);			\
	  	/* Weird state ! */					\
	    }								\
	    DWRITE(sprintf("TELNET: => " #WILL " %s, state 0x%04x\n",	\
	  		     lookup_telopt[option], state));		\
	    set_##OPTIONS##_option(option,state);			\
	    break;							\
	  }								\
	  								\
	  case WONT:							\
	  {								\
	    int option = a[i][1];					\
	    int state = OPTIONS##_options[option];			\
	    a[i] = a[i][2..];						\
	    								\
	    DWRITE(sprintf(#WONT " %s, state 0x%04x\n",			\
	  		     lookup_telopt[option], state));		\
	    								\
	    switch(state)						\
	    {								\
	    case UNKNOWN:						\
	    case NO:							\
	  	state=NO;						\
	  	break;							\
	  								\
	    case YES:							\
	  	state=NO;						\
	  	send_##DONT(option);					\
	  	break;							\
	  								\
	    case WANT | NO:						\
	  	state=NO;						\
	  	break;							\
	  								\
	    case WANT | NO | OPPOSITE:					\
	  	state=WANT | YES;					\
	  	send_##DO(option);					\
	  	break;							\
	  								\
	    case WANT | YES:						\
	    case WANT | YES | OPPOSITE:					\
	  	state=NO;						\
	  	break;							\
	  								\
	    default:							\
	  	error("TELNET: Strange remote_options[%d]=%d\n",	\
	  	      option, remote_options[option]);			\
	  	/* Weird state */					\
	    }								\
	    								\
	    DWRITE(sprintf("TELNET: => " #WONT " %s, state 0x%04x\n",	\
	  		     lookup_telopt[option], state));		\
	    set_##OPTIONS##_option(option,state);			\
	  }								\
	  break

	  HANDLE(remote,WILL,WONT,DO,DONT);
	  HANDLE(local,DO,DONT,WILL,WONT);

	  case DM:	// Data Mark
	    if (synch) {
	      for (int j=0; j < i; j++) {
		a[j] = "";
	      }
	    }
	    a[i] = a[i][1..];
	    synch = 0;
	    break;
	  }
	} else {
	  // IAC IAC => IAC
	  a[i] = C(IAC);
	  i++;
	}
      }
//      werror("%O\n",a);
      line = a * "";
    }

    if ((!synch)) {
#ifdef TELNET_DEBUG
      werror("TELNET: calling read_callback(X,%O)\n", line);
#endif
      call_read_cb(line);
    }
    enable_write();
  }

  //! Sets the callback to be called when it is clear to send.
  //!
  //! @param w_cb
  //!   The new write callback.
  void set_write_callback(function(mixed|void:string) w_cb)
  {
    DWRITE(sprintf("TELNET: set_write_callback(%O)\n", w_cb));
    write_cb = w_cb;
    if (w_cb) {
      enable_write();
    } else {
      disable_write();
    }
  }

  //! Called when the initial setup is done.
  //!
  //! Created specifically for overloading
  //!
  void setup()
  {
  }

  //! Creates a TELNET protocol handler, and sets its callbacks.
  //!
  //! @param f
  //!   File to use for the connection.
  //! @param r_cb
  //!   Function to call when data has arrived.
  //! @param w_cb
  //!   Function to call when the send buffer is empty.
  //! @param c_cb
  //!   Function to call when the connection is closed.
  //! @param callbacks
  //!   Mapping with callbacks for the various TELNET commands.
  //! @param new_id
  //!   Value to send to the various callbacks.
  //!
  void create(object f,
	      function(mixed,string:void) r_cb,
	      function(mixed|void:string) w_cb,
	      function(mixed|void:void) c_cb,
	      mapping callbacks, mixed|void new_id)
  {
    fd = f;
    cb = callbacks;

    read_cb = r_cb;
    write_cb = w_cb;
    close_cb = c_cb;
    id = new_id;

    fd->set_nonblocking(got_data, w_cb && send_data, close_cb, got_oob);
    nonblocking_write = !!w_cb;
    setup();
  }

  void set_nonblocking(function r_cb, function w_cb, function c_cb)
  {
    read_cb = r_cb;
    write_cb = w_cb;
    close_cb = c_cb;
    DWRITE(sprintf("TELNET: set_nonblocking(): "
		   "Calling fd->set_nonblocking() %O %O\n",
		   w_cb, w_cb || send_data));
    fd->set_nonblocking(got_data, w_cb && send_data, close_cb, got_oob);
    nonblocking_write = !!w_cb;
  }

  void set_blocking()
  {
    read_cb = 0;
    write_cb = 0;
    close_cb = 0;
    fd->set_blocking();
    nonblocking_write = 0;
  }

  string query_address(int|void remote)
  {
    return fd->query_address(remote);
  }
}

//! Line-oriented TELNET protocol handler.
class LineMode
{
  //! Based on the generic TELNET protocol handler.
  inherit protocol;

  protected string line_buffer="";

  protected void call_read_cb(string data)
  {
    if(read_cb)
    {
      DWRITE(sprintf("TELNET: Line callback... %O\n",data));
      data=replace(data,
		   ({"\r\n", "\n", "\r", "\r\0"}),
		   ({"\r",   "\r", "\r", "\r",}));
      line_buffer+=data;
      array(string) tmp=line_buffer/"\r";
      line_buffer=tmp[-1];
      for(int e=0;e<sizeof(tmp)-1;e++) read_cb(id,tmp[e]+"\n");
    }
  }

  //! Perform the initial TELNET handshaking for LINEMODE.
  void setup()
  {
    send_DO(TELOPT_BINARY);
    send_DO(TELOPT_SGA);
    send_DONT(TELOPT_LINEMODE);
    send_WILL(TELOPT_SGA);
  }
}

//! Line-oriented TELNET protocol handler with @[Stdio.Readline] support.
class Readline
{
  //! Based on the Line-oriented TELNET protocol handler.
  inherit LineMode;

  object readline;
  
  string term;
  int width=80;
  int height=24;
  int icanon;
  
  mapping tcgetattr()
  {
    return ([
      "rows":height,
      "columns":width,
      "ECHO":local_options[TELOPT_ECHO],
      "ICANON": icanon,
      ]);
  }
  
  protected void call_read_cb(string data)
  {
    if(read_cb)
    {
      if(!icanon)
      {
	if(sizeof(data)) read_cb(id,data);
      }else{
	DWRITE(sprintf("TELNET: Line callback... %O\n",data));
	data=replace(data,
		     ({"\r\n","\r\n","\r","\r\0"}),
		     ({"\r",  "\r",  "\r","\r",}));
	line_buffer+=data;
	array(string) tmp=line_buffer/"\r";
	line_buffer=tmp[-1];
	for(int e=0;e<sizeof(tmp)-1;e++)
	{
	  read_cb(id,tmp[e]+"\n");
	  write(prompt);
	}
      }
    }
  }
  
  int tcsetattr(mapping options)
  {
    ( options->ECHO ? send_WONT : send_WILL )(TELOPT_ECHO);
    ( (icanon=options->ICANON) ? send_DONT : send_DO )(TELOPT_LINEMODE);
  }

  void set_secret(int onoff)
  {
    DWRITE(sprintf("TELNET: set_secret(%d)\n", onoff));
    if(readline)
    {
      DWRITE("TELNET: setting secret via Stdio.Readline\n");
      readline->set_echo(!onoff);
    }else{
      DWRITE("TELNET: setting secret via telnet option\n");
      ( onoff ? send_WILL : send_WONT )(TELOPT_ECHO);
    }
    DWRITE(sprintf("TELNET: LOCAL TELNET ECHO STATE IS %O\n", local_options[TELOPT_ECHO]));
    DWRITE(sprintf("TELNET: REMOTE TELNET ECHO STATE IS %O\n", remote_options[TELOPT_ECHO]));

  }
  
  protected function(mixed,string:void) read_cb2;
  
  protected void readline_callback(string data)
  {
    read_cb2(id,data+"\n");
  }
  
  protected string prompt="";
  protected mixed call_callback(mixed what, mixed ... args)
  {
    switch(what)
    {
      case SB:
	string data=args[0];
	DWRITE(sprintf("TELNET: SB callback %O\n",data));
	switch(data[0])
	{
	  case TELOPT_TTYPE:
	    if(data[1]==TELQUAL_IS)
	    {
	      if(!read_cb2)
	      {
		read_cb2=read_cb;
		term=data[2..];
 		DWRITE(sprintf("TELNET.Readline: Enabling READLINE, term=%s\n",
			       term));
		// This fix for the secret mode might not
		// be the best way to do things, but it seems to
		// work.
		int secret_mode = local_options[TELOPT_ECHO] != NO;
		set_secret(0);
		readline=Stdio.Readline(this,lower_case(term));
		set_secret(secret_mode);
		DWRITE("TELNET.Readline: calling readline->set_nonblocking()\n");
		readline->set_nonblocking(readline_callback);
		DWRITE(sprintf("TELNET: Setting the readline prompt to %O\n", prompt));
		readline->set_prompt(prompt);
		DWRITE(sprintf("TELNET: LOCAL TELNET ECHO STATE IS %O\n", local_options[TELOPT_ECHO]));
		DWRITE(sprintf("TELNET: REMOTE TELNET ECHO STATE IS %O\n", remote_options[TELOPT_ECHO]));

		readline->enable_history(200);
		/* enable data processing */
	      }
	    }
	    break;
	    
	  case TELOPT_NAWS:
	    if(sscanf(data[1..],"%2c%2c",width,height)==2)
	      if(readline)
		readline->redisplay();
	    break;
	}
    }
  }

  void remote_option_callback(int opt, int onoff)
  {
    switch(opt)
    {
      case TELOPT_TTYPE:
	if(onoff)
	{
	  send_SB(TELOPT_TTYPE,TELQUAL_SEND);
	}else{
	  werror("Revert to line mode not yet operational.\n");
	  /* revert to stupid line mode */
	}
    }
    ::remote_option_callback(opt,onoff);
  }
  
  void setup()
  {
    send_DO(TELOPT_SGA);
    send_DO(TELOPT_BINARY);
    send_DO(TELOPT_NAWS);
    send_DO(TELOPT_TTYPE);
    /* disable data processing */
  }

  //! Write a message.
  void message(string s,void|int word_wrap)
  {
    if(readline)
    {
      readline->write(s,word_wrap);
    }else{
      write(replace(s,"\n","\r\n"));
    }
  }

  //! Set the readline prompt.
  void set_prompt(string s)
  {
    if(readline)
    {
      DWRITE(sprintf("TELNET: Setting readline prompt to %O\n", s));
      prompt=s;
      readline->set_prompt(prompt);
    }else{
      if(prompt!=s)
      {
	DWRITE(sprintf("TELNET: Setting prompt without readline to %O\n", s));

	// What is the point of this if-statement?
	// I think that write(s) should be called every time!
	// Agehall 2004-04-25
	//
	// No idea; it looks like it checks if the old prompt
	// is a prefix of the new prompt. It also probably ought
	// to use message() rather than write().
	// Grubba 2008-12-04
	if(s[..sizeof(prompt)-1]==prompt)
	  write(s);
	prompt=s;
      }
    }
  }

  //! Close the connection.
  void close()
  {
    readline->set_blocking();
    readline=0;
    ::close();
  }
}
