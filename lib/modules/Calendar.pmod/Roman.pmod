//!
//! module Calendar
//! submodule Roman
//!
//! base for all Roman-kind of calendars
//! ie, one with years, months, weeks and days
//!

#pike __VERSION__
#pragma strict_types

import ".";
inherit Time:Time;

class cMinute
{
   inherit Time::cMinute;
   void bip() { werror("roman\n"); }
   int roman(int x,int y) { return 17; }
}

