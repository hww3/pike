// $Id$
#pike __REAL_VERSION__

inherit files;

#ifdef SENDFILE_DEBUG
#define SF_WERR(X) werror("Stdio.sendfile(): %s\n", X)
#else
#define SF_WERR(X)
#endif

//#define BACKEND_DEBUG
#ifdef BACKEND_DEBUG
#define BE_WERR(X) werror("FD %O: %s\n", _fd, X)
#else
#define BE_WERR(X)
#endif

// STDIO_DIRECT_FD is a work in progress to get rid of Stdio.Fd_ref, where
// Stdio.File et al instead inherit Stdio.Fd directly.
// #define STDIO_DIRECT_FD

// TRACK_OPEN_FILES is a debug tool to track down where a file is
// currently opened from (see report_file_open_places). It's used
// primarily when debugging on NT since an opened file can't be
// renamed or removed there.
#ifndef TRACK_OPEN_FILES
#define register_open_file(file, id, backtrace)
#define register_close_file(id)
#endif

protected constant LineIterator = __builtin.file_line_iterator;

final constant DATA_CHUNK_SIZE = 64 * 1024;
//! Size used in various places to divide incoming or outgoing data
//! into chunks.

//! The Stdio.Stream API.
//!
//! This class exists purely for typing reasons.
//!
//! Use in types in place of @[Stdio.File] where only blocking stream-oriented
//! I/O is done with the object.
//!
//! @seealso
//! @[NonblockingStream], @[BlockFile], @[File], @[FILE]
//!
class Stream
{
  string read(int nbytes);
  int write(string data);
  void close();
  optional string read_oob(int nbytes);
  optional int write_oob(string data);
  optional mapping(string:int) tcgetattr();
  optional int tcsetattr(mapping(string:int) attr, string|void when);
}

//! Argument to @[Stdio.File()->tcsetattr()].
//!
//! Change immediately.
constant TCSANOW = "TCSANOW";

//! Argument to @[Stdio.File()->tcsetattr()].
//!
//! Change after all output has been written.
constant TCSADRAIN = "TCSADRAIN";

//! Argument to @[Stdio.File()->tcsetattr()].
//!
//! Change after all output has been written,
//! and empty the input buffers.
constant TCSAFLUSH = "TCSAFLUSH";

//! The Stdio.NonblockingStream API.
//!
//! This class exists purely for typing reasons.
//!
//! Use in types in place of @[Stdio.File] where nonblocking and/or blocking
//! stream-oriented I/O is done with the object.
//! 
//! @seealso
//! @[Stream], @[BlockFile], @[File], @[FILE]
//!
class NonblockingStream
{
  inherit Stream;
  NonblockingStream set_read_callback( function f, mixed ... rest );
  NonblockingStream set_write_callback( function f, mixed ... rest );
  NonblockingStream set_close_callback( function f, mixed ... rest );

  optional NonblockingStream set_read_oob_callback(function f, mixed ... rest)
  {
    error("OOB not implemented for this stream type\n");
  }
  optional NonblockingStream set_write_oob_callback(function f, mixed ... rest)
  {
    error("OOB not implemented for this stream type\n");
  }

  void set_nonblocking( function a, function b, function c,
                        function|void d, function|void e);
  void set_blocking();
}

//! The Stdio.BlockFile API.
//!
//! This class exists purely for typing reasons.
//!
//! Use in types in place of @[Stdio.File] where only blocking
//! I/O is done with the object.
//! 
//! @seealso
//! @[Stream], @[NonblockingStream], @[File], @[FILE]
//!
class BlockFile
{
  inherit Stream;
  int seek(int to);
  int tell();
}

//! This is the basic I/O object, it provides socket and pipe
//! communication as well as file access. It does not buffer reads and
//! writes or provide line-by-line reading, that is done with
//! @[Stdio.FILE] object.
//!
//! @note
//! The file or stream will normally be closed when this object is
//! destructed (unless there are more objects that refer to the same
//! file through use of @[assign] or @[dup]). Objects do not contain
//! cyclic references in themselves, so they will be destructed timely
//! when they run out of references.
//!
//! @seealso
//! @[Stdio.FILE]
class File
{
#ifndef STDIO_DIRECT_FD
  optional inherit Fd_ref;
#else
  optional inherit Fd;

  // This is needed in case we get overloaded by strange code
  // (socktest.pike).
  protected Fd fd_factory()
  {
    return File()->_fd;
  }
#endif
  
#ifdef TRACK_OPEN_FILES
  /*static*/ int open_file_id = next_open_file_id++;
#endif

  int is_file;

  function(mixed|void,string|void:int) ___read_callback;
  function(mixed|void:int) ___write_callback;
  function(mixed|void:int) ___close_callback;
  function(mixed|void,string|void:int) ___read_oob_callback;
  function(mixed|void:int) ___write_oob_callback;
  mixed ___id;

#ifdef __STDIO_DEBUG
  string __closed_backtrace;
#define CHECK_OPEN()							\
  if(!is_open())							\
  {									\
    error( "Stdio.File(): line "+__LINE__+" on closed file.\n" +	\
	   (__closed_backtrace ?					\
	    sprintf("File was closed from:\n"				\
		    "    %-=200s\n",					\
		    __closed_backtrace) :				\
	    "This file has never been open.\n" ) );			\
  }
#else
#define CHECK_OPEN()
#endif

  //! Returns the error code for the last command on this file.
  //! Error code is normally cleared when a command is successful.
  //!
  int errno()
  {
    return ::errno();
  }

  protected string|int debug_file;
  protected string debug_mode;
  protected int debug_bits;

  optional void _setup_debug( string f, string m, int|void b )
  {
    debug_file = f;
    debug_mode = m;
    debug_bits = b;
  }

  protected string _sprintf( int type, mapping flags )
  {
    if(type!='O') return 0;
    return sprintf("%O(%O, %O, %o /* fd=%d */)",
		   this_program,
		   debug_file, debug_mode,
		   debug_bits||0777,
		   _fd && is_open() ? query_fd() : -1 );
  }

  //  @decl int open(int fd, string mode)
  //! @decl int open(string filename, string mode)
  //! @decl int open(string filename, string mode, int mask)
  //!
  //! Open a file for read, write or append. The parameter @[mode] should
  //! contain one or more of the following letters:
  //! @string
  //!   @value "r"
  //!   Open file for reading.
  //!   @value "w"
  //!   Open file for writing.
  //!   @value "a"
  //!   Open file for append (use with @expr{"w"@}).
  //!   @value "t"
  //!   Truncate file at open (use with @expr{"w"@}).
  //!   @value "c"
  //!   Create file if it doesn't exist (use with @expr{"w"@}).
  //!   @value "x"
  //!   Fail if file already exists (use with @expr{"c"@}).
  //! @endstring
  //!
  //! @[mode] should always contain at least one of the letters
  //! @expr{"r"@} or @expr{"w"@}.
  //!
  //! The parameter @[mask] is protection bits to use if the file is
  //! created. Default is @expr{0666@} (read+write for all in octal
  //! notation).
  //!
  //! @returns
  //! This function returns @expr{1@} for success, @expr{0@} otherwise.
  //!
  //! @seealso
  //! @[close()], @[create()]
  //!
  int open(string file, string mode, void|int bits)
  {
    is_file = 1;
#ifdef __STDIO_DEBUG
    __closed_backtrace=0;
#endif
    if (zero_type(bits)) bits=0666;
    debug_file = file;  debug_mode = mode;
    debug_bits = bits;
    if (::open(file,mode,bits)) {
      register_open_file (file, open_file_id, backtrace());
      fix_internal_callbacks();
      return 1;
    }
    return 0;
  }

#if constant(files.__HAVE_OPENPT__)
  //! @decl int openpt(string mode)
  //!
  //! Open the master end of a pseudo-terminal pair.  The parameter
  //! @[mode] should contain one or more of the following letters:
  //! @string
  //!   @value "r"
  //!   Open terminal for reading.
  //!   @value "w"
  //!   Open terminal for writing.
  //! @endstring
  //!
  //! @[mode] should always contain at least one of the letters
  //! @expr{"r"@} or @expr{"w"@}.
  //!
  //! @seealso
  //! @[grantpt()]
  //!
  int openpt(string mode)
  {
    is_file = 0;
#ifdef __STDIO_DEBUG
    __closed_backtrace=0;
#endif
    debug_file = "pty master";  debug_mode = mode; debug_bits=0;
    if (::openpt(mode)) {
      register_open_file ("pty master", open_file_id, backtrace());
      fix_internal_callbacks();
      return 1;
    }
    return 0;
  }
#endif

  //! This makes this file into a socket ready for connections. The reason
  //! for this function is so that you can set the socket to nonblocking
  //! or blocking (default is blocking) before you call @[connect()].
  //!
  //! @param port
  //!   If you give a port number to this function, the socket will be
  //!   bound to this port locally before connecting anywhere. This is
  //!   only useful for some silly protocols like @b{FTP@}. The port can
  //!   also be specified as a string, giving the name of the service
  //!   associated with the port.
  //!
  //! @param address
  //!   You may specify an address to bind to if your machine has many IP
  //!   numbers.
  //!
  //! @param family_hint
  //!   A protocol family for the socket can be specified. If no family is
  //!   specified, one which is appropriate for the address is automatically
  //!   selected. Thus, there is normally no need to specify it.  If you
  //!   do not want to specify a bind address, you can provide the address
  //!   as a hint here instead, to allow the automatic selection to work
  //!   anyway.
  //!
  //! @returns
  //! This function returns 1 for success, 0 otherwise.
  //!
  //! @seealso
  //! @[connect()], @[set_nonblocking()], @[set_blocking()]
  //!
  int open_socket(int|string|void port, string|void address,
		  int|string|void family_hint)
  {
    is_file = 0;
#ifdef __STDIO_DEBUG
    __closed_backtrace=0;
#endif
    debug_file="socket";
    debug_mode=0; debug_bits=0;
    int ok;
    switch(query_num_arg()) {
    case 0:
      ok = ::open_socket();
      break;
    case 1:
      ok = ::open_socket(port);
      break;
    case 2:
      ok = ::open_socket(port, address);
      break;
    default:
      ok = ::open_socket(port, address, family_hint);
      break;
    }
    if (ok) {
      register_open_file ("socket", open_file_id, backtrace());
      fix_internal_callbacks();
    }
    return ok;
  }

  //! This function connects a socket previously created with
  //! @[open_socket()] to a remote socket through TCP/IP. The
  //! @[host] argument is the hostname or IP number of the remote machine.
  //! A local IP and port can be explicitly bound by specifying @[client]
  //! and @[client_port].
  //!
  //! @returns
  //! This function returns 1 for success, 0 otherwise.
  //!
  //! @note
  //! In nonblocking mode @expr{0@} (zero) may be returned and
  //! @[errno()] set to @expr{EWOULDBLOCK@} or
  //! @expr{WSAEWOULDBLOCK@}. This should not be regarded as a
  //! connection failure. In nonblocking mode you need to wait for a
  //! write or close callback before you know if the connection failed
  //! or not.
  //!
  //! @seealso
  //! @[query_address()], @[async_connect()], @[connect_unix()]
  //!
  int connect(string host, int|string port,
	      void|string client, void|int|string client_port)
  {
#ifdef __STDIO_DEBUG
    __closed_backtrace=0;
#endif
    is_file = 0;
    debug_file = "socket";
    debug_mode = host+":"+port; 
    debug_bits = 0;
    if(!client) {
      if (::connect(host, port)) {
	register_open_file ("socket", open_file_id, backtrace());
	fix_internal_callbacks();
	return 1;
      }
    }
    else
      if (::connect(host, port, client, client_port)) {
	register_open_file ("socket", open_file_id, backtrace());
	fix_internal_callbacks();
	return 1;
      }
    return 0;
  }

#if constant(files.__HAVE_CONNECT_UNIX__)
  int connect_unix(string path)
  //! Open a UNIX domain socket connection to the specified destination.
  //! 
  //! @returns
  //!  Returns @expr{1@} on success, and @expr{0@} on failure.
  //!
  //! @note
  //!  Nonblocking mode is not supported while connecting
  {
#ifdef __STDIO_DEBUG
    __closed_backtrace=0;
#endif
    is_file = 0;
    debug_file = "unix_socket";
    debug_mode = path;
    debug_bits = 0;
    if (::connect_unix( path )) {
      register_open_file ("unix_socket", open_file_id, backtrace());
      fix_internal_callbacks();
      return 1;
    }
    return 0;
  }
#endif

