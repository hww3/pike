#pike __REAL_VERSION__

// Create a simple alert window. The default title is @expr{Alert@}. A
// single button @expr{Ok@} will be created. Pressing it removes the
// dialog.

inherit GTK.Dialog;

void begone(object w2)
{
  destruct( this );
}

object ok_button;

//! Returns the ok @[Button] object.
GTK.Button ok()
{
  return ok_button;
}

//!
void create(string text, string|void title)
{
  ::create();
  object l = GTK.Label( text );
  set_title( title||"Alert" );
  l->set_justify( GTK.JUSTIFY_LEFT );
  l->set_alignment(0.0, 0.5);
  l->set_padding( 20, 20 );
  vbox()->add( l->show() );
  ok_button = GTK.Button("OK");
  ok_button->signal_connect( "clicked", begone );
  action_area()->add( ok_button->show() );
  show();
}
