//!  GnomeFontPicker - Button that displays current font; click to select new font.
//!@code{ Gnome.FontPicker();@}
//!@xml{<image src='../images/gnome_fontpicker.png'/>@}
//!
//!@code{ Gnome.FontPicker()->set_mode( Gnome.FontPickerModeFontInfo );@}
//!@xml{<image src='../images/gnome_fontpicker_2.png'/>@}
//!
//!
//!
//!  Signals:
//! @b{font_set@}
//!

inherit GTK.Button;

static Gnome.FontPicker create( );
//! Create a new font pick button
//!
//!

Gnome.FontPicker fi_set_show_size( int show_size );
//! If show_size is TRUE, font size will be displayed along with font
//! chosen by user. This only applies if current button mode is
//! Gnome.FontPickerModeFontInfo.
//!
//!

Gnome.FontPicker fi_set_use_font_in_label( int use_font_in_label, int size );
//! If use_font_in_label is TRUE, font name will be written using font
//! chosen by user and using size passed to this function. This only
//! applies if current button mode is Gnome.FontPickerModeFontInfo.
//!
//!

GDK.Font get_font( );
//! Retrieves the font from the font selection dialog.
//!
//!

string get_font_name( );
//! Retrieve name of font from font selection dialog.
//!
//!

int get_mode( );
//! Returns current font picker button mode (or what to show).
//!
//!

string get_preview_text( );
//! Retrieve preview text from font selection dialog if available.
//!
//!

int set_font_name( string fontname );
//! Set or update the currently displayed font in the font picker dialog
//!
//!

Gnome.FontPicker set_mode( int mode );
//! Set value of subsequent font picker button mode (or what to show).
//! Mode is one of @[GNOME_FONT_PICKER_MODE_UNKNOWN], @[GNOME_FONT_PICKER_MODE_PIXMAP], @[GNOME_FONT_PICKER_MODE_FONT_INFO] and @[GNOME_FONT_PICKER_MODE_USER_WIDGET]
//!
//!

Gnome.FontPicker set_preview_text( string text );
//! Set preview text in font picker, and in font selection dialog if
//! one is being displayed.
//!
//!

Gnome.FontPicker set_title( string title );
//! Sets the title for the font selection dialog.
//!
//!

Gnome.FontPicker uw_set_widget( GTK.Widget widget );
//! Set the user-supplied widget as the inside of the font picker. This
//! only applies with Gnome.FontPickerModeUserWidget.
//!
//!
