
#ifndef TR_COMMON_H
#define TR_COMMON_H

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_public.h"
#include "qgl.h"

typedef enum
{
	IMGTYPE_COLORALPHA, // for color, lightmap, diffuse, and specular
	IMGTYPE_NORMAL,
	IMGTYPE_NORMALHEIGHT,
	IMGTYPE_DELUXE, // normals are swizzled, deluxe are not
} imgType_t;

typedef enum
{
	IMGFLAG_NONE = 0x0000,
	IMGFLAG_MIPMAP = 0x0001,
	IMGFLAG_PICMIP = 0x0002,
	IMGFLAG_CUBEMAP = 0x0004,
	IMGFLAG_NO_COMPRESSION = 0x0010,
	IMGFLAG_NOLIGHTSCALE = 0x0020,
	IMGFLAG_CLAMPTOEDGE = 0x0040,
	IMGFLAG_SRGB = 0x0080,
	IMGFLAG_GENNORMALMAP = 0x0100,
} imgFlags_t;

typedef struct image_s {
	char		imgName[MAX_QPATH];		// game path, including extension
	int			width, height;				// source image
	int			uploadWidth, uploadHeight;	// after power of two and picmip but not including clamp to MAX_TEXTURE_SIZE
	GLuint		texnum;					// gl texture binding

	int			frameUsed;			// for texture usage in frame statistics

	int			internalFormat;
	int			TMU;				// only needed for voodoo2

	imgType_t   type;
	imgFlags_t  flags;

	struct image_s* next;
} image_t;

// any change in the LIGHTMAP_* defines here MUST be reflected in
// R_FindShader() in tr_bsp.c
#define LIGHTMAP_2D         -4	// shader is for 2D rendering
#define LIGHTMAP_BY_VERTEX  -3	// pre-lit triangle models
#define LIGHTMAP_WHITEIMAGE -2
#define LIGHTMAP_NONE       -1

extern	refimport_t		ri;
extern glconfig_t	glConfig;		// outside of TR since it shouldn't be cleared during ref re-init

// These variables should live inside glConfig but can't because of
// compatibility issues to the original ID vms.  If you release a stand-alone
// game and your mod uses tr_types.h from this build you can safely move them
// to the glconfig_t struct.
extern qboolean  textureFilterAnisotropic;
extern int       maxAnisotropy;
extern float     displayAspect;

#ifdef FRAMEBUFFER_AND_GLSL_SUPPORT
// Needed in sdl_glimp.c
extern qboolean framebufferSupported;
extern qboolean glslSupported;
extern qboolean multisampleSupported;
extern cvar_t* r_ext_framebuffer;
#endif

//
// cvars
//
extern cvar_t* r_colorbits;			// number of desired color bits, only relevant for fullscreen
extern cvar_t* r_depthbits;			// number of desired depth bits
extern cvar_t* r_stencilbits;			// number of desired stencil bits
extern cvar_t* r_stereoEnabled;
extern cvar_t* r_swapInterval;

extern cvar_t* r_ext_multisample;
extern cvar_t* r_ext_compressed_textures;
extern cvar_t* r_ext_texture_env_add;
extern cvar_t* r_ext_multitexture;
extern cvar_t* r_ext_compiled_vertex_array;
extern cvar_t* r_ext_texture_filter_anisotropic;

extern cvar_t* r_drawBuffer;

extern cvar_t* r_stencilbits;			// number of desired stencil bits
extern cvar_t* r_depthbits;			// number of desired depth bits
extern cvar_t* r_colorbits;			// number of desired color bits, only relevant for fullscreen
extern cvar_t* r_texturebits;			// number of desired texture bits
extern cvar_t* r_ext_multisample;
// 0 = use framebuffer depth
// 16 = use 16-bit textures
// 32 = use 32-bit textures
// all else = error

extern cvar_t* r_mode;				// video mode
extern cvar_t* r_noborder;
extern cvar_t* r_fullscreen;
extern cvar_t* r_ignorehwgamma;		// overrides hardware gamma capabilities
extern cvar_t* r_drawBuffer;
extern cvar_t* r_swapInterval;

extern cvar_t* r_allowExtensions;				// global enable/disable of OpenGL extensions
extern cvar_t* r_ext_compressed_textures;		// these control use of specific extensions
extern cvar_t* r_ext_multitexture;
extern cvar_t* r_ext_compiled_vertex_array;
extern cvar_t* r_ext_texture_env_add;

extern cvar_t* r_ext_texture_filter_anisotropic;
extern cvar_t* r_ext_max_anisotropy;

extern cvar_t* r_stereoEnabled;

extern	cvar_t* r_saveFontData;

qboolean	R_GetModeInfo(int* width, int* height, float* windowAspect, int mode);

