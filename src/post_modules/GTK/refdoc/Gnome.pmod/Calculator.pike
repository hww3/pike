//! This widget provides a simple calculator that you can embed in your
//! applications for doing quick computations.
//! 
//! The widget consists of a fully functional calculator including
//! standard arithmetic functions as well as trigonometric
//! capabilities, exponents, factorials, nested equations, and others.
//!@code{ Gnome.Calculator()@}
//!@xml{<image src='../images/gnome_calculator.png'/>@}
//!
//! 
//!
//!
//!  Signals:
//! @b{result_changed@}
//! This signal is emited by the widget when the result has been changed.
//!
//!
inherit Vbox;

GnomeCalculator clear( int reset )
//! Resets the calculator back to zero. If reset is TRUE, results
//! stored in memory and the calculator mode are cleared also.
//!
//!

static GnomeCalculator create( )
//! Create a new calculator widget
//!
//!

float get_result( )
//! Value currently stored in calculator buffer.
//!
//!

GnomeCalculator set( float result )
//! Sets the value stored in the calculator's result buffer to the
//! given result.
//!
//!
