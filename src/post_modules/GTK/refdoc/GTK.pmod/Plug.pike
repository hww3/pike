//! Together with W(Socket), GTK.Plug provides the ability to embed
//! widgets from one process into another process in a fashion that is
//! transparent to the user. One process creates a W(Socket) widget
//! and, passes the XID of that widgets window to the other process,
//! which then creates a GTK.Plug window with that XID. Any widgets
//! contained in the GTK.Plug then will appear inside the first
//! applications window.
//!
//!

inherit GTK.Window;

static GTK.Plug create( int socket_id );
//! Create a new plug, the socket_id is the window into which this plug
//! will be plugged.
//!
//!

int get_same_app( );
//! returns 1 if the socket the plug is connected to is in this
//! application.
//!
//!
