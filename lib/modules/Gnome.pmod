#pike __REAL_VERSION__

mixed `[](string what)
{
  if(what == "_module_value") return ([])[0];
  return (GTK["Gnome"+what] || GTK["gnome_"+what]);
}

array _indices()
{
  return glob( "GNOME_*", indices(GTK) ) + glob( "Gnome_*", indices(GTK) );
}
