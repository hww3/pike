//! A HbuttonBox is very similar to a Hbox.
//! The major diffference is that the button box
//! is made to pack buttons in, and has a few convenience function for
//! normal button layouts.
//!
//!@code{ GTK.HbuttonBox()->add(GTK.Button("Hello"))->add(GTK.Button("World"))->set_usize(400,30)@}
//!@xml{<image src='../images/gtk_hbuttonbox.png'/>@}
//!
//!@code{ GTK.HbuttonBox()->add(GTK.Button("Hello"))->add(GTK.Button("World"))->set_layout(GTK.BUTTONBOX_SPREAD)->set_usize(400,30)@}
//!@xml{<image src='../images/gtk_hbuttonbox_2.png'/>@}
//!
//!@code{ GTK.HbuttonBox()->add(GTK.Button("Hello"))->add(GTK.Button("World"))->set_layout(GTK.BUTTONBOX_EDGE)->set_usize(400,30)@}
//!@xml{<image src='../images/gtk_hbuttonbox_3.png'/>@}
//!
//!@code{ GTK.HbuttonBox()->add(GTK.Button("Hello"))->add(GTK.Button("World"))->set_layout(GTK.BUTTONBOX_START)->set_usize(400,30)@}
//!@xml{<image src='../images/gtk_hbuttonbox_4.png'/>@}
//!
//!@code{ GTK.HbuttonBox()->add(GTK.Button("Hello"))->add(GTK.Button("World"))->set_layout(GTK.BUTTONBOX_END)->set_usize(400,30)@}
//!@xml{<image src='../images/gtk_hbuttonbox_5.png'/>@}
//!
//!
//!

inherit ButtonBox;

static HbuttonBox create( );
//! Create a new horizontal button box
//!
//!