  protected private function(int, mixed ...:void) _async_cb;
  protected private array(mixed) _async_args;
  protected private void _async_check_cb(mixed|void ignored)
  {
    // Copy the args to avoid races.
    function(int, mixed ...:void) cb = _async_cb;
    array(mixed) args = _async_args;
    _async_cb = 0;
    _async_args = 0;
    set_nonblocking(0,0,0,0,0);
    if (cb) {
      if (is_open() && query_address()) {
	// Connection OK.
	cb(1, @args);
      } else {
	// Connection failed.
	// Make sure the state is reset.
	close();
	cb(0, @args);
      }
    }
  }

  function(:string) read_function(int nbytes)
  //! Returns a function that when called will call @[read] with
  //! nbytes as argument. Can be used to get various callback
  //! functions, eg for the fourth argument to
  //! @[String.SplitIterator].
  {
    return lambda(){ return read(nbytes); };
  }

  String.SplitIterator|LineIterator line_iterator( int|void trim )
  //! Returns an iterator that will loop over the lines in this file. 
  //! If trim is true, all @tt{'\r'@} characters will be removed from
  //! the input.
  {
    if( trim )
      return String.SplitIterator( "",(<'\n','\r'>),1,read_function(8192));
    // This one is about twice as fast, but it's way less flexible.
    return LineIterator( read_function(8192) );
  }


  //! Open a TCP/IP connection asynchronously.
  //!
  //! This function is similar to @[connect()], but works asynchronously.
  //!
  //! @param host
  //!   Hostname or IP to connect to.
  //!
  //! @param port
  //!   Port number or service name to connect to.
  //!
  //! @param callback
  //!   Function to be called on completion.
  //!   The first argument will be @expr{1@} if a connection was
  //!   successfully estabished, and @expr{0@} (zero) on failure.
  //!   The rest of the arguments to @[callback] are passed
  //!   verbatim from @[args].
  //!
  //! @param args
  //!   Extra arguments to pass to @[callback].
  //!
  //! @returns
  //!   Returns @expr{0@} on failure, and @expr{1@} if @[callback]
  //!   will be used.
  //!
  //! @note
  //!   The socket may be opened with @[open_socket()] ahead of
  //!   the call to this function, but it is not required.
  //!
  //! @note
  //!   This object is put in callback mode by this function. For
  //!   @[callback] to be called, the backend must be active. See e.g.
  //!   @[set_read_callback] for more details about backends and
  //!   callback mode.
  //!
  //! @note
  //!   The socket will be in nonblocking state if the connection is
  //!   successful, and any callbacks will be cleared.
  //!
  //! @seealso
  //!   @[connect()], @[open_socket()], @[set_nonblocking()]
  int async_connect(string host, int|string port,
		    function(int, mixed ...:void) callback,
		    mixed ... args)
  {
    if (!is_open() ||
	!::stat()->issock ||
	catch { throw(query_address()); }) {
      // Open a new socket if:
      //   o We don't have an fd.
      //   o The fd isn't a socket.
      //   o query_address() returns non zero (ie connected).
      //   o query_address() throws an error (eg delayed UDP error) [bug 2691].
      //
      // This code is here to support the socket being opened (and locally
      // bound) by the calling code, to support eg FTP.
      if (!open_socket(-1, 0, host)) {
	// Out of sockets?
	return 0;
      }
    }

    _async_cb = callback;
    _async_args = args;
    set_nonblocking(0, _async_check_cb, _async_check_cb, _async_check_cb, 0);
    mixed err;
    int res;
    if (err = catch(res = connect(host, port))) {
      // Illegal format. -- Bad hostname?
      set_nonblocking(0, 0, 0, 0, 0);
      call_out(_async_check_cb, 0);
    } else if (!res) {
      // Connect failed.
      set_nonblocking(0, 0, 0, 0, 0);
      call_out(_async_check_cb, 0);
    }
    return 1;	// OK so far. (Or rather the callback will be used).
  }

  //! This function creates a pipe between the object it was called in
  //! and an object that is returned.
  //!
  //! @param required_properties
  //!   Binary or (@[predef::`|()]) of required @expr{PROP_@} properties.
  //!   @int
  //!     @value PROP_IPC
  //!       The resulting pipe may be used for inter process communication.
  //!     @value PROP_NONBLOCK
  //!       The resulting pipe supports nonblocking I/O.
  //!     @value PROP_SHUTDOWN
  //!       The resulting pipe supports shutting down transmission in either
  //!       direction (see @[close()]).
  //!     @value PROP_BUFFERED
  //!       The resulting pipe is buffered (usually 4KB).
  //!     @value PROP_BIDIRECTIONAL
  //!       The resulting pipe is bi-directional.
  //!     @value PROP_SEND_FD
  //!       The resulting pipe might support sending of file descriptors
  //!       (see @[send_fd()] and @[receive_fd()] for details).
  //!     @value PROP_REVERSE
  //!       The resulting pipe supports communication "backwards" (but
  //!       not necessarily "forwards", see @[PROP_BIDIRECTIONAL]).
  //!   @endint
  //!   The default is @expr{PROP_NONBLOCK|PROP_BIDIRECTIONAL@}.
  //!
  //! If @[PROP_BIDIRECTIONAL] isn't specified, the read-end is this
  //! object, and the write-end is the returned object (unless
  //! @[PROP_REVERSE] has been specified, in which case it is the other
  //! way around).
  //!
  //! The two ends of a bi-directional pipe are indistinguishable.
  //!
  //! If the File object this function is called in was open to begin with,
  //! it will be closed before the pipe is created.
  //!
  //! @note
  //!   Calling this function with an argument of @tt{0@} is not the
  //!   same as calling it with no arguments.
  //!
  //! @seealso
  //!   @[Process.create_process()], @[send_fd()], @[receive_fd()],
  //!   @[PROP_IPC], @[PROP_NONBLOCK], @[PROP_SEND_FD],
  //!   @[PROP_SHUTDOWN], @[PROP_BUFFERED], @[PROP_REVERSE],
  //!   @[PROP_BIDIRECTIONAL]
  //!
  File pipe(void|int required_properties)
  {
#ifdef __STDIO_DEBUG
    __closed_backtrace=0;
#endif
    is_file = 0;
    if(query_num_arg()==0)
      required_properties=PROP_NONBLOCK | PROP_BIDIRECTIONAL;
    if(Fd fd = ::pipe(required_properties))
    {
#ifndef STDIO_DIRECT_FD
      File o=File();
      o->_fd=fd;
#else
      File o = function_object(fd->read);
#endif
      o->_setup_debug( "pipe", 0 );
      register_open_file ("pipe", open_file_id, backtrace());
      register_open_file ("pipe", o->open_file_id, backtrace());
      fix_internal_callbacks();
      return o;
    }else{
      return 0;
    }
  }

#if constant(files.__HAVE_OPENAT__)
  //! @decl File openat(string filename, string mode)
  //! @decl File openat(string filename, string mode, int mask)
  //!
  //! Open a file relative to an open directory.
  //!
  //! @seealso
  //!   @[File.statat()], @[File.unlinkat()]
  File openat(string filename, string mode, int|void mask)
  {
    if(query_num_arg()<3)
      mask = 0777;
    if(Fd fd = ::openat(filename, mode, mask))
    {
#ifndef STDIO_DIRECT_FD
      File o=File();
      o->_fd=fd;
#else
      File o = function_object(fd->read);
#endif
      string path = combine_path(debug_file||"", filename);
      o->_setup_debug(path, mode, mask);
      register_open_file(path, o->open_file_id, backtrace());
      return o;
    }else{
      return 0;
    }
  }
#endif

#if constant(files.__HAVE_SEND_FD__)
  //!
  int(0..1) send_fd(File|Fd file)
  {
    return ::send_fd(file->_fd);
  }
#endif

  //! @decl void create()
  //! @decl void create(string filename)
  //! @decl void create(string filename, string mode)
  //! @decl void create(string filename, string mode, int mask)
  //! @decl void create(string descriptorname)
  //! @decl void create(int fd)
  //! @decl void create(int fd, string mode)
  //!
  //! There are four basic ways to create a Stdio.File object.
  //! The first is calling it without any arguments, in which case the you'd
  //! have to call @[open()], @[connect()] or some other method which connects
  //! the File object with a stream.
  //!
  //! The second way is calling it with a @[filename] and open @[mode]. This is
  //! the same thing as cloning and then calling @[open()], except shorter and
  //! faster.
  //!
  //! The third way is to call it with @[descriptorname] of @expr{"stdin"@},
  //! @expr{"stdout"@} or @expr{"stderr"@}. This will open the specified
  //! standard stream.
  //!
  //! For the advanced users, you can use the file descriptors of the
  //! systems (note: emulated by pike on some systems - like NT). This is
  //! only useful for streaming purposes on unix systems. This is @b{not
  //! recommended at all@} if you don't know what you're into. Default
  //! @[mode] for this is @expr{"rw"@}.
  //!
  //! @note
  //! Open mode will be filtered through the system UMASK. You
  //! might need to use @[chmod()] later.
  //!
  //! @seealso
  //! @[open()], @[connect()], @[Stdio.FILE],
  protected void create(int|string|void file,void|string mode,void|int bits)
  {
    if (zero_type(file)) {
#ifndef STDIO_DIRECT_FD
      _fd = Fd();
#endif
      return;
    }

    debug_file = file;  
    debug_mode = mode;
    debug_bits = bits;
    switch(file)
    {
      case "stdin":
#ifndef STDIO_DIRECT_FD
	_fd=_stdin;
#ifdef __STDIO_DEBUG
	__closed_backtrace=0;
#endif
#else
	create(0, mode, bits);
	return;
#endif
	break; /* ARGH, this missing break took 6 hours to find! /Hubbe */

      case "stdout":
#ifndef STDIO_DIRECT_FD
	_fd=_stdout;
#ifdef __STDIO_DEBUG
	__closed_backtrace=0;
#endif
#else
	create(1, mode, bits);
	return;
#endif
	break;
	
      case "stderr":
#ifndef STDIO_DIRECT_FD
	_fd=_stderr;
#ifdef __STDIO_DEBUG
	__closed_backtrace=0;
#endif
#else
	create(2, mode, bits);
	return;
#endif
	break;

      case 0..0x7fffffff:
	 if (!mode) mode="rw";
#ifndef STDIO_DIRECT_FD
	_fd=Fd(file,mode);
#else
	::create(file, mode);
#endif
	register_open_file ("fd " + file, open_file_id, backtrace());
#ifdef __STDIO_DEBUG
	__closed_backtrace=0;
#endif
	break;

      default:
#ifndef STDIO_DIRECT_FD
	_fd=Fd();
#endif
	is_file = 1;
#ifdef __STDIO_DEBUG
	__closed_backtrace=0;
#endif
	if(query_num_arg()<3) bits=0666;
	if(!mode) mode="r";
	if (!::open(file,mode,bits))
	   error("Failed to open %O mode %O : %s\n",
		 file,mode,strerror(errno()));
	register_open_file (file, open_file_id, backtrace());
    }
  }

  //! This function takes a clone of Stdio.File and assigns all
  //! variables of this file from it. It can be used together with @[dup()]
  //! to move files around.
  //!
  //! @seealso
  //! @[dup()]
  //!
  int assign(File|Fd o)
  {
    BE_WERR("assign()\n");
    is_file = o->is_file;
#ifndef STDIO_DIRECT_FD
    if((program)Fd == (program)object_program(o))
    {
      _fd = o->dup();
    }else{
      File _o = [object(File)]o;
      _fd = _o->_fd;
      set_read_callback(_o->query_read_callback());
      set_write_callback(_o->query_write_callback());
      set_close_callback(_o->query_close_callback());
      set_read_oob_callback(_o->query_read_oob_callback());
      set_write_oob_callback(_o->query_write_oob_callback());
      set_id(_o->query_id());
    }
#else
    o->dup2(_fd);
    //error("FIXME!\n");
#endif
    return 0;
  }

