//! The Gnome.PropertyBox widget simplifies coding a consistent dialog
//! box for configuring properties of any kind.
//! 
//! The Gnome.PropertyBox is a toplevel widget (it will create its own
//! window), inside it contains a GtkNotebook which is used to hold the
//! various property pages.
//! 
//! The box will include ok, cancel, apply and help buttons (the actual
//! buttons depends on the settings the user has, for example, apply
//! can be hidden). The ok and apply buttons will start up in
//! non-sensitive state, the programmer needs to configure the widgets
//! inserted into the property box to inform the widget of any state
//! changes to enable the ok and apply buttons. This is done by calling
//! the changed() function.
//! 
//! To use this widget, you create the widget and then you call
//! append_page() for each property page you want in the property box.
//! 
//! The widget emits two signals: "apply" and "help". To make a
//! functional dialog box you will want to connect to at least the
//! "apply" signal. Your function will be invoked once for each page
//! and one more time at the end, passing a special value of -1 for the
//! page number.
//! 
//!
//!
//!  Signals:
//! @b{apply@}
//! This signal is invoked with the page number that is being
//! applied. The signal is emited with the special page number -1 when
//! it has finished emiting the signals for all of the property pages.
//!
//!
//! @b{help@}
//! This signal is invoked when the user clicks on the help button in
//! the property box. An argument is passed that identifies the
//! currently active page number.
//!
//!

inherit Gnome.Dialog;

int append_page( GTK.Widget child, GTK.Widget tab_label );
//! Appends a new page to the Gnome.PropertyBox.
//! widget is the widget that is being inserted, and tab_label will be
//! used as the label for this configuration page.
//!
//!

Gnome.PropertyBox changed( );
//! When a setting has changed, the code needs to invoke this routine
//! to make the Ok/Apply buttons sensitive.
//!
//!

static Gnome.PropertyBox create( );
//! Creates a new Gnome.PropertyBox widget.
//!
//!

Gnome.PropertyBox set_state( int state );
//!
