/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id$
*/

void init_sdl_constants() {
  /* Video related flags */
  add_integer_constant("SWSURFACE", SDL_SWSURFACE, 0);
  add_integer_constant("HWSURFACE", SDL_HWSURFACE, 0);
  add_integer_constant("ASYNCBLIT", SDL_ASYNCBLIT, 0);
  add_integer_constant("ANYFORMAT", SDL_ANYFORMAT, 0);
  add_integer_constant("HWPALETTE", SDL_HWPALETTE, 0);
  add_integer_constant("DOUBLEBUF", SDL_DOUBLEBUF, 0);
  add_integer_constant("FULLSCREEN", SDL_FULLSCREEN, 0);
  add_integer_constant("OPENGL", SDL_OPENGL, 0);
  add_integer_constant("OPENGLBLIT", SDL_OPENGLBLIT, 0);
  add_integer_constant("RESIZABLE", SDL_RESIZABLE, 0);
  add_integer_constant("NOFRAME", SDL_NOFRAME, 0);
  add_integer_constant("HWACCEL", SDL_HWACCEL, 0);
  add_integer_constant("SRCCOLORKEY", SDL_SRCCOLORKEY, 0);
  add_integer_constant("RLEACCELOK", SDL_RLEACCELOK, 0);
  add_integer_constant("RLEACCEL", SDL_RLEACCEL, 0);
  add_integer_constant("SRCALPHA", SDL_SRCALPHA, 0);
  add_integer_constant("PREALLOC", SDL_PREALLOC, 0);
  add_integer_constant("ALPHA_OPAQUE", SDL_ALPHA_OPAQUE, 0);
  add_integer_constant("ALPHA_TRANSPARENT", SDL_ALPHA_TRANSPARENT, 0);
  add_integer_constant("YV12_OVERLAY", SDL_YV12_OVERLAY, 0);
  add_integer_constant("IYUV_OVERLAY", SDL_IYUV_OVERLAY, 0);
  add_integer_constant("YUY2_OVERLAY", SDL_YUY2_OVERLAY, 0);
  add_integer_constant("UYVY_OVERLAY", SDL_UYVY_OVERLAY, 0);
  add_integer_constant("YVYU_OVERLAY", SDL_YVYU_OVERLAY, 0);
  add_integer_constant("GRAB_QUERY", SDL_GRAB_QUERY, 0);
  add_integer_constant("GRAB_OFF", SDL_GRAB_OFF, 0);
  add_integer_constant("GRAB_ON", SDL_GRAB_ON, 0);
  add_integer_constant("GRAB_FULLSCREEN", SDL_GRAB_FULLSCREEN, 0);

  /* GL related constants */
  add_integer_constant( "GL_RED_SIZE", SDL_GL_RED_SIZE, 0 );
  add_integer_constant( "GL_GREEN_SIZE", SDL_GL_GREEN_SIZE, 0 );
  add_integer_constant( "GL_BLUE_SIZE", SDL_GL_BLUE_SIZE, 0 );
  add_integer_constant( "GL_DEPTH_SIZE", SDL_GL_DEPTH_SIZE, 0 );
  add_integer_constant( "GL_DOUBLEBUFFER", SDL_GL_DOUBLEBUFFER, 0 );
  
  /* Init related constants */
  /* TIMER is disabled because Pike doesn't need that functionality.
     add_integer_constant("INIT_TIMER", SDL_INIT_TIMER, 0);
  */
  add_integer_constant("INIT_AUDIO", SDL_INIT_AUDIO, 0);
  add_integer_constant("INIT_VIDEO", SDL_INIT_VIDEO, 0);
  add_integer_constant("INIT_CDROM", SDL_INIT_CDROM, 0);
  add_integer_constant("INIT_JOYSTICK", SDL_INIT_JOYSTICK, 0);
  add_integer_constant("INIT_NOPARACHUTE", SDL_INIT_NOPARACHUTE, 0);
  add_integer_constant("INIT_EVENTTHREAD", SDL_INIT_EVENTTHREAD, 0);
  add_integer_constant("INIT_EVERYTHING", SDL_INIT_EVERYTHING, 0);

  /* App active constants */
  add_integer_constant("APPMOUSEFOCUS", SDL_APPMOUSEFOCUS, 0);
  add_integer_constant("APPINPUTFOCUS", SDL_APPINPUTFOCUS, 0);
  add_integer_constant("APPACTIVE", SDL_APPACTIVE, 0);
  
  /* Audio related constants */
  add_integer_constant("AUDIO_U8", AUDIO_U8, 0);
  add_integer_constant("AUDIO_S8", AUDIO_S8, 0);
  add_integer_constant("AUDIO_U16LSB", AUDIO_U16LSB, 0);
  add_integer_constant("AUDIO_S16LSB", AUDIO_S16LSB, 0);
  add_integer_constant("AUDIO_U16MSB", AUDIO_U16MSB, 0);
  add_integer_constant("AUDIO_S16MSB", AUDIO_S16MSB, 0);
  add_integer_constant("AUDIO_U16", AUDIO_U16, 0);
  add_integer_constant("AUDIO_S16", AUDIO_S16, 0);
  add_integer_constant("AUDIO_U16SYS", AUDIO_U16SYS, 0);
  add_integer_constant("AUDIO_S16SYS", AUDIO_S16SYS, 0);
 
  /* Joystick hat constants */
  add_integer_constant("HAT_CENTERED", SDL_HAT_CENTERED, 0);
  add_integer_constant("HAT_UP", SDL_HAT_UP, 0);
  add_integer_constant("HAT_RIGHT", SDL_HAT_RIGHT, 0);
  add_integer_constant("HAT_DOWN", SDL_HAT_DOWN, 0);
  add_integer_constant("HAT_LEFT", SDL_HAT_LEFT, 0);
  add_integer_constant("HAT_RIGHTUP", SDL_HAT_RIGHTUP, 0);
  add_integer_constant("HAT_RIGHTDOWN", SDL_HAT_RIGHTDOWN, 0);
  add_integer_constant("HAT_LEFTUP", SDL_HAT_LEFTUP, 0);
  add_integer_constant("HAT_LEFTDOWN", SDL_HAT_LEFTDOWN, 0);  
  
  /* Event related constants */
  add_integer_constant("ALLEVENTS", SDL_ALLEVENTS, 0);
  add_integer_constant("QUERY", SDL_QUERY, 0);
  add_integer_constant("IGNORE", SDL_IGNORE, 0);
  add_integer_constant("DISABLE", SDL_DISABLE, 0);
  add_integer_constant("ENABLE", SDL_ENABLE, 0);
  
  add_integer_constant("NOEVENT", SDL_NOEVENT, 0);
  add_integer_constant("ACTIVEEVENT", SDL_ACTIVEEVENT, 0);
  add_integer_constant("KEYDOWN", SDL_KEYDOWN, 0);
  add_integer_constant("KEYUP", SDL_KEYUP, 0);
  add_integer_constant("MOUSEMOTION", SDL_MOUSEMOTION, 0);
  add_integer_constant("MOUSEBUTTONDOWN", SDL_MOUSEBUTTONDOWN, 0);
  add_integer_constant("MOUSEBUTTONUP", SDL_MOUSEBUTTONUP, 0);
  add_integer_constant("JOYAXISMOTION", SDL_JOYAXISMOTION, 0);
  add_integer_constant("JOYBALLMOTION", SDL_JOYBALLMOTION, 0);
  add_integer_constant("JOYHATMOTION", SDL_JOYHATMOTION, 0);
  add_integer_constant("JOYBUTTONDOWN", SDL_JOYBUTTONDOWN, 0);
  add_integer_constant("JOYBUTTONUP", SDL_JOYBUTTONUP, 0);
  add_integer_constant("QUIT", SDL_QUIT, 0);
  add_integer_constant("SYSWMEVENT", SDL_SYSWMEVENT, 0);
  add_integer_constant("VIDEORESIZE", SDL_VIDEORESIZE, 0);
  add_integer_constant("VIDEOEXPOSE", SDL_VIDEOEXPOSE, 0);
  add_integer_constant("USEREVENT", SDL_USEREVENT, 0);

  add_integer_constant("ACTIVEEVENTMASK", SDL_ACTIVEEVENTMASK, 0);
  add_integer_constant("KEYDOWNMASK", SDL_KEYDOWNMASK, 0);
  add_integer_constant("KEYUPMASK", SDL_KEYUPMASK, 0);
  add_integer_constant("MOUSEMOTIONMASK", SDL_MOUSEMOTIONMASK, 0);
  add_integer_constant("MOUSEBUTTONDOWNMASK", SDL_MOUSEBUTTONDOWNMASK, 0);
  add_integer_constant("MOUSEBUTTONUPMASK", SDL_MOUSEBUTTONUPMASK, 0);
  add_integer_constant("MOUSEEVENTMASK", SDL_MOUSEEVENTMASK, 0);
  add_integer_constant("JOYAXISMOTIONMASK", SDL_JOYAXISMOTIONMASK, 0);
  add_integer_constant("JOYBALLMOTIONMASK", SDL_JOYBALLMOTIONMASK, 0);
  add_integer_constant("JOYHATMOTIONMASK", SDL_JOYHATMOTIONMASK, 0);
  add_integer_constant("JOYBUTTONDOWNMASK", SDL_JOYBUTTONDOWNMASK, 0);
  add_integer_constant("JOYBUTTONUPMASK", SDL_JOYBUTTONUPMASK, 0);
  add_integer_constant("JOYEVENTMASK", SDL_JOYEVENTMASK, 0);
  add_integer_constant("VIDEORESIZEMASK", SDL_VIDEORESIZEMASK, 0);
  add_integer_constant("VIDEOEXPOSEMASK", SDL_VIDEOEXPOSEMASK, 0);
  add_integer_constant("QUITMASK", SDL_QUITMASK, 0);
  add_integer_constant("SYSWMEVENTMASK", SDL_SYSWMEVENTMASK, 0);

  /* Byte order related constants */ 
  add_integer_constant("LIL_ENDIAN", SDL_LIL_ENDIAN, 0);
  add_integer_constant("BIG_ENDIAN", SDL_BIG_ENDIAN, 0);
  add_integer_constant("BYTEORDER", SDL_BYTEORDER, 0);

  /* CD Related constants */
  add_integer_constant("MAX_TRACKS", SDL_MAX_TRACKS, 0);
  add_integer_constant("AUDIO_TRACK", SDL_AUDIO_TRACK, 0);
  add_integer_constant("DATA_TRACK", SDL_DATA_TRACK, 0);
  add_integer_constant("CD_FPS", CD_FPS, 0);
  add_integer_constant("CD_TRAYEMPTY", CD_TRAYEMPTY, 0);
  add_integer_constant("CD_STOPPED", CD_STOPPED, 0);
  add_integer_constant("CD_PLAYING", CD_PLAYING, 0);
  add_integer_constant("CD_PAUSED", CD_PAUSED, 0);
  add_integer_constant("CD_ERROR", CD_ERROR, 0);
  
  /* Mouse related constants */
  add_integer_constant("BUTTON_LEFT", SDL_BUTTON_LEFT, 0);
  add_integer_constant("BUTTON_MIDDLE", SDL_BUTTON_MIDDLE, 0);
  add_integer_constant("BUTTON_RIGHT", SDL_BUTTON_RIGHT, 0);
  add_integer_constant("BUTTON_LMASK", SDL_BUTTON_LMASK, 0);
  add_integer_constant("BUTTON_MMASK", SDL_BUTTON_MMASK, 0);
  add_integer_constant("BUTTON_RMASK", SDL_BUTTON_RMASK, 0);

  /* Version */
  add_integer_constant("MAJOR_VERSION", SDL_MAJOR_VERSION, 0);
  add_integer_constant("MINOR_VERSION", SDL_MINOR_VERSION, 0);
  add_integer_constant("PATCHLEVEL", SDL_PATCHLEVEL, 0);
  
  /* Keyboard constants */
  add_integer_constant("ALL_HOTKEYS", SDL_ALL_HOTKEYS, 0);
  add_integer_constant("DEFAULT_REPEAT_DELAY", SDL_DEFAULT_REPEAT_DELAY, 0);
  add_integer_constant("DEFAULT_REPEAT_INTERVAL", SDL_DEFAULT_REPEAT_INTERVAL, 0);

  /* Keysyms :  FIXME - autogenerate  */
  add_integer_constant("K_UNKNOWN", SDLK_UNKNOWN, 0);
  add_integer_constant("K_FIRST", SDLK_FIRST, 0);
  add_integer_constant("K_BACKSPACE", SDLK_BACKSPACE, 0);
  add_integer_constant("K_TAB", SDLK_TAB, 0);
  add_integer_constant("K_CLEAR", SDLK_CLEAR, 0);
  add_integer_constant("K_RETURN", SDLK_RETURN, 0);
  add_integer_constant("K_PAUSE", SDLK_PAUSE, 0);
  add_integer_constant("K_ESCAPE", SDLK_ESCAPE, 0);
  add_integer_constant("K_SPACE", SDLK_SPACE, 0);
  add_integer_constant("K_EXCLAIM", SDLK_EXCLAIM, 0);
  add_integer_constant("K_QUOTEDBL", SDLK_QUOTEDBL, 0);
  add_integer_constant("K_HASH", SDLK_HASH, 0);
  add_integer_constant("K_DOLLAR", SDLK_DOLLAR, 0);
  add_integer_constant("K_AMPERSAND", SDLK_AMPERSAND, 0);
  add_integer_constant("K_QUOTE", SDLK_QUOTE, 0);
  add_integer_constant("K_LEFTPAREN", SDLK_LEFTPAREN, 0);
  add_integer_constant("K_RIGHTPAREN", SDLK_RIGHTPAREN, 0);
  add_integer_constant("K_ASTERISK", SDLK_ASTERISK, 0);
  add_integer_constant("K_PLUS", SDLK_PLUS, 0);
  add_integer_constant("K_COMMA", SDLK_COMMA, 0);
  add_integer_constant("K_MINUS", SDLK_MINUS, 0);
  add_integer_constant("K_PERIOD", SDLK_PERIOD, 0);
  add_integer_constant("K_SLASH", SDLK_SLASH, 0);
  add_integer_constant("K_0", SDLK_0, 0);
  add_integer_constant("K_1", SDLK_1, 0);
  add_integer_constant("K_2", SDLK_2, 0);
  add_integer_constant("K_3", SDLK_3, 0);
  add_integer_constant("K_4", SDLK_4, 0);
  add_integer_constant("K_5", SDLK_5, 0);
  add_integer_constant("K_6", SDLK_6, 0);
  add_integer_constant("K_7", SDLK_7, 0);
  add_integer_constant("K_8", SDLK_8, 0);
  add_integer_constant("K_9", SDLK_9, 0);
  add_integer_constant("K_COLON", SDLK_COLON, 0);
  add_integer_constant("K_SEMICOLON", SDLK_SEMICOLON, 0);
  add_integer_constant("K_LESS", SDLK_LESS, 0);
  add_integer_constant("K_EQUALS", SDLK_EQUALS, 0);
  add_integer_constant("K_GREATER", SDLK_GREATER, 0);
  add_integer_constant("K_QUESTION", SDLK_QUESTION, 0);
  add_integer_constant("K_AT", SDLK_AT, 0);
  add_integer_constant("K_LEFTBRACKET", SDLK_LEFTBRACKET, 0);
  add_integer_constant("K_BACKSLASH", SDLK_BACKSLASH, 0);
  add_integer_constant("K_RIGHTBRACKET", SDLK_RIGHTBRACKET, 0);
  add_integer_constant("K_CARET", SDLK_CARET, 0);
  add_integer_constant("K_UNDERSCORE", SDLK_UNDERSCORE, 0);
  add_integer_constant("K_BACKQUOTE", SDLK_BACKQUOTE, 0);
  add_integer_constant("K_a", SDLK_a, 0);
  add_integer_constant("K_b", SDLK_b, 0);
  add_integer_constant("K_c", SDLK_c, 0);
  add_integer_constant("K_d", SDLK_d, 0);
  add_integer_constant("K_e", SDLK_e, 0);
  add_integer_constant("K_f", SDLK_f, 0);
  add_integer_constant("K_g", SDLK_g, 0);
  add_integer_constant("K_h", SDLK_h, 0);
  add_integer_constant("K_i", SDLK_i, 0);
  add_integer_constant("K_j", SDLK_j, 0);
  add_integer_constant("K_k", SDLK_k, 0);
  add_integer_constant("K_l", SDLK_l, 0);
  add_integer_constant("K_m", SDLK_m, 0);
  add_integer_constant("K_n", SDLK_n, 0);
  add_integer_constant("K_o", SDLK_o, 0);
  add_integer_constant("K_p", SDLK_p, 0);
  add_integer_constant("K_q", SDLK_q, 0);
  add_integer_constant("K_r", SDLK_r, 0);
  add_integer_constant("K_s", SDLK_s, 0);
  add_integer_constant("K_t", SDLK_t, 0);
  add_integer_constant("K_u", SDLK_u, 0);
  add_integer_constant("K_v", SDLK_v, 0);
  add_integer_constant("K_w", SDLK_w, 0);
  add_integer_constant("K_x", SDLK_x, 0);
  add_integer_constant("K_y", SDLK_y, 0);
  add_integer_constant("K_z", SDLK_z, 0);
  add_integer_constant("K_DELETE", SDLK_DELETE, 0);
  add_integer_constant("K_WORLD_0", SDLK_WORLD_0, 0);
  add_integer_constant("K_WORLD_1", SDLK_WORLD_1, 0);
  add_integer_constant("K_WORLD_2", SDLK_WORLD_2, 0);
  add_integer_constant("K_WORLD_3", SDLK_WORLD_3, 0);
  add_integer_constant("K_WORLD_4", SDLK_WORLD_4, 0);
  add_integer_constant("K_WORLD_5", SDLK_WORLD_5, 0);
  add_integer_constant("K_WORLD_6", SDLK_WORLD_6, 0);
  add_integer_constant("K_WORLD_7", SDLK_WORLD_7, 0);
  add_integer_constant("K_WORLD_8", SDLK_WORLD_8, 0);
  add_integer_constant("K_WORLD_9", SDLK_WORLD_9, 0);
  add_integer_constant("K_WORLD_10", SDLK_WORLD_10, 0);
  add_integer_constant("K_WORLD_11", SDLK_WORLD_11, 0);
  add_integer_constant("K_WORLD_12", SDLK_WORLD_12, 0);
  add_integer_constant("K_WORLD_13", SDLK_WORLD_13, 0);
  add_integer_constant("K_WORLD_14", SDLK_WORLD_14, 0);
  add_integer_constant("K_WORLD_15", SDLK_WORLD_15, 0);
  add_integer_constant("K_WORLD_16", SDLK_WORLD_16, 0);
  add_integer_constant("K_WORLD_17", SDLK_WORLD_17, 0);
  add_integer_constant("K_WORLD_18", SDLK_WORLD_18, 0);
  add_integer_constant("K_WORLD_19", SDLK_WORLD_19, 0);
  add_integer_constant("K_WORLD_20", SDLK_WORLD_20, 0);
  add_integer_constant("K_WORLD_21", SDLK_WORLD_21, 0);
  add_integer_constant("K_WORLD_22", SDLK_WORLD_22, 0);
  add_integer_constant("K_WORLD_23", SDLK_WORLD_23, 0);
  add_integer_constant("K_WORLD_24", SDLK_WORLD_24, 0);
  add_integer_constant("K_WORLD_25", SDLK_WORLD_25, 0);
  add_integer_constant("K_WORLD_26", SDLK_WORLD_26, 0);
  add_integer_constant("K_WORLD_27", SDLK_WORLD_27, 0);
  add_integer_constant("K_WORLD_28", SDLK_WORLD_28, 0);
  add_integer_constant("K_WORLD_29", SDLK_WORLD_29, 0);
  add_integer_constant("K_WORLD_30", SDLK_WORLD_30, 0);
  add_integer_constant("K_WORLD_31", SDLK_WORLD_31, 0);
  add_integer_constant("K_WORLD_32", SDLK_WORLD_32, 0);
  add_integer_constant("K_WORLD_33", SDLK_WORLD_33, 0);
  add_integer_constant("K_WORLD_34", SDLK_WORLD_34, 0);
  add_integer_constant("K_WORLD_35", SDLK_WORLD_35, 0);
  add_integer_constant("K_WORLD_36", SDLK_WORLD_36, 0);
  add_integer_constant("K_WORLD_37", SDLK_WORLD_37, 0);
  add_integer_constant("K_WORLD_38", SDLK_WORLD_38, 0);
  add_integer_constant("K_WORLD_39", SDLK_WORLD_39, 0);
  add_integer_constant("K_WORLD_40", SDLK_WORLD_40, 0);
  add_integer_constant("K_WORLD_41", SDLK_WORLD_41, 0);
  add_integer_constant("K_WORLD_42", SDLK_WORLD_42, 0);
  add_integer_constant("K_WORLD_43", SDLK_WORLD_43, 0);
  add_integer_constant("K_WORLD_44", SDLK_WORLD_44, 0);
  add_integer_constant("K_WORLD_45", SDLK_WORLD_45, 0);
  add_integer_constant("K_WORLD_46", SDLK_WORLD_46, 0);
  add_integer_constant("K_WORLD_47", SDLK_WORLD_47, 0);
  add_integer_constant("K_WORLD_48", SDLK_WORLD_48, 0);
  add_integer_constant("K_WORLD_49", SDLK_WORLD_49, 0);
  add_integer_constant("K_WORLD_50", SDLK_WORLD_50, 0);
  add_integer_constant("K_WORLD_51", SDLK_WORLD_51, 0);
  add_integer_constant("K_WORLD_52", SDLK_WORLD_52, 0);
  add_integer_constant("K_WORLD_53", SDLK_WORLD_53, 0);
  add_integer_constant("K_WORLD_54", SDLK_WORLD_54, 0);
  add_integer_constant("K_WORLD_55", SDLK_WORLD_55, 0);
  add_integer_constant("K_WORLD_56", SDLK_WORLD_56, 0);
  add_integer_constant("K_WORLD_57", SDLK_WORLD_57, 0);
  add_integer_constant("K_WORLD_58", SDLK_WORLD_58, 0);
  add_integer_constant("K_WORLD_59", SDLK_WORLD_59, 0);
  add_integer_constant("K_WORLD_60", SDLK_WORLD_60, 0);
  add_integer_constant("K_WORLD_61", SDLK_WORLD_61, 0);
  add_integer_constant("K_WORLD_62", SDLK_WORLD_62, 0);
  add_integer_constant("K_WORLD_63", SDLK_WORLD_63, 0);
  add_integer_constant("K_WORLD_64", SDLK_WORLD_64, 0);
  add_integer_constant("K_WORLD_65", SDLK_WORLD_65, 0);
  add_integer_constant("K_WORLD_66", SDLK_WORLD_66, 0);
  add_integer_constant("K_WORLD_67", SDLK_WORLD_67, 0);
  add_integer_constant("K_WORLD_68", SDLK_WORLD_68, 0);
  add_integer_constant("K_WORLD_69", SDLK_WORLD_69, 0);
  add_integer_constant("K_WORLD_70", SDLK_WORLD_70, 0);
  add_integer_constant("K_WORLD_71", SDLK_WORLD_71, 0);
  add_integer_constant("K_WORLD_72", SDLK_WORLD_72, 0);
  add_integer_constant("K_WORLD_73", SDLK_WORLD_73, 0);
  add_integer_constant("K_WORLD_74", SDLK_WORLD_74, 0);
  add_integer_constant("K_WORLD_75", SDLK_WORLD_75, 0);
  add_integer_constant("K_WORLD_76", SDLK_WORLD_76, 0);
  add_integer_constant("K_WORLD_77", SDLK_WORLD_77, 0);
  add_integer_constant("K_WORLD_78", SDLK_WORLD_78, 0);
  add_integer_constant("K_WORLD_79", SDLK_WORLD_79, 0);
  add_integer_constant("K_WORLD_80", SDLK_WORLD_80, 0);
  add_integer_constant("K_WORLD_81", SDLK_WORLD_81, 0);
  add_integer_constant("K_WORLD_82", SDLK_WORLD_82, 0);
  add_integer_constant("K_WORLD_83", SDLK_WORLD_83, 0);
  add_integer_constant("K_WORLD_84", SDLK_WORLD_84, 0);
  add_integer_constant("K_WORLD_85", SDLK_WORLD_85, 0);
  add_integer_constant("K_WORLD_86", SDLK_WORLD_86, 0);
  add_integer_constant("K_WORLD_87", SDLK_WORLD_87, 0);
  add_integer_constant("K_WORLD_88", SDLK_WORLD_88, 0);
  add_integer_constant("K_WORLD_89", SDLK_WORLD_89, 0);
  add_integer_constant("K_WORLD_90", SDLK_WORLD_90, 0);
  add_integer_constant("K_WORLD_91", SDLK_WORLD_91, 0);
  add_integer_constant("K_WORLD_92", SDLK_WORLD_92, 0);
  add_integer_constant("K_WORLD_93", SDLK_WORLD_93, 0);
  add_integer_constant("K_WORLD_94", SDLK_WORLD_94, 0);
  add_integer_constant("K_WORLD_95", SDLK_WORLD_95, 0);
  add_integer_constant("K_KP0", SDLK_KP0, 0);
  add_integer_constant("K_KP1", SDLK_KP1, 0);
  add_integer_constant("K_KP2", SDLK_KP2, 0);
  add_integer_constant("K_KP3", SDLK_KP3, 0);
  add_integer_constant("K_KP4", SDLK_KP4, 0);
  add_integer_constant("K_KP5", SDLK_KP5, 0);
  add_integer_constant("K_KP6", SDLK_KP6, 0);
  add_integer_constant("K_KP7", SDLK_KP7, 0);
  add_integer_constant("K_KP8", SDLK_KP8, 0);
  add_integer_constant("K_KP9", SDLK_KP9, 0);
  add_integer_constant("K_KP_PERIOD", SDLK_KP_PERIOD, 0);
  add_integer_constant("K_KP_DIVIDE", SDLK_KP_DIVIDE, 0);
  add_integer_constant("K_KP_MULTIPLY", SDLK_KP_MULTIPLY, 0);
  add_integer_constant("K_KP_MINUS", SDLK_KP_MINUS, 0);
  add_integer_constant("K_KP_PLUS", SDLK_KP_PLUS, 0);
  add_integer_constant("K_KP_ENTER", SDLK_KP_ENTER, 0);
  add_integer_constant("K_KP_EQUALS", SDLK_KP_EQUALS, 0);
  add_integer_constant("K_UP", SDLK_UP, 0);
  add_integer_constant("K_DOWN", SDLK_DOWN, 0);
  add_integer_constant("K_RIGHT", SDLK_RIGHT, 0);
  add_integer_constant("K_LEFT", SDLK_LEFT, 0);
  add_integer_constant("K_INSERT", SDLK_INSERT, 0);
  add_integer_constant("K_HOME", SDLK_HOME, 0);
  add_integer_constant("K_END", SDLK_END, 0);
  add_integer_constant("K_PAGEUP", SDLK_PAGEUP, 0);
  add_integer_constant("K_PAGEDOWN", SDLK_PAGEDOWN, 0);
  add_integer_constant("K_F1", SDLK_F1, 0);
  add_integer_constant("K_F2", SDLK_F2, 0);
  add_integer_constant("K_F3", SDLK_F3, 0);
  add_integer_constant("K_F4", SDLK_F4, 0);
  add_integer_constant("K_F5", SDLK_F5, 0);
  add_integer_constant("K_F6", SDLK_F6, 0);
  add_integer_constant("K_F7", SDLK_F7, 0);
  add_integer_constant("K_F8", SDLK_F8, 0);
  add_integer_constant("K_F9", SDLK_F9, 0);
  add_integer_constant("K_F10", SDLK_F10, 0);
  add_integer_constant("K_F11", SDLK_F11, 0);
  add_integer_constant("K_F12", SDLK_F12, 0);
  add_integer_constant("K_F13", SDLK_F13, 0);
  add_integer_constant("K_F14", SDLK_F14, 0);
  add_integer_constant("K_F15", SDLK_F15, 0);
  add_integer_constant("K_NUMLOCK", SDLK_NUMLOCK, 0);
  add_integer_constant("K_CAPSLOCK", SDLK_CAPSLOCK, 0);
  add_integer_constant("K_SCROLLOCK", SDLK_SCROLLOCK, 0);
  add_integer_constant("K_RSHIFT", SDLK_RSHIFT, 0);
  add_integer_constant("K_LSHIFT", SDLK_LSHIFT, 0);
  add_integer_constant("K_RCTRL", SDLK_RCTRL, 0);
  add_integer_constant("K_LCTRL", SDLK_LCTRL, 0);
  add_integer_constant("K_RALT", SDLK_RALT, 0);
  add_integer_constant("K_LALT", SDLK_LALT, 0);
  add_integer_constant("K_RMETA", SDLK_RMETA, 0);
  add_integer_constant("K_LMETA", SDLK_LMETA, 0);
  add_integer_constant("K_LSUPER", SDLK_LSUPER, 0);
  add_integer_constant("K_RSUPER", SDLK_RSUPER, 0);
  add_integer_constant("K_MODE", SDLK_MODE, 0);
  add_integer_constant("K_COMPOSE", SDLK_COMPOSE, 0);
  add_integer_constant("K_HELP", SDLK_HELP, 0);
  add_integer_constant("K_PRINT", SDLK_PRINT, 0);
  add_integer_constant("K_SYSREQ", SDLK_SYSREQ, 0);
  add_integer_constant("K_BREAK", SDLK_BREAK, 0);
  add_integer_constant("K_MENU", SDLK_MENU, 0);
  add_integer_constant("K_POWER", SDLK_POWER, 0);
  add_integer_constant("K_EURO", SDLK_EURO, 0);
  /*  add_integer_constant("K_UNDO", SDLK_UNDO, 0); */

  /* Key modifers : FIXME - autogenerate */  
  add_integer_constant("KMOD_NONE", KMOD_NONE, 0);
  add_integer_constant("KMOD_LSHIFT", KMOD_LSHIFT, 0);
  add_integer_constant("KMOD_RSHIFT", KMOD_RSHIFT, 0);
  add_integer_constant("KMOD_LCTRL", KMOD_LCTRL, 0);
  add_integer_constant("KMOD_RCTRL", KMOD_RCTRL, 0);
  add_integer_constant("KMOD_LALT", KMOD_LALT, 0);
  add_integer_constant("KMOD_RALT", KMOD_RALT, 0);
  add_integer_constant("KMOD_LMETA", KMOD_LMETA, 0);
  add_integer_constant("KMOD_RMETA", KMOD_RMETA, 0);
  add_integer_constant("KMOD_NUM", KMOD_NUM, 0);
  add_integer_constant("KMOD_CAPS", KMOD_CAPS, 0);
  add_integer_constant("KMOD_MODE", KMOD_MODE, 0);
  add_integer_constant("KMOD_RESERVED", KMOD_RESERVED, 0);
  add_integer_constant("KMOD_CTRL", KMOD_CTRL, 0);
  add_integer_constant("KMOD_SHIFT", KMOD_SHIFT, 0);
  add_integer_constant("KMOD_ALT", KMOD_ALT, 0);
  add_integer_constant("KMOD_META", KMOD_META, 0);

  /* misc constants */
  add_integer_constant("PRESSED", SDL_PRESSED, 0);
  add_integer_constant("RELEASED", SDL_RELEASED, 0);
}
