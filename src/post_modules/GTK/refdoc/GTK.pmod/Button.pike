//! A container that can only contain one child, and accepts events.
//! draws a bevelbox around itself.
//!@code{ GTK.Button("A button")@}
//!@xml{<image src='../images/gtk_button.png'/>@}
//!
//!@code{ GTK.Button("A button\nwith multiple lines\nof text")@}
//!@xml{<image src='../images/gtk_button_2.png'/>@}
//!
//!@code{ GTK.Button()->add(GTK.Image(GDK.Image(0)->set(Image.image(100,40)->test())))@}
//!@xml{<image src='../images/gtk_button_3.png'/>@}
//!
//!
//!
//!  Signals:
//! @b{clicked@}
//! Called when the button is pressed, and then released
//!
//!
//! @b{enter@}
//! Called when the mouse enters the button
//!
//!
//! @b{leave@}
//! Called when the mouse leaves the button
//!
//!
//! @b{pressed@}
//! Called when the button is pressed
//!
//!
//! @b{released@}
//! Called when the button is released
//!
//!
inherit Container;

Button clicked( )
//! Emulate a 'clicked' event (press followed by release).
//!
//!

static Button create( string|void label_text )
//! If a string is supplied, a W(Label) is created and added to the button.
//!
//!

Button enter( )
//! Emulate a 'enter' event.
//!
//!

GTK.Widget get_child( )
//! The (one and only) child of this container.
//!
//!

int get_relief( )
//! One of @[RELIEF_NONE], @[RELIEF_HALF] and @[RELIEF_NORMAL], set with set_relief()
//!
//!

Button leave( )
//! Emulate a 'leave' event.
//!
//!

Button pressed( )
//! Emulate a 'press' event.
//!
//!

Button released( )
//! Emulate a 'release' event.
//!
//!

Button set_relief( int newstyle )
//! One of @[RELIEF_NONE], @[RELIEF_HALF] and @[RELIEF_NORMAL]
//!
//!
