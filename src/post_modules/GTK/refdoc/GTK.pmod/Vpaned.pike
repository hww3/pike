//! The paned window widgets are useful when you want to divide an area
//! into two parts, with the relative size of the two parts controlled
//! by the user. A groove is drawn between the two portions with a
//! handle that the user can drag to change the ratio. This widgets
//! makes a vertical division
//!
//!@code{ GTK.Vpaned()->add1(GTK.Label("Top Side Of Pane"))->add2(GTK.Label("Bottom"))->set_usize(100,100)@}
//!@xml{<image src='../images/gtk_vpaned.png'/>@}
//!
//!
//!
inherit Paned;

static Vpaned create( )
//!
