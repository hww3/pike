//!
//! module Calendar
//! submodule Swedish
//!
//!	Same as the ISO calendar,
//!	but with swedish as the default language.
//!
//!	This calendar exist only for backwards compatible 
//!	purposes. 
//!

import ".";
inherit ISO:ISO;

private static mixed __initstuff=lambda()
{
   default_rules=default_rules->set_language("SE_sv");
}();