  //! This function returns a clone of Stdio.File with all variables
  //! copied from this file.
  //!
  //! @note
  //! All variables, even @tt{id@}, are copied.
  //!
  //! @seealso
  //! @[assign()]
  File dup()
  {
    BE_WERR("dup()\n");
#ifndef STDIO_DIRECT_FD
    File to = File();
    to->is_file = is_file;
    to->_fd = _fd;

    to->set_read_callback(query_read_callback());
    to->set_write_callback(query_write_callback());
    to->set_close_callback(query_close_callback());
    to->set_read_oob_callback(query_read_oob_callback());
    to->set_write_oob_callback(query_write_oob_callback());
    to->_setup_debug( debug_file, debug_mode, debug_bits );
    to->set_id(query_id());
    return to;
#else
    return function_object(::dup()->read);
#endif
  }


  //! @decl int close()
  //! @decl int close(string direction)
  //!
  //! Close the file. Optionally, specify "r", "w" or "rw" to close just
  //! the read, just the write or both read and write directions of the file
  //! respectively.
  //!
  //! An exception is thrown if an I/O error occurs.
  //!
  //! @returns
  //! Nonzero is returned if the file wasn't open in the specified
  //! direction, zero otherwise.
  //!
  //! @note
  //! This function will not call the @tt{close_callback@}.
  //!
  //! @seealso
  //! @[open], @[open_socket]
  //!
  int close(void|string how)
  {
    if(::close(how||"rw"))
    {
      // Avoid cyclic refs.
#define FREE_CB(X) _fd->_##X = 0
      FREE_CB(read_callback);
      FREE_CB(write_callback);
      FREE_CB(read_oob_callback);
      FREE_CB(write_oob_callback);

      register_close_file (open_file_id);
#ifdef __STDIO_DEBUG
      __closed_backtrace=master()->describe_backtrace(backtrace());
#endif
      return 1;
    }
    return 0;
  }

#ifdef STDIO_CALLBACK_TEST_MODE
  // Test mode where we are nasty and never return a string longer
  // than one byte in the read callbacks and never let nonblocking
  // writes write more than one byte. Useful to test that the callback
  // stuff really handles packets cut at odd positions.

  int write (string|array(string) s, mixed... args)
  {
    if (!(::mode() & PROP_IS_NONBLOCKING))
      return ::write (s, @args);

    if (arrayp (s)) s *= "";
    if (sizeof (args)) s = sprintf (s, @args);
    return ::write (s[..0]);
  }

  int write_oob (string s, mixed... args)
  {
    if (!(::mode() & PROP_IS_NONBLOCKING))
      return ::write_oob (s, @args);

    if (sizeof (args)) s = sprintf (s, @args);
    return ::write_oob (s[..0]);
  }
#endif

  __deprecated__ this_program set_peek_file_before_read_callback(int(0..1) ignored)
  {
    // This hack is not necessary anymore - the backend now properly
    // ignores events if other callbacks/threads have managed to read
    // the data before the read callback.
    return this;
  }

  // FIXME: No way to specify the maximum to read.
  protected int __stdio_read_callback()
  {
    BE_WERR("__stdio_read_callback()");

    if (!___read_callback) {
      if (___close_callback) {
#if 0
	/* This code only works for the POLL case! */
	if ((peek(0, 1) == -1) && (errno() == System.EPIPE))
#endif /* 0 */
	  return __stdio_close_callback();
      }
      return 0;
    }

    if (!errno()) {
#if 0
      if (!(::mode() & PROP_IS_NONBLOCKING))
	error ("Read callback called on blocking socket!\n"
	       "Callbacks: %O, %O\n"
	       "Id: %O\n",
	       ___read_callback,
	       ___close_callback,
	       ___id);
#endif /* 0 */

      string s;
#ifdef STDIO_CALLBACK_TEST_MODE
      s = ::read (1, 1);
#else
      s = ::read(8192,1);
#endif
      if (s) {
	if(sizeof(s))
	{
	  BE_WERR(sprintf("  calling read callback with %O", s));
	  return ___read_callback(___id, s);
	}
	BE_WERR ("  got eof");
      }

      else {
#if constant(System.EWOULDBLOCK)
	if (errno() == System.EWOULDBLOCK) {
	  // Necessary to reregister since the callback is disabled
	  // until a successful read() has been done.
	  ::set_read_callback(__stdio_read_callback);
	  return 0;
	}
#endif
	BE_WERR ("  got error " + strerror (errno()) + " from read()");
      }
    }
    else
      BE_WERR ("  got error " + strerror (errno()) + " from backend");


    ::set_read_callback(0);
    if (___close_callback) {
      BE_WERR ("  calling close callback");
      return ___close_callback(___id);
    }

    return 0;
  }

  protected int __stdio_close_callback()
  {
    BE_WERR ("__stdio_close_callback()");
#if 0
    if (!(::mode() & PROP_IS_NONBLOCKING)) ::set_nonblocking();
#endif /* 0 */

    if (!___close_callback) return 0;

    if (!errno()) {
      // There's data to read...
      //
      // FIXME: This doesn't work well since the close callback might
      // very well be called sometime later, due to an error if
      // nothing else. What we really need is a special error callback
      // from the backend. /mast
      BE_WERR ("  WARNING: data to read - __stdio_close_callback deregistered");
      ::set_read_callback(0);
      //___close_callback = 0;
    }
    else
    {
#ifdef BACKEND_DEBUG
      if (errno())
	BE_WERR ("  got error " + strerror (errno()) + " from backend");
      else
	BE_WERR ("  got eof");
#endif
      ::set_read_callback(0);
      BE_WERR ("  calling close callback");
      return ___close_callback(___id);
    }

    return 0;
  }

  protected int __stdio_write_callback()
  {
    BE_WERR("__stdio_write_callback()");

    if (!errno()) {
      if (!___write_callback) return 0;

      BE_WERR ("  calling write callback");
      return ___write_callback(___id);
    }

    BE_WERR ("  got error " + strerror (errno()) + " from backend");
    // Don't need to report the error to ___close_callback here - we
    // know it isn't installed. If it were, either
    // __stdio_read_callback or __stdio_close_callback would be
    // installed and would get the error first.
    return 0;
  }

  protected int __stdio_read_oob_callback()
  {
    BE_WERR ("__stdio_read_oob_callback()");

    string s;
    if (!___read_oob_callback) {
      // The out of band callback was probably removed after the backend
      // was started. Propagate the event to __stdio_read_callback().
      s = "";
    } else {
#ifdef STDIO_CALLBACK_TEST_MODE
      s = ::read_oob (1, 1);
#else
      s = ::read_oob(8192,1);
#endif
    }

    if(s)
    {
      if (sizeof(s)) {
	BE_WERR (sprintf ("  calling read oob callback with %O", s));
	return ___read_oob_callback(___id, s);
      }

      // If the backend doesn't support separate read oob events then
      // we'll get here if there's normal data to read or a read eof,
      // and due to the way file_read_oob in file.c currently clears
      // both read events, it won't call __stdio_read_callback or
      // __stdio_close_callback afterwards. Therefore we need to try a
      // normal read here.
      BE_WERR ("  no oob data - trying __stdio_read_callback");
      return __stdio_read_callback();
    }

    else {
      BE_WERR ("  got error " + strerror (errno()) + " from read_oob()");

#if constant(System.EWOULDBLOCK)
      if (errno() == System.EWOULDBLOCK) {
	// Necessary to reregister since the callback is disabled
	// until a successful read() has been done.
	::set_read_oob_callback(__stdio_read_oob_callback);
	return 0;
      }
#endif

      // In case the read fails (it shouldn't, but anyway..).
      ::set_read_oob_callback(0);
      if (___close_callback) {
	BE_WERR ("  calling close callback");
	return ___close_callback(___id);
      }
    }

    return 0;
  }

  protected int __stdio_write_oob_callback()
  {
    BE_WERR ("__stdio_write_oob_callback()");
    if (!___write_oob_callback) return 0;

    BE_WERR ("  calling write oob callback");
    return ___write_oob_callback(___id);
  }

  //! @decl void set_read_callback(function(mixed, string:int) read_cb)
  //! @decl void set_write_callback(function(mixed:int) write_cb)
  //! @decl void set_read_oob_callback(function(mixed, string:int) read_oob_cb)
  //! @decl void set_write_oob_callback(function(mixed:int) write_oob_cb)
  //! @decl void set_close_callback(function(mixed:int) close_cb)
  //!
  //! These functions set the various callbacks, which will be called
  //! when various events occur on the stream. A zero as argument will
  //! remove the callback.
  //!
  //! A @[Pike.Backend] object is responsible for calling the
  //! callbacks. It requires a thread to be waiting in it to execute
  //! the calls. That means that only one of the callbacks will be
  //! running at a time, so you don't need mutexes between them.
  //!
  //! Unless you've specified otherwise with the @[set_backend]
  //! function, the default backend @[Pike.DefaultBackend] will be
  //! used. It's normally activated by returning @expr{-1@} from the
  //! @tt{main@} function and will then execute in the main thread.
  //!
  //! @ul
  //! @item
  //!   When data arrives on the stream, @[read_cb] will be called with
  //!   some or all of that data as the second argument.
  //!
  //! @item
  //!   When the stream has buffer space over for writing, @[write_cb]
  //!   will be called so that you can write more data to it.
  //!
  //!   This callback is also called after the remote end of a socket
  //!   connection has closed the write direction. An attempt to write
  //!   data to it in that case will generate a @[System.EPIPE] errno.
  //!   If the remote end has closed both directions simultaneously
  //!   (the usual case), Pike will first attempt to call @[close_cb],
  //!   then this callback (unless @[close_cb] has closed the stream).
  //!
  //! @item
  //!   When out-of-band data arrives on the stream, @[read_oob_cb]
  //!   will be called with some or all of that data as the second
  //!   argument.
  //!
  //! @item
  //!   When the stream allows out-of-band data to be sent,
  //!   @[write_oob_cb] will be called so that you can write more
  //!   out-of-band data to it.
  //!
  //!   If the OS doesn't separate the write events for normal and
  //!   out-of-band data, Pike will try to call @[write_oob_cb] first.
  //!   If it doesn't write anything, then @[write_cb] will be tried.
  //!   This also means that @[write_oob_cb] might get called when the
  //!   remote end of a connection has closed the write direction.
  //!
  //! @item
  //!   When an error or an end-of-stream in the read direction
  //!   occurs, @[close_cb] will be called. @[errno] will return the
  //!   error, or zero in the case of an end-of-stream.
  //!
  //!   The name of this callback is rather unfortunate since it
  //!   really has nothing to do with a close: The stream is still
  //!   open when @[close_cb] is called (you might not be able to read
  //!   and/or write to it, but you can still use things like
  //!   @[query_address], and the underlying file descriptor is still
  //!   allocated). Also, this callback will not be called for a local
  //!   close, neither by a call to @[close] or by destructing this
  //!   object.
  //!
  //!   Also, @[close_cb] will not be called if a remote close only
  //!   occurs in the write direction; that is handled by @[write_cb]
  //!   (or possibly @[write_oob_cb]).
  //!
  //!   Events to @[read_cb] and @[close_cb] will be automatically
  //!   deregistered if an end-of-stream occurs, and all events in the
  //!   case of an error. I.e. there won't be any more calls to the
  //!   callbacks unless they are reinstalled. This doesn't affect the
  //!   callback settings - @[query_read_callback] et al will still
  //!   return the installed callbacks.
  //! @endul
  //!
  //! If the stream is a socket performing a nonblocking connect (see
  //! @[open_socket] and @[connect]), a connection failure will call
  //! @[close_cb], and a successful connect will call either
  //! @[read_cb] or @[write_cb] as above.
  //!
  //! All callbacks will receive the @tt{id@} set by @[set_id] as
  //! first argument.
  //!
  //! If a callback returns @expr{-1@}, no other callback or call out
  //! will be called by the backend in that round. I.e. the caller of
  //! the backend will get control back right away. For the default
  //! backend that means it will immediately start another round and
  //! check files and call outs anew.
  //!
  //! @note
  //!   These functions do not set the file nonblocking.
  //!
  //! @note
  //!   Callbacks are also set by @[set_callbacks] and
  //!   @[set_nonblocking()].
  //!
  //! @note
  //! After a callback has been called, it's disabled until it has
  //! accessed the stream accordingly, i.e. the @[write_cb] callback
  //! is disabled after it's been called until something has been
  //! written with @[write], and the @[write_oob_cb] callback is
  //! likewise disabled until something has been written with
  //! @[write_oob]. Since the data already has been read when the read
  //! callbacks are called, this effect is not noticeable for them.
  //!
  //! @note
  //! Installing callbacks means that you will start doing I/O on the
  //! stream from the thread running the backend. If you are running
  //! these set functions from another thread you must be prepared
  //! that the callbacks can be called immediately by the backend
  //! thread, so it might not be safe to continue using the stream in
  //! this thread.
  //! 
  //! Because of that, it's useful to talk about "callback mode" when
  //! any callback is installed. In callback mode the stream should be
  //! seen as "bound" to the backend thread. For instance, it's only
  //! the backend thread that reliably can end callback mode before
  //! the stream is "handed over" to another thread.
  //!
  //! @note
  //! Callback mode has nothing to do with nonblocking mode - although
  //! the two often are used together they don't have to be.
  //!
  //! @note
  //! The file object will stay referenced from the backend object as
  //! long as there are callbacks that can receive events.
  //!
  //! @bugs
  //! Setting a close callback without a read callback currently only
  //! works when there's no risk of getting more data on the stream.
  //! Otherwise the close callback will be silently deregistered if
  //! data arrives.
  //!
  //! @seealso
  //! @[set_callbacks], @[set_nonblocking()], @[set_id()],
  //! @[set_backend], @[query_read_callback], @[query_write_callback],
  //! @[query_read_oob_callback], @[query_write_oob_callback],
  //! @[query_close_callback]

#define SET(X,Y) ::set_##X ((___##X = (Y)) && __stdio_##X)
#define _SET(X,Y) _fd->_##X=(___##X = (Y)) && __stdio_##X

