array funcEV = ({
  "glBegin",
  "glCullFace",
  "glDepthFunc",
  "glDisable",
  "glDisableClientState",
  "glDrawBuffer",
  "glEnable",
  "glEnableClientState",
  "glFrontFace",
  "glLogicOp",
  "glMatrixMode",
  "glReadBuffer",
  "glRenderMode",
  "glShadeModel",
});
array funcV = ({
  "glEnd",
  "glEndList",
  "glFinish",
  "glFlush",
  "glInitNames",
  "glLoadIdentity",
  "glPopAttrib",
  "glPopClientAttrib",
  "glPopMatrix",
  "glPopName",
  "glPushMatrix",
});
array func_misc = ({
  ({"glAccum", "VEF"}),
  ({"glAlphaFunc", "VEF"}),
  ({"glArrayElement", "VI"}),
  ({"glBindTexture","VEI"}),
  ({"glBlendFunc", "VEE"}),
  ({"glCallList","VI"}),
  ({"glClear","VB"}),
  ({"glClearAccum", "V+FFFF"}),
  ({"glClearColor", "V+FFFF"}),
  ({"glClearDepth", "VD"}),
  ({"glClearIndex", "VF"}),
  ({"glClearStencil", "VI"}),
  ({"glClipPlane", "VE=DDDD"}),
  ({"glColor", "V+ZZZZ"}),
  ({"glColorMask", "VOOOO"}),
  ({"glColorMaterial", "VEE"}),
  ({"glCopyPixels", "VIIIIE"}),
  ({"glCopyTexImage1D", "VEIEIIII"}),
  ({"glCopyTexImage2D", "VEIEIIIII"}),
  ({"glCopyTexSubImage1D", "VEIIIII"}),
  ({"glCopyTexSubImage2D", "VEIIIIIII"}),
  ({"glDeleteLists", "VII"}),
  ({"glDepthMask", "VO"}),
  ({"glDepthRange", "VDD"}),
  ({"glDrawArrays", "VEII"}),
  ({"glDrawPixels", "Vwhfti"}),
  ({"glEdgeFlag", "VO"}),
  ({"glEvalCoord", "V+RR"}),
  ({"glEvalPoint", "V+II"}),
  ({"glFog","VE@Q"}),

  // Replaced by my_glFrustum since glFrustum is
  // broken on some SUN implementations.
  //  ({"glFrustum", "VDDDDDD"}),

  ({"glGenLists", "II"}),
  ({"glGetError", "E"}),
  ({"glGetString", "SE"}),
  ({"glGetTexImage", "VEIII&"}),
  ({"glReadPixels", "VIIIIEE&"}),
  ({"glSelectBuffer","VI&" }),
  ({"glFeedbackBuffer","VIE&" }),
  ({"glVertexPointer","VIEI&" }),
  ({"glInterleavedArrays", "VEI&" }),
  ({"glTexCoordPointer","VIEI&" }),
  ({"glIndexPointer","VEI&" }),
  ({"glNormalPointer","VEI&" }),
  ({"glColorPointer","VIEI&" }),
  ({"glEdgeFlagPointer","VI&" }),
  ({"glHint", "VEE"}),
  ({"glIndex", "VZ"}),
  ({"glIndexMask", "VI"}),
  ({"glIsEnabled", "OE"}),
  ({"glIsList", "OI"}),
  ({"glIsTexture", "OI"}),
  ({"glLight", "VEE@Q"}),
  ({"glLightModel", "VE@Q"}),
  ({"glLineStipple", "VII"}),
  ({"glLineWidth", "VF"}),
  ({"glListBase", "VI"}),
  ({"glLoadMatrix", "V[16R"}),
  ({"glLoadName", "VI"}),
  ({"glMaterial", "VEE@Q"}),
  ({"glMultMatrix", "V[16R"}),
  ({"glNewList", "VIE"}),
  ({"glNormal", "V#ZZZ"}),
  ({"glOrtho", "VDDDDDD"}),
  ({"glPassThrough", "VF"}),
  ({"glPixelZoom", "VFF"}),
  ({"glPointSize", "VF"}),
  ({"glPolygonMode", "VEE"}),
  ({"glPolygonOffset", "VFF"}),
  ({"glPushAttrib", "VB"}),
  ({"glPushClientAttrib", "VB"}),
  ({"glPushName", "VI"}),
  ({"glRasterPos", "V+ZZZ"}),
  ({"glRotate", "V!RRRR"}),
  ({"glScale", "V!RRR"}),
  ({"glScissor", "VIIII"}),
  ({"glStencilFunc", "VEII"}),
  ({"glStencilMask", "VI"}),
  ({"glStencilOp", "VEEE"}),
  ({"glTexCoord", "V+Z"}),
  ({"glTexEnv","VEE@Q"}),
  ({"glTexGen","VEE@Z"}),
  ({"glEvalMesh1", "VEII" }),
  ({"glEvalMesh2", "VEIIII" }),
  ({"glTexImage2D","VEIIwhIfti"}),
  ({"glTexImage1D","VEIIwIfti"}),
  ({"glTexParameter","VEE@Q"}),
  ({"glTexSubImage2D","VEIIIwhfti"}),
  ({"glTexSubImage1D","VEIIwfti"}),
  ({"glTranslate", "V!RRR"}),
  ({"glVertex","V+ZZZ"}),
  ({"glViewport", "VIIII"}),
});
mapping func_cat = ([
  "VE":funcEV,
  "V":funcV,
]);

/*
  Not implemented:

  glAreTexturesResident
  glBitmap
  glDrawElements
  glEvalMesh
  glFeedbackBuffer
  glGetClipPlane
  glGetLight
  glGetMap
  glGetMaterial
  glGetPixelMap
  glGetPointer
  glGetPolygonStipple
  glGetTexEnv
  glGetTexGen
  glGetTexLevelParameter
  glGetTexParameter
  glPixelMap
  glPixelTransfer
  glMapGrid
  glMap1
  glMap2
  glPolygonStipple
  glPrioritizeTextures
  glRect
  glVertexPoint

*/
