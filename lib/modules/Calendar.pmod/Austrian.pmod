//!
//! module Calendar
//! submodule Austrian
//!
//!	Same as the ISO calendar,
//!	but with austrian as the default language.
//!
//!	This calendar exist only for backwards compatible 
//!	purposes. 
//!

#pike __REAL_VERSION__

inherit Calendar.ISO:ISO;

import ".";
inherit ISO:ISO;

private static mixed __initstuff=lambda()
{
   default_rules=default_rules->set_language("austrian");
}();
