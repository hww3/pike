//! The W(Alignment) widget controls the alignment and size of its
//! child widget. It has four settings: xscale, yscale, xalign, and
//! yalign.
//! 
//! The scale settings are used to specify how much the child widget
//! should expand to fill the space allocated to the W(Alignment). The
//! values can range from 0 (meaning the child doesn't expand at all)
//! to 1 (meaning the child expands to fill all of the available
//! space).
//! 
//! The align settings are used to place the child widget within the
//! available area. The values range from 0 (top or left) to 1 (bottom
//! or right). Of course, if the scale settings are both set to 1, the
//! alignment settings have no effect.
//! 
//! NOIMG
//!
//!
inherit Bin;

static Alignment create( float xalign, float yalign, float xscale, float yscale )
//! <table>
//! <tr><td>xalign :</td>
//! <td>the horizontal alignment of the child widget, from 0 (left) to 1 (right).</td></tr>
//! <tr><td>                  yalign :</td>
//! <td>the vertical alignment of the child widget, from 0 (top) to 1 (bottom).</td></tr>
//! <tr><td>                  xscale :</td>
//! <td>the amount that the child widget expands horizontally to fill up unused space, from 0 to 1. A value of 0 indicates that the child widget should never expand. A value of 1 indicates that the child widget will expand to fill all of the space allocated for the GTK.Alignment.</td></tr>
//! <tr><td>                  yscale :</td>
//! <td>the amount that the child widget expands vertically to fill up unused space, from 0 to 1. The values are similar to xscale.</td></tr>
//! </table>
//!
//!

float get_xalign( )
//! the horizontal alignment of the child widget, from 0 (left) to 1 (right).
//!
//!

float get_xscale( )
//! the amount that the child widget expands horizontally to fill up
//! unused space, from 0 to 1. A value of 0 indicates that the child
//! widget should never expand. A value of 1 indicates that the child
//! widget will expand to fill all of the space allocated for the
//! GTK.Alignment.
//!
//!

float get_yalign( )
//! the vertical alignment of the child widget, from 0 (top) to 1 (bottom).
//!
//!

float get_yscale( )
//! the amount that the child widget expands vertically to fill up
//! unused space, from 0 to 1. The values are similar to xscale.
//!
//!

Alignment set( float xalign, float yalign, float xscale, float yscale )
//! <table>
//! <tr><td>xalign :</td>
//! <td>the horizontal alignment of the child widget, from 0 (left) to 1 (right).</td></tr>
//! <tr><td>                  yalign :</td>
//! <td>the vertical alignment of the child widget, from 0 (top) to 1 (bottom).</td></tr>
//! <tr><td>                  xscale :</td>
//! <td>the amount that the child widget expands horizontally to fill up unused space, from 0 to 1. A value of 0 indicates that the child widget should never expand. A value of 1 indicates that the child widget will expand to fill all of the space allocated for the GTK.Alignment.</td></tr>
//! <tr><td>                  yscale :</td>
//! <td>the amount that the child widget expands vertically to fill up unused space, from 0 to 1. The values are similar to xscale.</td></tr>
//! </table>
//!
//!
