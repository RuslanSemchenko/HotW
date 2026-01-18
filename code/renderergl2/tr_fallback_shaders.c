/*
===========================================================================

  Minimal fallback GLSL shader strings for the OpenGL2 renderer.

  In the original Makefile-based build these are auto-generated from the
  .glsl files into C string constants named:
      fallbackShader_<shadername>_[vp|fp]

  The CMake build previously didn't generate or provide them, which caused
  unresolved externals when linking renderer_opengl2.dll.  This file
  provides simple (safe) stub definitions so the renderer links correctly.

  If a real GLSL shader fails to compile, these fallbacks will be used.
  For now they are empty shaders; they should only be hit in error paths.

===========================================================================
*/

const char *fallbackShader_bokeh_vp         = "";
const char *fallbackShader_bokeh_fp         = "";
const char *fallbackShader_calclevels4x_vp  = "";
const char *fallbackShader_calclevels4x_fp  = "";
const char *fallbackShader_depthblur_vp     = "";
const char *fallbackShader_depthblur_fp     = "";
const char *fallbackShader_dlight_vp        = "";
const char *fallbackShader_dlight_fp        = "";
const char *fallbackShader_down4x_vp        = "";
const char *fallbackShader_down4x_fp        = "";
const char *fallbackShader_fogpass_vp       = "";
const char *fallbackShader_fogpass_fp       = "";
const char *fallbackShader_generic_vp       = "";
const char *fallbackShader_generic_fp       = "";
const char *fallbackShader_lightall_vp      = "";
const char *fallbackShader_lightall_fp      = "";
const char *fallbackShader_pshadow_vp       = "";
const char *fallbackShader_pshadow_fp       = "";
const char *fallbackShader_shadowfill_vp    = "";
const char *fallbackShader_shadowfill_fp    = "";
const char *fallbackShader_shadowmask_vp    = "";
const char *fallbackShader_shadowmask_fp    = "";
const char *fallbackShader_ssao_vp          = "";
const char *fallbackShader_ssao_fp          = "";
const char *fallbackShader_texturecolor_vp  = "";
const char *fallbackShader_texturecolor_fp  = "";
const char *fallbackShader_tonemap_vp       = "";
const char *fallbackShader_tonemap_fp       = "";