  void set_callbacks (void|function(mixed, string:int) read_cb,
		      void|function(mixed:int) write_cb,
		      void|function(mixed:int) close_cb,
		      void|function(mixed, string:int) read_oob_cb,
		      void|function(mixed:int) write_oob_cb)
  //! Installs all the specified callbacks at once. Use @[UNDEFINED]
  //! to keep the current setting for a callback.
  //!
  //! Like @[set_nonblocking], the callbacks are installed atomically.
  //! As opposed to @[set_nonblocking], this function does not do
  //! anything with the stream, and it doesn't even have to be open.
  //!
  //! @seealso
  //! @[set_read_callback], @[set_write_callback],
  //! @[set_read_oob_callback], @[set_write_oob_callback],
  //! @[set_close_callback], @[query_callbacks]
  {
    ::_disable_callbacks();

    // Bypass the ::set_xxx_callback functions; we instead enable all
    // the event bits at once through the _enable_callbacks call at the end.

    if (!zero_type (read_cb))
      _SET (read_callback, read_cb);
    if (!zero_type (write_cb))
      _SET (write_callback, write_cb);

    if (!zero_type (close_cb) &&
	(___close_callback = close_cb) && !___read_callback)
      _fd->_read_callback = __stdio_close_callback;

    if (!zero_type (read_oob_cb))
      _SET (read_oob_callback, read_oob_cb);
    if (!zero_type (write_oob_cb))
      _SET (write_oob_callback, write_oob_cb);

    ::_enable_callbacks();
  }

  //! @decl function(mixed, string:int) query_read_callback()
  //! @decl function(mixed:int) query_write_callback()
  //! @decl function(mixed, string:int) query_read_oob_callback()
  //! @decl function(mixed:int) query_write_oob_callback()
  //! @decl function(mixed:int) query_close_callback()
  //! @decl array(function(mixed,void|string:int)) query_callbacks()
  //!
  //! These functions return the currently installed callbacks for the
  //! respective events.
  //!
  //! @[query_callbacks] returns the callbacks in the same order as
  //! @[set_callbacks] and @[set_nonblocking] expect them.
  //!
  //! @seealso
  //! @[set_nonblocking()], @[set_read_callback],
  //! @[set_write_callback], @[set_read_oob_callback],
  //! @[set_write_oob_callback], @[set_close_callback],
  //! @[set_callbacks]

  //! @ignore

  void set_read_callback(function(mixed|void,string|void:int) read_cb)
  {
    BE_WERR(sprintf("setting read_callback to %O\n", read_cb));
    ::set_read_callback(((___read_callback = read_cb) &&
			 __stdio_read_callback) ||
			(___close_callback && __stdio_close_callback));
  }

