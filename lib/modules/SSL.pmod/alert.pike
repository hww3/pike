#pike __REAL_VERSION__

/* $Id: alert.pike,v 1.7 2002/03/20 16:40:00 nilsson Exp $
 *
 */

//! Alert package.

inherit "packet" : packet;

int level;
int description;

string message;
mixed trace;

constant is_alert = 1;

//! @decl void create(int level, int description, string|void message, mixed|void trace)
void create(int l, int d, int version, string|void m, mixed|void t)
{
  if (! ALERT_levels[l])
    error( "SSL.alert->create: Invalid level\n" );
  if (! ALERT_descriptions[d])
    error( "SSL.alert->create: Invalid description\n" );

  level = l;
  description = d;
  message = m;
  trace = t;

#ifdef SSL3_DEBUG
  if(m)
    werror(m);
  if(t)
    werror(describe_backtrace(t));
#endif

  packet::create();
  packet::content_type = PACKET_alert;
  packet::protocol_version = ({ 3, version });
  packet::fragment = sprintf("%c%c", level, description);
}
    