float R_NoiseGet4f(float x, float y, float z, float t);
void  R_NoiseInit(void);

image_t* R_FindImageFile(const char* name, imgType_t type, imgFlags_t flags);
image_t* R_CreateImage(const char* name, byte* pic, int width, int height, imgType_t type, imgFlags_t flags, int internalFormat);

void R_IssuePendingRenderCommands(void);
qhandle_t		 RE_RegisterShaderLightMap(const char* name, int lightmapIndex);
qhandle_t		 RE_RegisterShader(const char* name);
qhandle_t		 RE_RegisterShaderNoMip(const char* name);
qhandle_t RE_RegisterShaderFromImage(const char* name, int lightmapIndex, image_t* image, qboolean mipRawImage);

// font stuff
void R_InitFreeType(void);
void R_DoneFreeType(void);
void RE_RegisterFont(const char* fontName, int pointSize, fontInfo_t* font);

/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/

void		GLimp_Init(void);
void		GLimp_Shutdown(void);
void		GLimp_EndFrame(void);

void		GLimp_LogComment(char* comment);
void		GLimp_Minimize(void);

void		GLimp_SetGamma(unsigned char red[256],
	unsigned char green[256],
	unsigned char blue[256]);

//
// glimp_ext.c
//
extern	void (APIENTRYP qglMultiTexCoord2fARB)(GLenum target, GLfloat s, GLfloat t);
extern	void (APIENTRYP qglActiveTextureARB)(GLenum texture);
extern	void (APIENTRYP qglClientActiveTextureARB)(GLenum texture);

extern	void (APIENTRYP qglLockArraysEXT)(int, int);
extern	void (APIENTRYP qglUnlockArraysEXT)(void);

#ifdef FRAMEBUFFER_AND_GLSL_SUPPORT
//added framebuffer extensions
extern void (APIENTRYP qglGenFramebuffersEXT)(GLsizei, GLuint*);
extern void (APIENTRYP qglBindFramebufferEXT)(GLenum, GLuint);
extern void (APIENTRYP qglGenRenderbuffersEXT)(GLsizei, GLuint*);
extern void (APIENTRYP qglBindRenderbufferEXT)(GLenum, GLuint);
extern void (APIENTRYP qglRenderbufferStorageEXT)(GLenum, GLenum, GLsizei, GLsizei);
extern void (APIENTRYP qglRenderbufferStorageMultisampleEXT)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
extern void (APIENTRYP qglFramebufferRenderbufferEXT)(GLenum, GLenum, GLenum, GLuint);
extern void (APIENTRYP qglFramebufferTexture2DEXT)(GLenum, GLenum, GLenum, GLuint, GLint);
extern GLenum(APIENTRYP qglCheckFramebufferStatusEXT)(GLenum);
extern void (APIENTRYP qglDeleteFramebuffersEXT)(GLsizei, const GLuint*);
extern void (APIENTRYP qglDeleteRenderbuffersEXT)(GLsizei, const GLuint*);
extern void (APIENTRYP qglBlitFramebufferEXT)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLenum, GLbitfield);

//added fragment/vertex program extensions
extern void (APIENTRYP qglAttachShader) (GLuint, GLuint);
extern void (APIENTRYP qglBindAttribLocation) (GLuint, GLuint, const GLchar*);
extern void (APIENTRYP qglCompileShader) (GLuint);
extern GLuint(APIENTRYP qglCreateProgram) (void);
extern GLuint(APIENTRYP qglCreateShader) (GLenum);
extern void (APIENTRYP qglDeleteProgram) (GLuint);
extern void (APIENTRYP qglDeleteShader) (GLuint);
extern void (APIENTRYP qglShaderSource) (GLuint, GLsizei, const GLchar**, const GLint*);
extern void (APIENTRYP qglLinkProgram) (GLuint);
extern void (APIENTRYP qglUseProgram) (GLuint);
extern GLint(APIENTRYP qglGetUniformLocation) (GLuint, const GLchar*);
extern void (APIENTRYP qglUniform1f) (GLint, GLfloat);
extern void (APIENTRYP qglUniform2f) (GLint, GLfloat, GLfloat);
extern void (APIENTRYP qglUniform1i) (GLint, GLint);
extern void (APIENTRYP qglGetProgramiv) (GLuint, GLenum, GLint*);
extern void (APIENTRYP qglGetProgramInfoLog) (GLuint, GLsizei, GLsizei*, GLchar*);
extern void (APIENTRYP qglGetShaderiv) (GLuint, GLenum, GLint*);
extern void (APIENTRYP qglGetShaderInfoLog) (GLuint, GLsizei, GLsizei*, GLchar*);
#endif

#endif
