// $Id: Float.pmod,v 1.2 2003/11/07 21:55:45 mast Exp $

#pike __REAL_VERSION__

constant DIGITS_10 = __builtin.FLOAT_DIGITS_10;
constant MIN_10_EXP = __builtin.FLOAT_MIN_10_EXP;
constant MAX_10_EXP = __builtin.FLOAT_MAX_10_EXP;
constant MIN = __builtin.FLOAT_MIN;
constant MAX = __builtin.FLOAT_MAX;
constant EPSILON = __builtin.FLOAT_EPSILON;
//! These constants define the limits for floats on the current
//! architecture:
//!
//! @dl
//! @item DIGITS_10
//!   The number of decimal digits that can be represented. Any number
//!   with this many decimal digits can be stored in a float and
//!   converted back to decimal form without change.
//! @item MIN_10_EXP
//! @item MAX_10_EXP
//!   Limits of the exponent in decimal base. 10 raised to any number
//!   within this range can be represented in normalized form.
//! @item MIN
//!   The smallest normalized float greater than zero.
//! @item MAX
//!   The largest finite float.
//! @item EPSILON
//!   The difference between 1 and the smallest value greater than 1
//!   that can be represented.
//! @enddl
//!
//! @note
//! The size of the float type can be controlled when Pike is compiled
//! with the configure flags @expr{--with-double-precision@} and
//! @expr{--with-long-double-precision@}. The default is to use the
//! longest available float type that fits inside a pointer.

#if constant (__builtin.__FLOAT_PRECISION_FLOAT__)
constant FLOAT_PRECISION = __builtin.__FLOAT_PRECISION_FLOAT__;
#endif
#if constant (__builtin.__DOUBLE_PRECISION_FLOAT__)
constant DOUBLE_PRECISION = __builtin.__DOUBLE_PRECISION_FLOAT__;
#endif
#if constant (__builtin.__LONG_DOUBLE_PRECISION_FLOAT__)
constant LONG_DOUBLE_PRECISION = __builtin.__LONG_DOUBLE_PRECISION_FLOAT__;
#endif

//! @decl constant FLOAT_PRECISION;
//! @decl constant DOUBLE_PRECISION;
//! @decl constant LONG_DOUBLE_PRECISION;
//!
//! Tells which C compiler float type that is used for Pike floats.
//! Only one of these constants will exist (with the value @expr{1@})
//! at runtime.
//!
//! @dl
//! @item FLOAT_PRECISION
//!   The @expr{float@} type of the C compiler is used.
//! @item DOUBLE_PRECISION
//!   The @expr{double@} type of the C compiler is used.
//! @item LONG_DOUBLE_PRECISION
//!   The @expr{long double@} type of the C compiler is used.
//! @enddl
//!
//! @note
//! The float type can be controlled when Pike is compiled with the
//! configure flags @expr{--with-double-precision@} and
//! @expr{--with-long-double-precision@}. The default is to use the
//! longest available float type that fits inside a pointer.
