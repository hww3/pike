//! A GTK.Menu is a W(MenuShell) that implements a drop down menu
//! consisting of a list of W(MenuItem) objects which can be navigated
//! and activated by the user to perform application functions.
//! 
//! A GTK.Menu is most commonly dropped down by activating a W(MenuItem)
//! in a W(MenuBar) or popped up by activating a W(MenuItem) in another
//! GTK.Menu.
//! 
//! A GTK.Menu can also be popped up by activating a
//! W(OptionMenu). Other composite widgets such as the W(Notebook) can
//! pop up a GTK.Menu as well.
//! 
//! Applications can display a GTK.Menu as a popup menu by calling the
//! popup() function. The example below shows how an application can
//! pop up a menu when the 3rd mouse button is pressed.
//! 
//! <pre><font size="-1">
//! &nbsp; GTK.Menu menu = create_menu();
//! &nbsp; GTK.Window window = create_window();
//! &nbsp; window->signal_connect( "button_press_event", lambda(GTK.Menu m,
//! &nbsp;                                                      GTK.Window w,
//! &nbsp;                                                      mapping e ) {
//! &nbsp;           if( e->button == 3 )
//! &nbsp;            menu->popup();
//! &nbsp;        }, menu );
//! </font></pre>
//!
//!

inherit MenuShell;

Menu append( GTK.Widget widget );
//! Adds a new W(MenuItem) to the end of the menu's item list.
//!
//!

Menu attach_to_widget( GTK.Widget widget );
//! Attach the menu to a widget
//!
//!

static Menu create( );
//! Create a new GTK.Menu widget.
//!
//!

Menu detach( );
//!  Detaches the menu from the widget to which it had been attached.
//!
//!

GTK.Widget get_active( );
//! Returns the selected menu item from the menu.
//!  This is used by the W(OptionMenu).
//!
//!

GTK.Widget get_attach_widget( );
//! Returns the W(Widget) that the menu is attached to.
//!
//!

int get_torn_off( );
//! return 1 if the menu is torn off.
//!
//!

Menu insert( GTK.Widget widget, int position );
//! Adds a new W(MenuItem) to the menu's item list at the position
//! indicated by position.
//!
//!

Menu popdown( );
//! Removes the menu from the screen.
//!
//!

Menu popup( int|void button_pressed_to_show_menu );
//! The default button is 3.
//!
//!

Menu prepend( GTK.Widget widget );
//! Adds a new W(MenuItem) to the start of the menu's item list.
//!
//!

Menu reorder_child( GTK.Widget menuitem, int pos );
//! Moves a W(MenuItem) to a new position within the GTK.Menu.
//!
//!

Menu reposition( );
//! Repositions the menu according to its position function.
//!
//!

Menu set_accel_group( GTK.AccelGroup accelerators );
//! Set the W(AccelGroup) which holds global accelerators for the menu.
//!
//!

Menu set_active( int activep );
//! Selects the specified menu item within the menu. This is used by
//! the W(OptionMenu).
//!
//!

Menu set_tearoff_state( int torn_off );
//! Changes the tearoff state of the menu. A menu is normally displayed
//! as drop down menu which persists as long as the menu is active. It
//! can also be displayed as a tearoff menu which persists until it is
//! closed or reattached.
//!
//!

Menu set_title( string new_title );
//! Sets the title string for the menu. The title is displayed when the
//! menu is shown as a tearoff menu.
//!
//!
