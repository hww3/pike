//! This widget implements a graphical "more" command. It allows the
//! user to view a text file. There are various possible ways to
//! specify the contents to display: loading the data from a file (by
//! providing a filename) or by loading it from an open file data
//! stream or from the output of a Unix command.
//!
//!@code{ Gnome.Less()->show_string("Example string\nshown in this\nwidget")@}
//!@xml{<image src='../images/gnome_less.png'/>@}
//!
//!@code{ Gnome.Less()->show_file("/usr/dict/words" );@}
//!@xml{<image src='../images/gnome_less_2.png'/>@}
//!
//!@code{ Gnome.Less()->show_command( "psrinfo -v" )@}
//!@xml{<image src='../images/gnome_less_3.png'/>@}
//!
//! 
//!
//!

inherit Vbox;

GnomeLess clear( );
//! Clears all the text
//!
//!

static GnomeLess create( );
//! Creates a new GnomeLess widget.
//!
//!

GnomeLess reshow( );
//! Re-displays all of the text in the GnomeLess widget gl. If the font
//! has changed since the last show/reshow of text, it will update the
//! current text to the new font.
//!
//!

GnomeLess set_fixed_font( int fixed );
//! Specifies whether or not new text should be displayed using a fixed
//! font. Pass TRUE in fixed to use a fixed font, or FALSE to revert to
//! the default GtkText font.
//! 
//! Note: This will not affect text already being displayed. If you use
//! this function after adding text to the widget, you must show it
//! again by using gnome_less_reshow or one of the gnome_less_show
//! commands.
//!
//!

GnomeLess set_font( GDK.Font font );
//! Sets the font of the text to be displayed in the GnomeLess widget
//! gl to font.
//! Note: This will not affect text already being displayed. If you use
//! this function after adding text to the widget, you must show it
//! again by using reshow or one of the show commands.
//!
//!

GnomeLess show_command( string command_line );
//! Runs the shell command specified in command_line, and places the
//! output of that command in the GnomeLess widget specified by
//! gl. Replaces any text already being displayed in the widget.
//!
//!

GnomeLess show_file( string file );
//! Displays a file in a GnomeLess widget. Replaces any text already
//! being displayed in the widget.
//!
//!

GnomeLess show_filestream( object(implements 1) stream );
//!

GnomeLess show_string( string data );
//! Displays a string in the GnomeLess widget gl. Replaces any text
//! already being displayed.
//!
//!

int write_file( string path );
//! Writes the text displayed in the GnomeLess widget gl to the file
//! specified by path.
//!
//!

int write_filestream( object(implements 1) fd );
//!