  function(mixed|void,string|void:int) query_read_callback()
  {
    return ___read_callback;
  }

#define CBFUNC(TYPE, X)					\
  void set_##X (TYPE l##X)				\
  {							\
    BE_WERR(sprintf("setting " #X " to %O\n", l##X));	\
    SET( X , l##X );					\
  }							\
							\
  TYPE query_##X ()					\
  {							\
    return ___##X;					\
  }

  CBFUNC(function(mixed|void:int), write_callback)
  CBFUNC(function(mixed|void,string|void:int), read_oob_callback)
  CBFUNC(function(mixed|void:int), write_oob_callback)

  void set_close_callback(function(mixed|void:int) c)  {
    ___close_callback=c;
    if (!___read_callback) {
      if (c) {
	::set_read_callback(__stdio_close_callback);
      } else {
	::set_read_callback(0);
      }
    }
  }

  function(mixed|void:int) query_close_callback() { return ___close_callback; }

  array(function(mixed,void|string:int)) query_callbacks()
  {
    return ({
      ___read_callback,
      ___write_callback,
      ___close_callback,
      ___read_oob_callback,
      ___write_oob_callback,
    });
  }

  protected void fix_internal_callbacks()
  {
    BE_WERR("fix_internal_callbacks()\n");
    ::set_read_callback ((___read_callback && __stdio_read_callback) ||
			 (___close_callback && __stdio_close_callback));
    ::set_write_callback (___write_callback && __stdio_write_callback);
    ::set_read_oob_callback (___read_oob_callback && __stdio_read_oob_callback);
    ::set_write_oob_callback (___write_oob_callback && __stdio_write_oob_callback);
  }

  //! @endignore

  //! This function sets the @tt{id@} of this file. The @tt{id@} is mainly
  //! used as an identifier that is sent as the first argument to all
  //! callbacks. The default @tt{id@} is @expr{0@} (zero). Another possible
  //! use of the @tt{id@} is to hold all data related to this file in a
  //! mapping or array.
  //!
  //! @seealso
  //! @[query_id()]
  //!
  void set_id(mixed id) { ___id=id; }

  //! This function returns the @tt{id@} that has been set with @[set_id()].
  //!
  //! @seealso
  //! @[set_id()]
  //!
  mixed query_id() { return ___id; }

  //! @decl void set_nonblocking(function(mixed, string:int) read_callback, @
  //!                            function(mixed:int) write_callback, @
  //!                            function(mixed:int) close_callback)
  //! @decl void set_nonblocking(function(mixed, string:int) read_callback, @
  //!                            function(mixed:int) write_callback, @
  //!                            function(mixed:int) close_callback, @
  //!                            function(mixed, string:int) read_oob_callback, @
  //!                            function(mixed:int) write_oob_callback)
  //! @decl void set_nonblocking()
  //!
  //! This function sets a stream to nonblocking mode and installs the
  //! specified callbacks. See the @expr{set_*_callback@} functions
  //! for details about them. If no arguments are given, the callbacks
  //! will be cleared.
  //!
  //! @note
  //! As opposed to calling the set callback functions separately,
  //! this function will set all the callbacks and nonblocking mode
  //! atomically so that no callback gets called in between. That
  //! avoids races in case the backend is executed by another thread.
  //!
  //! @note
  //!   Out-of-band data was not be supported on Pike 0.5 and earlier,
  //!   and not on Pike 0.6 through 7.4 if they were compiled with the
  //!   option @tt{'--without-oob'@}.
  //!
  //! @seealso
  //! @[set_blocking()], @[set_callbacks], @[set_read_callback()],
  //! @[set_write_callback()], @[set_read_oob_callback()],
  //! @[set_write_oob_callback()], @[set_close_callback()]
  //! @[set_nonblocking_keep_callbacks()],
  //! @[set_blocking_keep_callbacks()]
  //!
  void set_nonblocking(mixed|void rcb,
		       mixed|void wcb,
		       mixed|void ccb,
		       mixed|void roobcb,
		       mixed|void woobcb)
  {
    CHECK_OPEN();
    ::_disable_callbacks(); // Thread safing

    _SET(read_callback,rcb);
    _SET(write_callback,wcb);
    if ((___close_callback = ccb) && (!rcb)) {
      ::set_read_callback(__stdio_close_callback);
    }

    _SET(read_oob_callback,roobcb);
    _SET(write_oob_callback,woobcb);
#ifdef __STDIO_DEBUG
    if(mixed x=catch { ::set_nonblocking(); })
    {
      x[0]+=(__closed_backtrace ? 
	   sprintf("File was closed from:\n    %-=200s\n",__closed_backtrace) :
	   "This file has never been open.\n" );
      throw(x);
    }
#else
    ::set_nonblocking();
#endif
    ::_enable_callbacks();
  }

  //! This function clears all callbacks and sets a stream to blocking
  //! mode. i.e. reading, writing and closing will wait until data has
  //! been transferred before returning.
  //!
  //! @note
  //! The callbacks are cleared and blocking mode is set in one atomic
  //! operation, so no callback gets called in between if the backend
  //! is running in another thread.
  //!
  //! Even so, if the stream is in callback mode (i.e. if any
  //! callbacks are installed) then only the backend thread can use
  //! this function reliably; it might otherwise already be running in
  //! a callback which is about to call e.g. @[write] when the stream
  //! becomes blocking.
  //!
  //! @seealso
  //! @[set_nonblocking()], @[set_nonblocking_keep_callbacks()],
  //! @[set_blocking_keep_callbacks()]
  //!
  void set_blocking()
  {
    CHECK_OPEN();
    ::_disable_callbacks(); // Thread safing
    SET(read_callback,0);
    SET(write_callback,0);
    ___close_callback=0;
    SET(read_oob_callback,0);
    SET(write_oob_callback,0);
    ::set_blocking();
    // NOTE: _enable_callbacks() can throw in only one case;
    //       when callback operations aren't supported, which
    //       we don't care about in this case, since we've
    //       just cleared all the callbacks anyway.
    catch { ::_enable_callbacks(); };
  }

  //! @decl void set_nonblocking_keep_callbacks()
  //! @decl void set_blocking_keep_callbacks()
  //!    Toggle between blocking and nonblocking,
  //!    without changing the callbacks.
  //!
  //! @seealso
  //!   @[set_nonblocking()], @[set_blocking()]

  void set_blocking_keep_callbacks()
  {
     CHECK_OPEN();
#if 0
     ::_disable_callbacks(); // Thread safing // Unnecessary. /mast
#endif
     ::set_blocking();
#if 0
     ::_enable_callbacks();
#endif
  }

  void set_nonblocking_keep_callbacks()
  {
     CHECK_OPEN();
#if 0
     ::_disable_callbacks(); // Thread safing // Unnecessary. /mast
#endif
     ::set_nonblocking();
#if 0
     ::_enable_callbacks();
#endif
  }
   
  protected void destroy()
  {
    BE_WERR("destroy()");
    // Avoid cyclic refs.
    // Not a good idea; the fd may have been
    // given to another object (assign() or dup()).
    //	/grubba 2004-04-07
    // FREE_CB(read_callback);
    // FREE_CB(write_callback);
    // FREE_CB(read_oob_callback);
    // FREE_CB(write_oob_callback);

    register_close_file (open_file_id);
  }
}

//! Handles listening to socket ports. Whenever you need a bound
//! socket that is open and listens for connections you should
//! use this program.
class Port
{
  inherit _port;

  protected int|string debug_port;
  protected string debug_ip;

  protected string _sprintf( int f )
  {
    return f=='O' && sprintf( "%O(%s:%O)",
			      this_program, debug_ip||"", debug_port );
  }

#ifdef STDIO_DIRECT_FD
  //! Factory creating empty @[Fd] objects.
  //!
  //! This function is called by @[accept()] when it needs to create
  //! a new file.
  protected Fd fd_factory()
  {
    return File()->_fd;
  }
#endif

  //! @decl void create()
  //! @decl void create(int|string port)
  //! @decl void create(int|string port, function accept_callback)
  //! @decl void create(int|string port, function accept_callback, string ip)
  //! @decl void create("stdin")
  //! @decl void create("stdin", function accept_callback)
  //!
  //! If the first argument is other than @expr{"stdin"@} the arguments will
  //! be passed to @[bind()].
  //!
  //! When create is called with @expr{"stdin"@} as the first argument, a
  //! socket is created out of the file descriptor @expr{0@}. This is only
  //! useful if it actually is a socket to begin with.
  //!
  //! @seealso
  //! @[bind]
  protected void create( string|int|void p,
		      void|mixed cb,
		      string|void ip )
  {
    debug_ip = (ip||"ANY");
    debug_port = p;

    if( cb || ip )
      if( ip )
        ::create( p, cb, ip );
      else
        ::create( p, cb );
    else
      ::create( p );
  }

  int bind(int|string port, void|function accept_callback, void|string ip) {
    // Needed to fix _sprintf().
    debug_ip = (ip||"ANY");
    debug_port = port;
    return ::bind(port, accept_callback, ip);
  }

  //! This function completes a connection made from a remote machine to
  //! this port. It returns a two-way stream in the form of a clone of
  //! @[Stdio.File]. The new file is by initially set to blocking mode.
  //!
  //! @seealso
  //! @[Stdio.File]
  //!
  File accept()
  {
    if(object(Fd) x=::accept())
    {
#ifndef STDIO_DIRECT_FD
      File y=File();
      y->_fd=x;
#else
      File y = function_object(x->read);
#endif
      y->_setup_debug( "socket", x->query_address() );
      return y;
    }
    return 0;
  }
}

//! @[Stdio.FILE] is a buffered version of @[Stdio.File], it inherits
//! @[Stdio.File] and has most of the functionality of @[Stdio.File].
//! However, it has an input buffer that allows line-by-line input.
//!
//! It also has support for automatic charset conversion for both input
//! and output (see @[Stdio.FILE()->set_charset()]).
//!
//! @note
//!   The output part of @[Stdio.FILE] is currently not buffered.
class FILE
{
#define BUFSIZE 8192
  inherit File : file;

#ifdef STDIO_DIRECT_FD
  // This is needed since it was overloaded in File above.
  protected Fd fd_factory()
  {
    return FILE()->_fd;
  }
#endif

  /* Private functions / buffers etc. */

  private string b="";
  private int bpos=0, lp;

  // Contains a prefix of b splitted on "\n".
  // Note that the last element of the array is a partial line,
  // and should not be used.
  private array(string) cached_lines = ({});

  private function(string:string) output_conversion, input_conversion;
  
  protected string _sprintf( int type, mapping flags )
  {
    return ::_sprintf( type, flags );
  }

  inline private protected final int low_get_data()
  {
    string s = file::read(BUFSIZE,1);
    if(s && strlen(s)) {
      if( input_conversion ) {
	s = input_conversion( s );
      }
      b+=s;
      return 1;
    } else {
      return 0;
    }
  }
 
  inline private protected final int get_data()
  {
    if( bpos )
    {
      b = b[ bpos .. ];
      bpos=0;
    }
    return low_get_data();
  }

  // Update cached_lines and lp
  // Return 0 at end of file, 1 otherwise.
  // At exit cached_lines contains at least one string,
  // and lp is set to zero.
  inline private protected final int get_lines()
  {
    if( bpos )
    {
      b = b[ bpos .. ];
      bpos=0;
    }
    int start = 0;
    while ((search(b, "\n", start) == -1) &&
	   ((start = sizeof(b)), low_get_data()))
      ;

    cached_lines = b/"\n";
    lp = 0;
    return sizeof(cached_lines) > 1;
  }

  // NB: Caller is responsible for clearing cached_lines and lp.
  inline private protected final string extract(int bytes, int|void skip)
  {
    string s;
    s=b[bpos..bpos+bytes-1];
    if ((bpos += bytes+skip) > sizeof(b)) {
      bpos = 0;
      b = "";
    }
    return s;
  }

  /* Public functions. */

  void set_charset( string charset )
  //! Sets the input and output charset of this file to the specified
  //! @[charset].
  //!
  //! The default charset is @tt{"ISO-8859-1"@}.
  //!
  //! @fixme
  //!   Consider using one of
  //!   ISO-IR-196 (@tt{"\e%G"@} - switch to UTF-8 with return)
  //!   or ISO-IR-190 (@tt{"\e%/G"@} - switch to UTF-8 level 1 no return)
  //!   or ISO-IR-191 (@tt{"\e%/H"@} - switch to UTF-8 level 2 no return)
  //!   or ISO-IR-192 (@tt{"\e%/I"@} - switch to UTF-8 level 3 no return)
  //!   or ISO-IR-193 (@tt{"\e%/J"@} - switch to UTF-16 level 1 no return)
  //!   or ISO-IR-194 (@tt{"\e%/K"@} - switch to UTF-16 level 2 no return)
  //!   or ISO-IR-195 (@tt{"\e%/L"@} - switch to UTF-16 level 3 no return)
  //!   or ISO-IR-162 (@tt{"\e%/@@"@} - switch to UCS-2 level 1)
  //!   or ISO-IR-163 (@tt{"\e%/A"@} - switch to UCS-4 level 1)
  //!   or ISO-IR-174 (@tt{"\e%/C"@} - switch to UCS-2 level 2)
  //!   or ISO-IR-175 (@tt{"\e%/D"@} - switch to UCS-4 level 2)
  //!   or ISO-IR-176 (@tt{"\e%/E"@} - switch to UCS-2 level 3)
  //!   or ISO-IR-177 (@tt{"\e%/F"@} - switch to UCS-4 level 3)
  //!   or ISO-IR-178 (@tt{"\e%B"@} - switch to UTF-1)
  //!   automatically to encode wide strings.
  {
    charset = lower_case( charset );
    if( charset != "iso-8859-1" &&
	charset != "ascii")
    {
      object in =  master()->resolv("Locale.Charset.decoder")( charset );
      object out = master()->resolv("Locale.Charset.encoder")( charset );

      input_conversion =
	[function(string:string)]lambda( string s ) {
	  return in->feed( s )->drain();
	};
      output_conversion =
	[function(string:string)]lambda( string s ) {
	  return out->feed( s )->drain();
	};
    }
    else
      input_conversion = output_conversion = 0;
  }

  //! Read one line of input with support for input conversion.
  //!
  //! @param not_all
  //!   Set this parameter to ignore partial lines at EOF. This
  //!   is useful for eg monitoring a growing logfile.
  //!
  //! @returns
  //! This function returns the line read if successful, and @expr{0@} if
  //! no more lines are available.
  //!
  //! @seealso
  //!   @[ngets()], @[read()], @[line_iterator()], @[set_charset()]
  //!
  string gets(int(0..1)|void not_all)
  {
    string r;
    if( (sizeof(cached_lines) <= lp+1) &&
	!get_lines()) {
      // EOF

      // NB: lp is always zero here.
      if (sizeof(r = cached_lines[0]) && !not_all) {
	cached_lines = ({});
	b = "";
	bpos = 0;
	return r;
      }
      return 0;
    }
    bpos += sizeof(r = cached_lines[lp++]) + 1;
    return r;
  }

  int seek(int pos)
  {
    bpos=0;  b=""; cached_lines = ({}); lp=0;
    return file::seek(pos);
  }

  int(-1..1) peek(void|int|float timeout)
  {
    if(sizeof(b)-bpos) return 1;
    return file::peek(timeout);
  }

  int tell()
  {
    return file::tell()-sizeof(b)+bpos;
  }

  int close(void|string mode)
  {
    bpos=0; b="";
    if(!mode) mode="rw";
    file::close(mode);
  }

  int open(string file, void|string mode)
  {
    bpos=0; b="";
    if(!mode) mode="rwc";
    return file::open(file,mode);
  }

  int open_socket(int|string|void port, string|void address, int|string|void family_hint)
  {
    bpos=0;  b="";
    if(zero_type(port))
      return file::open_socket();
    return file::open_socket(port, address, family_hint);
  }

  //! Get @[n] lines.
  //!
  //! @param n
  //!   Number of lines to get, or all remaining if zero.
  //!
  //! @param not_all
  //!   Set this parameter to ignore partial lines at EOF. This
  //!   is useful for eg monitoring a growing logfile.
  array(string) ngets(void|int(1..) n, int(0..1)|void not_all)
  {
    array(string) res;
    if (!n) 
    {
       res=read()/"\n";
       if (res[-1]=="" || not_all) return res[..<1];
       return res;
    }
    if (n < 0) return ({});
    res = ({});
    do {
      array(string) delta;
      if (lp + n < sizeof(cached_lines)) {
	delta = cached_lines[lp..(lp += n)-1];
	bpos += `+(@sizeof(delta[*]), sizeof(delta));
	return res + delta;
      }
      delta = cached_lines[lp..<1];
      bpos += `+(@sizeof(delta[*]), sizeof(delta));
      res += delta;
      // NB: lp and cached_lines are reset by get_lines().
    } while(get_lines());

    // EOF, and we want more lines...

    // NB: At this point lp is always zero, and
    //     cached_lines contains a single string.
    if (sizeof(cached_lines[0]) && !not_all) {
      // Return the partial line too.
      res += cached_lines;
      b = "";
      bpos = 0;
      cached_lines = ({});
    }
    if (!sizeof(res)) return 0;
    return res;
  }

  //! @decl File pipe(int|void flags)
  //!
  //! Same as @[Stdio.File()->pipe()].
  //!
  //! @note
  //!   Returns an @[Stdio.File] object, NOT an @[Stdio.FILE] object.
  //!
  //!   In future releases of Pike this will most likely change
  //!   to returning an @[Stdio.FILE] object. This is already
  //!   the case if @expr{STDIO_DIRECT_FD@} has been defined.

  //! @ignore
#ifndef STDIO_DIRECT_FD
  File
#else
  FILE
#endif
  pipe(void|int flags)
  {
    bpos=0; cached_lines=({}); lp=0;
    b="";
    return query_num_arg() ? file::pipe(flags) : file::pipe();
  }

  //! @endignore

#if constant(files.__HAVE_OPENAT__)
  //! @decl FILE openat(string filename, string mode)
  //! @decl FILE openat(string filename, string mode, int mask)
  //!
  //! Same as @[Stdio.File()->openat()], but returns a @[Stdio.FILE]
  //! object.
  //!
  //! @seealso
  //!   @[Stdio.File()->openat()]
  FILE openat(string filename, string mode, int|void mask)
  {
    if(query_num_arg()<3)
      mask = 0777;
    if(Fd fd=[object(Fd)]_fd->openat(filename, mode, mask))
    {
#ifndef STDIO_DIRECT_FD
      FILE o=FILE();
      o->_fd=fd;
#else
      FILE o = function_object(fd->read);
#endif
      string path = combine_path(debug_file||"", filename);
      o->_setup_debug(path, mode, mask);
      register_open_file(path, o->open_file_id, backtrace());
      return o;
    }else{
      return 0;
    }
  }
#endif
  
  int assign(File|FILE foo)
  {
    bpos=0; cached_lines=({}); lp=0;
    b="";
    return ::assign(foo);
  }

  FILE dup()
  {
    FILE o=FILE();
    o->assign(this);
    return o;
  }

  void set_nonblocking()
  {
    error("Cannot use nonblocking IO with buffered files.\n");
  }

  //! Write @[what] with support for output_conversion.
  //!
  //! @seealso
  //!   @[Stdio.File()->write()]
  int write( array(string)|string what, mixed ... fmt  )
  {
    if( output_conversion )
    {
      if( sizeof( fmt ) )
      {
	if( arrayp( what ) )
	  what *="";
	what = sprintf( [string]what, @fmt );
      }
      if( arrayp( what ) )
	what = map( what, output_conversion );
      else
	what = output_conversion( [string]what );
      return ::write( what );
    }
    return ::write( what,@fmt );
  }

  //! This function does approximately the same as:
  //! @expr{@[write](@[sprintf](@[format],@@@[data]))@}.
  //!
  //! @seealso
  //! @[write()], @[sprintf()]
  //!
  int printf(string format, mixed ... data)
  {
    return ::write(format,@data);
  }

  function(:string) read_function(int nbytes)
  {
    return lambda(){ return read( nbytes); };
  }
    
  //! Returns an iterator that will loop over the lines in this file. 
  //!
  //! @seealso
  //!   @[line_iterator()]
  protected object _get_iterator()
  {
    if( input_conversion )
      return String.SplitIterator( "",'\n',1,read_function(8192));
    // This one is about twice as fast, but it's way less flexible.
    return __builtin.file_line_iterator( read_function(8192) );
  }

  object line_iterator( int|void trim )
  //! Returns an iterator that will loop over the lines in this file. 
  //! If @[trim] is true, all @tt{'\r'@} characters will be removed
  //! from the input.
  //!
  //! @note
  //! It's not supported to call this method more than once
  //! unless a call to @[seek] is done in advance. Also note that it's
  //! not possible to intermingle calls to @[read], @[gets] or other
  //! functions that read data with the line iterator, it will produce
  //! unexpected results since the internal buffer in the iterator will not
  //! contain sequential file-data in those cases.
  //! 
  //! @seealso
  //!   @[_get_iterator()]
  {
    if( trim )
      return String.SplitIterator( "",(<'\n','\r'>),1,read_function(8192));
    return _get_iterator();
  }

  //! Read @[bytes] (wide-) characters with buffering and support for
  //! input conversion.
  //!
  //! @seealso
  //!   @[Stdio.File()->read()], @[set_charset()], @[unread()]
  string read(int|void bytes,void|int(0..1) now)
  {
    if (!query_num_arg()) {
      bytes = 0x7fffffff;
    }

    /* Optimization - Hubbe */
    if(!sizeof(b) && bytes > BUFSIZE) {
      if (input_conversion) {
	// NOTE: This may depending on the charset return less
	//       characters than requested.
	// FIXME: Does this handle EOF correctly?
	return input_conversion(::read(bytes, now));
      }
      return ::read(bytes, now);
    }

    cached_lines = ({}); lp = 0;
    while(sizeof(b) - bpos < bytes) {
      if(!get_data()) {
	// EOF.
	// NB: get_data() sets bpos to zero.
	string res = b;
	b = "";
	return res;
      }
      else if (now) break;
    }

    return extract(bytes);
  }

  //! This function puts a string back in the input buffer. The string
  //! can then be read with eg @[read()], @[gets()] or @[getchar()].
  //!
  //! @seealso
  //! @[read()], @[gets()], @[getchar()], @[ungets()]
  //!
  void unread(string s)
  {
    cached_lines = ({});
    lp = 0;
    b=s+b[bpos..];
    bpos=0;
  }

  //! This function puts a line back in the input buffer. The line
  //! can then be read with eg @[read()], @[gets()] or @[getchar()].
  //!
  //! @note
  //!   The string is autoterminated by an extra line-feed.
  //!
  //! @seealso
  //! @[read()], @[gets()], @[getchar()], @[unread()]
  //!
  void ungets(string s)
  {
    if(sizeof(cached_lines)>lp)
      cached_lines = s/"\n" + cached_lines[lp..];
    else
      cached_lines = ({});
    lp = 0;
    b=s+"\n"+b[bpos..];
    bpos=0;
  }

  //! This function returns one character from the input stream.
  //!
  //! @returns
  //!   Returns the ISO-10646 (Unicode) value of the character.
  //!
  //! @note
  //!   Returns an @expr{int@} and not a @expr{string@} of length 1.
  //!
  int getchar()
  {
    if(sizeof(b) - bpos <= 0 && !get_data())
      return -1;

    if(sizeof(cached_lines)>lp+1 && sizeof(cached_lines[lp]))
      cached_lines = ({cached_lines[lp][1..]}) + cached_lines[lp+1..];
    else
      cached_lines = ({});
    lp=0;

    return b[bpos++];
  }
}

//! An instance of @tt{FILE("stderr")@}, the standard error stream. Use this
//! when you want to output error messages.
//!
//! @seealso
//!   @[predef::werror()]
FILE stderr=FILE("stderr");

//! An instance of @tt{FILE("stdout")@}, the standatd output stream. Use this
//! when you want to write anything to the standard output.
//!
//! @seealso
//!   @[predef::write()]
FILE stdout=FILE("stdout");

//! An instance of @tt{FILE("stdin")@}, the standard input stream. Use this
//! when you want to read anything from the standard input.
//! This example will read lines from standard input for as long as there
//! are more lines to read. Each line will then be written to stdout together
//! with the line number. We could use @[Stdio.stdout.write()] instead
//! of just @[write()], since they are the same function.
//!
//! @example
//!  int main()
//!  {
//!    int line;
//!    while(string s=Stdio.stdin.gets())
//! 	 write("%5d: %s\n", line++, s);
//!  }
FILE stdin=FILE("stdin");

#ifdef TRACK_OPEN_FILES
protected mapping(string|int:array) open_files = ([]);
protected mapping(string:int) registering_files = ([]);
protected int next_open_file_id = 1;

protected void register_open_file (string file, int id, array backtrace)
{
  file = combine_path (getcwd(), file);
  if (!registering_files[file]) {
    // Avoid the recursion which might occur when the backtrace is formatted.
    registering_files[file] = 1;
    open_files[id] =
      ({file, describe_backtrace (backtrace[..<1])});
    m_delete (registering_files, file);
  }
  else
    open_files[id] =
      ({file, "Cannot describe backtrace due to recursion.\n"});
  if (!open_files[file]) open_files[file] = ({id});
  else open_files[file] += ({id});
}

protected void register_close_file (int id)
{
  if (open_files[id]) {
    string file = open_files[id][0];
    open_files[file] -= ({id});
    if (!sizeof (open_files[file])) m_delete (open_files, file);
    m_delete (open_files, id);
  }
}

array(string) file_open_places (string file)
{
  file = combine_path (getcwd(), file);
  if (array(int) ids = open_files[file])
    return map (ids, lambda (int id) {
		       if (array ent = open_files[id])
			 return ent[1];
		     }) - ({0});
  return 0;
}

void report_file_open_places (string file)
{
  array(string) places = file_open_places (file);
  if (places)
    werror ("File " + file + " is currently opened from:\n" +
	    map (places,
		 lambda (string place) {
		   return " * " +
		     replace (place[..<1], "\n", "\n   ");
		 }) * "\n\n" + "\n");
  else
    werror ("File " + file + " is currently not opened from any known place.\n");
}
#endif

//! @decl string read_file(string filename)
//! @decl string read_file(string filename, int start, int len)
//!
//! Read @[len] lines from a regular file @[filename] after skipping
//! @[start] lines and return those lines as a string. If both
//! @[start] and @[len] are omitted the whole file is read.
//!
//! @throws
//!   Throws an error on any I/O error except when the file doesn't
//!   exist.
//!
//! @returns
//!   Returns @expr{0@} (zero) if the file doesn't exist or if
//!   @[start] is beyond the end of it.
//!
//!   Returns a string with the requested data otherwise.
//!
//! @seealso
//! @[read_bytes()], @[write_file()]
//!
string read_file(string filename,void|int start,void|int len)
{
  FILE f;
  string ret;
  f=FILE();
  if (!f->open(filename,"r")) {
    if (f->errno() == System.ENOENT)
      return 0;
    else
      error ("Failed to open %O: %s\n", filename, strerror (f->errno()));
  }

  // Disallow devices and directories.
  Stat st;
  if ((st = f->stat()) && !st->isreg) {
    error( "%O is not a regular file.\n", filename );
  }

  switch(query_num_arg())
  {
  case 1:
    ret=f->read();
    if (!ret)
      error ("Failed to read %O: %s\n", filename, strerror (f->errno()));
    break;

  case 2:
    len=0x7fffffff;
  case 3:
    while(start--) {
      if (!f->gets())
	if (int err = f->errno())
	  error ("Failed to read %O: %s\n", filename, strerror (err));
	else
	  return "";		// EOF reached.
    }
    String.Buffer buf=String.Buffer();
    while(len--)
    {
      if (string tmp = f->gets())
	buf->add(tmp, "\n");
      else
	if (int err = f->errno())
	  error ("Failed to read %O: %s\n", filename, strerror (err));
	else
	  break;		// EOF reached.
    }
    ret=buf->get();
    destruct(buf);
  }
  f->close();

  return ret;
}

//! @decl string read_bytes(string filename, int start, int len)
//! @decl string read_bytes(string filename, int start)
//! @decl string read_bytes(string filename)
//!
//! Read @[len] number of bytes from a regular file @[filename]
//! starting at byte @[start], and return it as a string.
//!
//! If @[len] is omitted, the rest of the file will be returned.
//!
//! If @[start] is also omitted, the entire file will be returned.
//!
//! @throws
//!   Throws an error on any I/O error except when the file doesn't
//!   exist.
//!
//! @returns
//!   Returns @expr{0@} (zero) if the file doesn't exist or if
//!   @[start] is beyond the end of it.
//!
//!   Returns a string with the requested data otherwise.
//!
//! @seealso
//! @[read_file], @[write_file()], @[append_file()]
//!
string read_bytes(string filename, void|int start,void|int len)
{
  string ret;
  File f = File();

  if (!f->open(filename,"r")) {
    if (f->errno() == System.ENOENT)
      return 0;
    else
      error ("Failed to open %O: %s\n", filename, strerror (f->errno()));
  }

  // Disallow devices and directories.
  Stat st;
  if ((st = f->stat()) && !st->isreg) {
    error( "%O is not a regular file.\n", filename );
  }

  switch(query_num_arg())
  {
  case 1:
  case 2:
    len=0x7fffffff;
  case 3:
    if(start)
      if (f->seek(start) < 0)
	error ("Failed to seek in %O: %s\n", filename, strerror(f->errno()));
  }
  ret=f->read(len);
  if (!ret)
    error ("Failed to read %O: %s\n", filename, strerror (f->errno()));
  f->close();
  return ret;
}

//! Write the string @[str] onto the file @[filename]. Any existing
//! data in the file is overwritten.
//!
//! For a description of @[access], see @[Stdio.File()->open()].
//!
//! @throws
//!   Throws an error if @[filename] couldn't be opened for writing.
//!
//! @returns
//!   Returns the number of bytes written, i.e. @expr{sizeof(str)@}.
//!
//! @seealso
//!   @[append_file()], @[read_bytes()], @[Stdio.File()->open()]
//!
int write_file(string filename, string str, int|void access)
{
  int ret;
  File f = File();

  if (zero_type (access)) {
    access = 0666;
  }

  if(!f->open(filename, "twc", access))
    error("Couldn't open %O: %s\n", filename, strerror(f->errno()));

  while (ret < sizeof (str)) {
    int bytes = f->write(str[ret..]);
    if (bytes <= 0) {
      error ("Couldn't write to %O: %s\n", filename, strerror (f->errno()));
    }
    ret += bytes;
  }

  f->close();
  return ret;
}

//! Append the string @[str] onto the file @[filename].
//!
//! For a description of @[access], see @[Stdio.File->open()].
//!
//! @throws
//!   Throws an error if @[filename] couldn't be opened for writing.
//!
//! @returns
//!   Returns the number of bytes written, i.e. @expr{sizeof(str)@}.
//!
//! @seealso
//!   @[write_file()], @[read_bytes()], @[Stdio.File()->open()]
//!
int append_file(string filename, string str, int|void access)
{
  int ret;
  File f = File();

  if (zero_type (access)) {
    access = 0666;
  }

  if(!f->open(filename, "awc", access))
    error("Couldn't open %O: %s\n", filename, strerror(f->errno()));

  while (ret < sizeof (str)) {
    int bytes = f->write(str[ret..]);
    if (bytes <= 0) {
      error ("Couldn't write to %O: %s\n", filename, strerror (f->errno()));
    }
    ret += bytes;
  }

  f->close();
  return ret;
}

//! Give the size of a file. Size -1 indicates that the file either
//! does not exist, or that it is not readable by you. Size -2
//! indicates that it is a directory, -3 that it is a symlink and -4
//! that it is a device.
//!
//! @seealso
//! @[file_stat()], @[write_file()], @[read_bytes()]
//!
int file_size(string filename)
{
  Stat stat;
  stat = file_stat(filename);
  if(!stat) return -1;
  // Please note that stat->size is not always the same thing as stat[1]. /mast
  return [int]stat[1];
}

//! @decl string append_path(string absolute, string ... relative)
//! @decl string append_path_unix(string absolute, string ... relative)
//! @decl string append_path_nt(string absolute, string ... relative)
//! 
//!   Append @[relative] paths to an @[absolute] path and remove any
//!   @expr{"//"@}, @expr{"../"@} or @expr{"/."@} to produce a
//!   straightforward absolute path as a result.
//!
//!   @expr{"../"@} is ignorded in the relative paths if it makes the
//!   created path begin with something else than the absolute path
//!   (or so far created path).
//!
//!   @[append_path_nt()] fixes drive letter issues in @[relative]
//!   by removing the colon separator @expr{":"@} if it exists (k:/fnord appends 
//!   as k/fnord)
//!
//!   @[append_path_nt()] also makes sure that UNC path(s) in @[relative] is appended 
//!   correctly by removing any @expr{"\\"@} or @expr{"//"@} from the beginning.
//!
//!   @[append_path()] is equivalent to @[append_path_unix()] on UNIX-like
//!   operating systems, and equivalent to @[append_path_nt()] on NT-like
//!   operating systems.
//!
//!   @seealso
//!   @[combine_path()]
//!

string append_path_unix(string absolute, string ... relative)
{
  return combine_path_unix(absolute,
			   @map(relative, lambda(string s) {
					    return combine_path_unix("/", s)[1..];
					  }));
}

string append_path_nt(string absolute, string ... relative)
{
  return combine_path_nt(absolute,
			 @map(relative, lambda(string s) {
					  if(s[1..1] == ":") {
					    s = s[0..0] + s[2..];
					  }
					  else if(s[0..1] == "\\\\" || s[0..1] == "//") {
					    s = s[2..];
					  }
					  return combine_path_nt("/", s)[1..];
					}));
}

string append_path(string absolute, string ... relative)
{
#ifdef __NT__
  return append_path_nt (absolute, @relative);
#else
  return append_path_unix (absolute, @relative);
#endif
}

//! Returns a canonic representation of @[path] (without /./, /../, //
//! and similar path segments).
string simplify_path(string path)
{
  if(has_prefix(path, "/"))
    return combine_path("/", path);
  return combine_path("/", path)[1..];
}

//! This function prints a message to stderr along with a description
//! of what went wrong if available. It uses the system errno to find
//! out what went wrong, so it is only applicable to IO errors.
//!
//! @seealso
//! @[werror()]
//!
void perror(string s)
{
#if efun(strerror)
  stderr->write(s+": "+strerror(predef::errno())+"\n");
#else
  stderr->write(s+": errno: "+predef::errno()+"\n");
#endif
}

/*
 * Predicates.
 */

//! Check if a @[path] is a file.
//!
//! @returns
//! Returns true if the given path is a file, otherwise false.
//!
//! @seealso
//! @[exist()], @[is_dir()], @[is_link()], @[file_stat()]
//!
int is_file(string path)
{
  if (Stat s = file_stat (path)) return [int]s->isreg;
  return 0;
}

//! Check if a @[path] is a directory.
//!
//! @returns
//! Returns true if the given path is a directory, otherwise false.
//!
//! @seealso
//! @[exist()], @[is_file()], @[is_link()], @[file_stat()]
//!
int is_dir(string path)
{
  if (Stat s = file_stat (path)) return [int]s->isdir;
  return 0;
}

//! Check if a @[path] is a symbolic link.
//!
//! @returns
//! Returns true if the given path is a symbolic link, otherwise false.
//!
//! @seealso
//! @[exist()], @[is_dir()], @[is_file()], @[file_stat()]
//!
int is_link(string path)
{
  if (Stat s = file_stat (path, 1)) return [int]s->islnk;
  return 0;
}

//! Check if a @[path] exists.
//!
//! @returns
//! Returns true if the given path exists (is a directory or file),
//! otherwise false.
//!
//! @note
//!   May fail with eg @[errno()] @tt{EFBIG@} if the file exists,
//!   but the filesystem doesn't support the file size.
//!
//! @seealso
//! @[is_dir()], @[is_file()], @[is_link()], @[file_stat()]
int exist(string path)
{
  return !!file_stat(path);
}

//! Convert the mode_string string as returned by Stdio.Stat object
//! to int suitable for chmod
//!  
//! @param mode_string
//!   The string as return from Stdio.Stat()->mode_string
//! @returns
//!   An int matching the permission of the mode_string string suitable for 
//!   chmod
int convert_modestring2int(string mode_string)
{
  constant user_permissions_letters2value =
    ([ 
      "r": 0400,
      "w": 0200,
      "x": 0100,
      "s": 4000,
      "S": 2000,
      "t": 1000
    ]);
  constant group_permissions_letters2value =
    ([
      "r": 0040,
      "w": 0020,
      "x": 0010,
      "s": 4000,
      "S": 2000,
      "t": 1000
    ]);
   constant other_permissions_letters2value =
    ([
      "r": 0004,
      "w": 0002,
      "x": 0001,
      "s": 4000,
      "S": 2000,
      "t": 1000
    ]);
  int result = 0;
  array arr_mode = mode_string / "";
  if(sizeof(mode_string) != 10)
  {
    throw(({ "Invalid mode_string", backtrace() }));
  }
  for(int i = 1; i < 4; i++)
  {
    result += user_permissions_letters2value[arr_mode[i]];
  }
  for(int i = 4; i < 7; i++)
  {
    result += group_permissions_letters2value[arr_mode[i]];
  }
  for(int i = 7; i < 10; i++)
  {
    result += other_permissions_letters2value[arr_mode[i]];
  }
  return result;
}

#define BLOCK 65536

int cp(string from, string to)
//! Copies the file @[from] to the new position @[to]. If there is
//! no system function for cp, a new file will be created and the
//! old one copied manually in chunks of 65536 bytes.
//! This function can also copy directories recursively.
//! @returns
//!  0 on error, 1 on success
//! @note
//! This function keeps file and directory mode bits, unlike in Pike
//! 7.6 and earlier.
{
  Stat stat = file_stat(from, 1);
  if( !stat ) 
     return 0;

  if(stat->isdir)
  {
    // recursive copying of directories
    if(!mkdir(to))
      return 0;
    array(string) sub_files = get_dir(from);
    foreach(sub_files, string sub_file)
    {
      if(!cp(combine_path(from, sub_file), combine_path(to, sub_file)))
        return 0;
    }
  }
  else
  {
#if constant(System.cp)
    if (!System.cp (from, to))
      return 0;
#else
    File f=File(), t;
    if(!f->open(from,"r")) return 0;
    function(int,int|void:string) r=f->read;
    t=File();
    if(!t->open(to,"wct")) {
      f->close();
      return 0;
    }
    string data;
    function(string:int) w=t->write;
    do
    {
      data=r(BLOCK);
      if(!data) return 0;
      if(w(data)!=sizeof(data)) return 0;
    }while(sizeof(data) == BLOCK);
  
    f->close();
    t->close();
#endif
  }

  chmod(to, convert_modestring2int([string]stat->mode_string));
  return 1;
}

int file_equal (string file_1, string file_2)
//! Returns nonzero if the given paths are files with identical
//! content, returns zero otherwise. Zero is also returned for any
//! sort of I/O error.
{
  File f1 = File(), f2 = File();

  if( file_1 == file_2 )
    return 1;

  if (!f1->open (file_1, "r") || !f2->open (file_2, "r")) return 0;


  // Some optimizations.
  Stat s1 = f1->stat(), s2 = f2->stat();

  if( s1->size != s2->size )
    return 0;

  // Detect sym- or hardlinks to the same file.
  if( (s1->dev == s2->dev) && (s1->ino == s2->ino) )
    return 1;
  
  function(int,int|void:string) f1_read = f1->read, f2_read = f2->read;
  string d1, d2;
  do {
    d1 = f1_read (BLOCK);
    if (!d1) return 0;
    d2 = f2_read (BLOCK);
    if (d1 != d2) return 0;
  } while (sizeof (d1) == BLOCK);
  f1->close();
  f2->close();
  return 1;
}

protected void call_cp_cb(int len,
		       function(int, mixed ...:void) cb, mixed ... args)
{
  // FIXME: Check that the lengths are the same?
  cb(1, @args);
}

//! Copy a file asynchronously.
//!
//! This function is similar to @[cp()], but works asynchronously.
//!
//! @param from
//!   Name of file to copy.
//!
//! @param to
//!   Name of file to create or replace with a copy of @[from].
//!
//! @param callback
//!   Function to be called on completion.
//!   The first argument will be @expr{1@} on success, and @expr{0@} (zero)
//!   otherwise. The rest of the arguments to @[callback] are passed
//!   verbatim from @[args].
//!
//! @param args
//!   Extra arguments to pass to @[callback].
//!
//! @note
//!   For @[callback] to be called, the backend must be active (ie
//!   @[main()] must have returned @expr{-1@}, or @[Pike.DefaultBackend]
//!   get called in some other way). The actual copying may start
//!   before the backend has activated.
//!
//! @bugs
//!   Currently the file sizes are not compared, so the destination file
//!   (@[to]) may be truncated.
//!
//! @seealso
//!   @[cp()], @[sendfile()]
void async_cp(string from, string to,
	      function(int, mixed...:void) callback, mixed ... args)
{
  File from_file = File();
  File to_file = File();

  if ((!(from_file->open(from, "r"))) ||
      (!(to_file->open(to, "wct")))) {
    call_out(callback, 0, 0, @args);
    return;
  }
  sendfile(0, from_file, 0, -1, 0, to_file, call_cp_cb, callback, @args);
}

//! Creates zero or more directories to ensure that the given @[pathname] is
//! a directory.
//!
//! If a @[mode] is given, it's used for the new directories after being &'ed
//! with the current umask (on OS'es that support this).
//!
//! @returns
//!   Returns zero on failure and nonzero on success.
//!
//! @seealso
//! @[mkdir()]
//!
int mkdirhier (string pathname, void|int mode)
{
  if (zero_type (mode)) mode = 0777; // &'ed with umask anyway.
  if (!sizeof(pathname)) return 0;
  string path = "";
#ifdef __NT__
  pathname = replace(pathname, "\\", "/");
  if (pathname[1..2] == ":/" && `<=("A", upper_case(pathname[..0]), "Z"))
    path = pathname[..2], pathname = pathname[3..];
#endif
  while (pathname[..0] == "/") pathname = pathname[1..], path += "/";
  foreach (pathname / "/", string name) {
    path += name;
    if (!file_stat(path)) {
      if (!mkdir(path, mode)) {
	if (errno() != System.EEXIST)
	  return 0;
      }
    }
    path += "/";
  }
  return is_dir (path);
}

//! Remove a file or a directory tree.
//!
//! @returns
//! Returns 0 on failure, nonzero otherwise.
//!
//! @seealso
//! @[rm]
//!
int recursive_rm (string path)
{
  Stat a = file_stat(path, 1);
  if(!a)
    return 0;
  if(a->isdir)
    if (array(string) sub = get_dir (path))
      foreach( sub, string name )
	recursive_rm (combine_path(path, name));
  return rm (path);
}

//! Copy a file or a directory tree by copying and then 
//! removing. Mode bits are preserved in the copy.
//! It's not the fastest but works on every OS and
//! works well across different file systems.
//!
//! @returns
//! Returns 0 on failure, nonzero otherwise.
//!
//! @seealso
//! @[recursive_rm] @[cp]
//!
int recursive_mv(string from, string to)
{
  if(!cp(from, to))
    return 0;
  return recursive_rm(from);
}

/*
 * Asynchronous sending of files.
 */

#define READER_RESTART 4
#define READER_HALT 32

// FIXME: Support for timeouts?
protected class nb_sendfile
{
  protected File from;
  protected int len;
  protected array(string) trailers;
  protected File to;
  protected Pike.Backend backend;
  protected function(int, mixed ...:void) callback;
  protected array(mixed) args;

  // NOTE: Always modified from backend callbacks, so no need
  // for locking.
  //
  // The strings in to_write are always split up into DATA_CHUNK_SIZE
  // pieces to limit the size of the strings passed to to->write().
  // That way repeated operations like str=str[x..] are avoided on
  // arbitrarily large strings.
  protected array(string) to_write = ({});
  protected int sent;

  protected int reader_awake;
  protected int writer_awake;

  protected int blocking_to;
  protected int blocking_from;

  /* Reader */

  protected string _sprintf( int f )
  {
    switch( f )
    {
     case 't':
       return "Stdio.Sendfile";
     case 'O':
       return sprintf( "%t()", this );
    }
  }

  protected void reader_done()
  {
    SF_WERR("Reader done.");

    from->set_blocking();
    from = 0;
    if (trailers) {
      to_write += trailers;
    }
    if (blocking_to) {
      while(sizeof(to_write)) {
	if (!do_write()) {
	  // Connection closed or Disk full.
	  writer_done();
	  return;
	}
      }
      if (!from) {
	writer_done();
	return;
      }
    } else {
      if (sizeof(to_write)) {
	start_writer();
      } else {
	writer_done();
	return;
      }
    }
  }

  protected void close_cb(mixed ignored)
  {
    SF_WERR("Input EOF.");
    reader_done();
  }

  protected void do_read()
  {
    SF_WERR("Blocking read.");
    if( sizeof( to_write ) > 2)
      return;
    string more_data = "";
    if ((len < 0) || (len > DATA_CHUNK_SIZE)) {
      more_data = from->read(DATA_CHUNK_SIZE, 1);
    } else if (len) {
      more_data = from->read(len, 1);
    }
    if (!more_data) {
      SF_WERR(sprintf("Blocking read failed with errno: %d\n", from->errno()));
      more_data = "";
    }
    if (more_data == "") {
      // EOF.
      SF_WERR("Blocking read got EOF.");

      from = 0;
      if (trailers) {
	to_write += (trailers - ({ "" }));
	trailers = 0;
      }
    } else {
      if (len > 0) len -= sizeof(more_data);
      to_write += ({ more_data });
    }
  }

  protected void read_cb(mixed ignored, string data)
  {
    SF_WERR("Read callback.");
    if (len >= 0) {
      if (sizeof(data) < len) {
	len -= sizeof(data);
	to_write += data / (float) DATA_CHUNK_SIZE;
      } else {
	to_write += data[..len-1] / (float) DATA_CHUNK_SIZE;
	len = 0;
	from->set_blocking();
	reader_done();
	return;
      }
    } else {
      to_write += data / (float) DATA_CHUNK_SIZE;
    }
    if (blocking_to) {
      while(sizeof(to_write)) {
	if (!do_write()) {
	  // Connection closed or Disk full.
	  writer_done();
	  return;
	}
      }
      if (!from) {
	writer_done();
	return;
      }
    } else {
      if (sizeof(to_write) > READER_HALT) {
	// Go to sleep.
	from->set_blocking();
	reader_awake = 0;
      }
      start_writer();
    }
  }

  protected void start_reader()
  {
    SF_WERR("Starting the reader.");
    if (!reader_awake) {
      reader_awake = 1;
      from->set_nonblocking(read_cb, from->query_write_callback(), close_cb);
    }
  }

  /* Writer */

  protected void writer_done()
  {
    SF_WERR("Writer done.");

    // Disable any reader.
    if (from && from->set_nonblocking) {
      from->set_nonblocking(0, from->query_write_callback(), 0);
    }

    // Disable any writer.
    if (to && to->set_nonblocking) {
      to->set_nonblocking(to->query_read_callback(), 0,
			  to->query_close_callback());
    }

    // Make sure we get rid of any references...
    to_write = 0;
    trailers = 0;
    from = 0;
    to = 0;
    backend = 0;
    array(mixed) a = args;
    function(int, mixed ...:void) cb = callback;
    args = 0;
    callback = 0;
    if (cb) {
      cb(sent, @a);
    }
  }

  protected int do_write()
  {
    SF_WERR("Blocking writer.");

    int bytes = sizeof(to_write) && to->write(to_write);

    if (bytes >= 0) {
      SF_WERR(sprintf("Wrote %d bytes.", bytes));
      sent += bytes;

      int n;
      for (n = 0; n < sizeof(to_write); n++) {
	if (bytes < sizeof(to_write[n])) {
	  to_write[n] = to_write[n][bytes..];
	  to_write = to_write[n..];

	  return 1;
	} else {
	  bytes -= sizeof(to_write[n]);
	  if (!bytes) {
	    to_write = to_write[n+1..];
	    return 1;
	  }
	}
      }

      return 1;
    } else {
      SF_WERR("Blocking writer got EOF.");
      // Premature end of file!
      return 0;
    }
  }

  protected void write_cb(mixed ignored)
  {
    SF_WERR("Write callback.");
    if (do_write()) {
      if (from) {
	if (sizeof(to_write) < READER_RESTART) {
	  if (blocking_from) {
	    do_read();
	    if (!sizeof(to_write)) {
	      // Done.
	      writer_done();
	    }
	  } else {
	    if (!sizeof(to_write)) {
	      // Go to sleep.
	      to->set_nonblocking(to->query_read_callback(),0,
				  to->query_close_callback());
	      writer_awake = 0;
	    }
	    start_reader();
	  }
	}
      } else if (!sizeof(to_write)) {
	// Done.
	writer_done();
      }
    } else {
      // Premature end of file!
      writer_done();
    }
  }

  protected void start_writer()
  {
    SF_WERR("Starting the writer.");

    if (!writer_awake) {
      writer_awake = 1;
      to->set_nonblocking(to->query_read_callback(), write_cb,
			  to->query_write_callback());
    }
  }

  /* Blocking */
  protected void do_blocking()
  {
    SF_WERR("Blocking I/O.");

    if (from && (sizeof(to_write) < READER_RESTART)) {
      do_read();
    }
    if (sizeof(to_write) && do_write()) {
      backend->call_out(do_blocking, 0);
    } else {
      SF_WERR("Blocking I/O done.");
      // Done.
      from = 0;
      to = 0;
      backend = 0;
      writer_done();
    }
  }

#ifdef SENDFILE_DEBUG
  void destroy()
  {
    werror("Stdio.sendfile(): Destructed.\n");
  }
#endif /* SENDFILE_DEBUG */

  /* Starter */

  protected void create(array(string) hd,
		     File f, int off, int l,
		     array(string) tr,
		     File t,
		     function(int, mixed ...:void)|void cb,
		     mixed ... a)
  {
    if (!l) {
      // No need for from.
      f = 0;

      // No need to differentiate between headers and trailers.
      if (tr) {
	if (hd) {
	  hd += tr;
	} else {
	  hd = tr;
	}
	tr = 0;
      }
    }

    if (!f && (!hd || !sizeof(hd - ({ "" })))) {
      // NOOP!
      SF_WERR("NOOP!");
      backend->call_out(cb, 0, 0, @a);
      return;
    }

    if (hd)
      to_write = map (hd, lambda (string s) {
			    return s / (float) DATA_CHUNK_SIZE;
			  }) * ({}) - ({""});
    else
      to_write = ({});

    from = f;
    len = l;

    if (tr)
      trailers = map (tr, lambda (string s) {
			    return s / (float) DATA_CHUNK_SIZE;
			  }) * ({}) - ({""});
    else
      trailers = ({});

    to = t;
    callback = cb;
    args = a;

    backend = (to->query_backend && to->query_backend()) ||
      Pike.DefaultBackend;

    blocking_to = to->is_file ||
      ((!to->set_nonblocking) ||
       (to->mode && !(to->mode() & PROP_NONBLOCK)));

    if (blocking_to && to->set_blocking) {
      SF_WERR("Blocking to.");
      to->set_blocking();
    }

    if (from) {
      blocking_from = from->is_file ||
	((!from->set_nonblocking) ||
	 (from->mode && !(from->mode() & PROP_NONBLOCK)));
	
      if (off >= 0) {
	from->seek(off);
      }
      if (blocking_from) {
	SF_WERR("Blocking from.");
	if (from->set_blocking) {
	  from->set_blocking();
	}
      } else {
	SF_WERR("Starting reader.");
	start_reader();
      }
    }

    if (blocking_to) {
      if (!from || blocking_from) {
	// Can't use the reader to push data.

	// Could have a direct call to do_blocking here,
	// but then the callback would be called from the wrong context.
	SF_WERR("Using fully blocking I/O.");
	backend->call_out(do_blocking, 0);
      }
    } else {
      if (blocking_from) {
	SF_WERR("Reading some data.");
	do_read();
	if (!sizeof(to_write)) {
	  SF_WERR("NOOP!");
	  backend->call_out(cb, 0, 0, @args);
	}
      }
      if (sizeof(to_write)) {
	SF_WERR("Starting the writer.");
	start_writer();
      }
    }
  }
}

//! @decl object sendfile(array(string) headers, @
//!                       File from, int offset, int len, @
//!                       array(string) trailers, @
//!                       File to)
//! @decl object sendfile(array(string) headers, @
//!                       File from, int offset, int len, @
//!                       array(string) trailers, @
//!                       File to, @
//!                       function(int, mixed ...:void) callback, @
//!                       mixed ... args)
//!
//! Sends @[headers] followed by @[len] bytes starting at @[offset]
//! from the file @[from] followed by @[trailers] to the file @[to].
//! When completed @[callback] will be called with the total number of
//! bytes sent as the first argument, followed by @[args].
//!
//! Any of @[headers], @[from] and @[trailers] may be left out
//! by setting them to @expr{0@}.
//!
//! Setting @[offset] to @expr{-1@} means send from the current position in
//! @[from].
//!
//! Setting @[len] to @expr{-1@} means send until @[from]'s end of file is
//! reached.
//!
//! @note
//! The sending is performed asynchronously, and may complete
//! both before and after the function returns.
//!
//! For @[callback] to be called, the backend must be active (ie
//! @[main()] must have returned @expr{-1@}, or @[Pike.DefaultBackend]
//! get called in some other way).
//!
//! In some cases, the backend must also be active for any sending to
//! be performed at all.
//!
//! In Pike 7.4.496, Pike 7.6.120 and Pike 7.7 and later the backend
//! associated with @[to] will be used rather than the default backend.
//! Note that you usually will want @[from] to have the same backend as @[to].
//!
//! @bugs
//! FIXME: Support for timeouts?
//!
//! @seealso
//! @[Stdio.File->set_nonblocking()]
//!
object sendfile(array(string) headers,
		File from, int offset, int len,
		array(string) trailers,
		File to,
		function(int, mixed ...:void)|void cb,
		mixed ... args)
{
#if !defined(DISABLE_FILES_SENDFILE) && constant(files.sendfile)
  // Try using files.sendfile().
  
  mixed err = catch {
    return files.sendfile(headers, from, offset, len,
			  trailers, to, cb, @args);
  };

#ifdef SENDFILE_DEBUG
  werror("files.sendfile() failed:\n%s\n", describe_backtrace(err));
#endif /* SENDFILE_DEBUG */

#endif /* !DISABLE_FILES_SENDFILE && files.sendfile */

  // Use nb_sendfile instead.
  return nb_sendfile(headers, from, offset, len, trailers, to, cb, @args);
}

//! UDP (User Datagram Protocol) handling.
class UDP
{
  inherit files.UDP;

  private protected array extra=0;
  private protected function(mapping,mixed...:void) callback=0;

  //! @decl UDP set_nonblocking()
  //! @decl UDP set_nonblocking(function(mapping(string:int|string), @
  //!                                    mixed ...:void) read_cb, @
  //!                           mixed ... extra_args)
  //!
  //! Set this object to nonblocking mode.
  //!
  //! If @[read_cb] and @[extra_args] are specified, they will be passed on
  //! to @[set_read_callback()].
  //!
  //! @returns
  //! The called object.
  //!
  this_program set_nonblocking(void|function(mapping,mixed...:void) f,
			       mixed ... stuff)
  {
    if(f)
      set_read_callback(f,@stuff);
    return _set_nonblocking();
  }

  //! @decl UDP set_read_callback(function(mapping(string:int|string), @
  //!                                      mixed...) read_cb, @
  //!                             mixed ... extra_args);
  //!
  //! The @[read_cb] function will receive a mapping similar to the mapping
  //! returned by @[read()]:
  //! @mapping
  //!   @member string "data"
  //!     Received data.
  //!   @member string "ip"
  //!     Data was sent from this IP.
  //!   @member int "port"
  //!     Data was sent from this port.
  //! @endmapping
  //!
  //! @returns
  //! The called object.
  //!
  //! @seealso
  //! @[read()]
  //!
  this_program set_read_callback(function(mapping,mixed ...:void) f,
				 mixed ...ext)
  {
    extra=ext;
    _set_read_callback((callback = f) && _read_callback);
    return this;
  }
   
  private protected void _read_callback()
  {
    mapping i;
    if (i=read())
      callback(i,@extra);
  }
}

//! @decl void werror(string s)
//!
//! Write a message to stderr. Stderr is normally the console, even if
//! the process output has been redirected to a file or pipe.
//!
//! @note
//!   This function is identical to @[predef::werror()].
//!
//! @seealso
//!   @[predef::werror()]

constant werror=predef::werror;
