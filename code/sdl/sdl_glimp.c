#ifdef USE_LOCAL_HEADERS
#	include "SDL.h"
#else
#ifndef APIENTRY
#define APIENTRY __cdecl
#endif
#ifndef WINGDIAPI
#define WINGDIAPI __declspec(dllimport)
#endif
#ifndef CALLBACK
#define CALLBACK __stdcall
#endif
#ifndef GLAPIENTRY
#define GLAPIENTRY APIENTRY
#endif

#	include <SDL3/SDL.h>
#   include <SDL3/SDL_video.h>
#   include <SDL3/SDL_opengl.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#include "../renderercommon/tr_common.h"
#include "../sys/sys_local.h"
#include "sdl_icon.h"

/* Just hack it for now. */
#ifdef MACOS_X
#include <OpenGL/OpenGL.h>
typedef CGLContextObj QGLContext;
#define GLimp_GetCurrentContext() CGLGetCurrentContext()
#define GLimp_SetCurrentContext(ctx) CGLSetCurrentContext(ctx)
#else
typedef void* QGLContext;
#define GLimp_GetCurrentContext() (NULL)
#define GLimp_SetCurrentContext(ctx)
#endif

static QGLContext opengl_context;

typedef enum
{
	RSERR_OK,

	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,

	RSERR_UNKNOWN
} rserr_t;

// Changed from SDL_Surface to SDL_Window
static SDL_Window* sdl_window = NULL;

// Removed redundant and conflicting extern declarations. They are defined in tr_common.h
extern glconfig_t glConfig;
extern cvar_t* r_fullscreen;
extern cvar_t* r_mode;
extern cvar_t* r_noborder;
// Removed extern cvars, will use ri.Cvar_Get instead
extern qboolean framebufferSupported;
extern qboolean glslSupported;
extern qboolean multisampleSupported;

// Forward declaration to fix implicit declaration warnings and linker errors
static qboolean GLimp_HaveExtension(const char* ext);

// Definitions for the function pointers declared as extern in tr_common.h
void (APIENTRY* qglMultiTexCoord2fARB)(GLenum target, GLfloat s, GLfloat t);
void (APIENTRY* qglActiveTextureARB)(GLenum texture);
void (APIENTRY* qglClientActiveTextureARB)(GLenum texture);

void (APIENTRY* qglLockArraysEXT)(int, int);
void (APIENTRY* qglUnlockArraysEXT)(void);

#ifdef FRAMEBUFFER_AND_GLSL_SUPPORT
//added framebuffer extensions
void (APIENTRY* qglGenFramebuffersEXT)(GLsizei, GLuint*);
void (APIENTRY* qglBindFramebufferEXT)(GLenum, GLuint);
void (APIENTRY* qglGenRenderbuffersEXT)(GLsizei, GLuint*);
void (APIENTRY* qglBindRenderbufferEXT)(GLenum, GLuint);
void (APIENTRY* qglRenderbufferStorageEXT)(GLenum, GLenum, GLsizei, GLsizei);
void (APIENTRY* qglRenderbufferStorageMultisampleEXT)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
void (APIENTRY* qglFramebufferRenderbufferEXT)(GLenum, GLenum, GLenum, GLuint);
void (APIENTRY* qglFramebufferTexture2DEXT)(GLenum, GLenum, GLenum, GLuint, GLint);
GLenum(APIENTRY* qglCheckFramebufferStatusEXT)(GLenum);
void (APIENTRY* qglDeleteFramebuffersEXT)(GLsizei, const GLuint*);
void (APIENTRY* qglDeleteRenderbuffersEXT)(GLsizei, const GLuint*);
void (APIENTRY* qglBlitFramebufferEXT)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLenum, GLbitfield);

//added fragment/vertex program extensions
void (APIENTRY* qglAttachShader) (GLuint, GLuint);
void (APIENTRY* qglBindAttribLocation) (GLuint, GLuint, const GLchar*);
void (APIENTRY* qglCompileShader) (GLuint);
GLuint(APIENTRY* qglCreateProgram) (void);
GLuint(APIENTRY* qglCreateShader) (GLenum);
void (APIENTRY* qglDeleteProgram) (GLuint);
void (APIENTRY* qglDeleteShader) (GLuint);
void (APIENTRY* qglShaderSource) (GLuint, GLsizei, const GLchar**, const GLint*);
void (APIENTRY* qglLinkProgram) (GLuint);
void (APIENTRY* qglUseProgram) (GLuint);
GLint(APIENTRY* qglGetUniformLocation) (GLuint, const GLchar*);
void (APIENTRY* qglUniform1f) (GLint, GLfloat);
void (APIENTRY* qglUniform2f) (GLint, GLfloat, GLfloat);
void (APIENTRY* qglUniform1i) (GLint, GLint);
void (APIENTRY* qglGetProgramiv) (GLuint, GLenum, GLint*);
void (APIENTRY* qglGetProgramInfoLog) (GLuint, GLsizei, GLsizei*, GLchar*);
void (APIENTRY* qglGetShaderiv) (GLuint, GLenum, GLint*);
void (APIENTRY* qglGetShaderInfoLog) (GLuint, GLsizei, GLsizei*, GLchar*);
#endif

/*
===============
GLimp_Shutdown
===============
*/
void GLimp_Shutdown(void)
{
	ri.IN_Shutdown();

	// Changed to use SDL2/3 API
	if (sdl_window) {
		SDL_DestroyWindow(sdl_window);
		sdl_window = NULL;
	}

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

/*
===============
GLimp_Minimize
===============
*/
void GLimp_Minimize(void)
{
	if (sdl_window) {
		SDL_MinimizeWindow(sdl_window);
	}
}


/*
===============
GLimp_LogComment
===============
*/
void GLimp_LogComment(char* comment)
{
}

/*
===============
GLimp_CompareModes
===============
*/
static int GLimp_CompareModes(const void* a, const void* b)
{
	const float ASPECT_EPSILON = 0.001f;
	// SDL3: SDL_DisplayMode pointers are passed here (array of pointers)
	const SDL_DisplayMode* modeA = *(const SDL_DisplayMode**)a;
	const SDL_DisplayMode* modeB = *(const SDL_DisplayMode**)b;
	float aspectA = (float)modeA->w / (float)modeA->h;
	float aspectB = (float)modeB->w / (float)modeB->h;
	int areaA = modeA->w * modeA->h;
	int areaB = modeB->w * modeB->h;
	float aspectDiffA = fabs(aspectA - displayAspect);
	float aspectDiffB = fabs(aspectB - displayAspect);
	float aspectDiffsDiff = aspectDiffA - aspectDiffB;

	if (aspectDiffsDiff > ASPECT_EPSILON)
		return 1;
	else if (aspectDiffsDiff < -ASPECT_EPSILON)
		return -1;
	else
		return areaA - areaB;
}

/*
===============
GLimp_DetectAvailableModes
===============
*/
static void GLimp_DetectAvailableModes(void)
{
	char buf[MAX_STRING_CHARS] = { 0 };
	int i, numDisplays, numModes;
	SDL_DisplayID* displays;
	const SDL_DisplayMode** modes;
	const SDL_DisplayMode* currentMode;
	SDL_DisplayID primaryDisplay;

	ri.Printf(PRINT_ALL, "Detecting available modes...\n");

	displays = SDL_GetDisplays(&numDisplays);
	if (numDisplays < 1 || !displays) {
		ri.Printf(PRINT_WARNING, "Can't get list of available modes, no displays found.\n");
		if (displays) SDL_free(displays);
		return;
	}
	primaryDisplay = displays[0];
	SDL_free(displays); // Free the list of displays

	currentMode = SDL_GetDesktopDisplayMode(primaryDisplay);
	if (!currentMode) {
		ri.Printf(PRINT_WARNING, "SDL_GetDesktopDisplayMode failed: %s\n", SDL_GetError());
		displayAspect = 4.0f / 3.0f; // Fallback
	}
	else {
		displayAspect = (float)currentMode->w / (float)currentMode->h;
		ri.Printf(PRINT_ALL, "Estimated display aspect: %.3f\n", displayAspect);
	}

	// SDL3: GetFullscreenDisplayModes returns an array of POINTERS to modes.
	modes = SDL_GetFullscreenDisplayModes(primaryDisplay, &numModes);
	if (numModes < 1 || !modes) {
		ri.Printf(PRINT_WARNING, "Can't get list of available modes\n");
		if (modes) SDL_free((void*)modes);
		return;
	}

	// Sort the list of pointers
	if (numModes > 1) {
		qsort(modes, numModes, sizeof(SDL_DisplayMode*), GLimp_CompareModes);
	}

	for (i = 0; i < numModes; i++)
	{
		const char* newModeString = va("%dx%d ", modes[i]->w, modes[i]->h);

		if (strlen(newModeString) < (int)sizeof(buf) - strlen(buf))
			Q_strcat(buf, sizeof(buf), newModeString);
		else
			ri.Printf(PRINT_WARNING, "Skipping mode %dx%d, buffer too small\n", modes[i]->w, modes[i]->h);
	}

	// SDL3 requires freeing the array returned by GetFullscreenDisplayModes
	SDL_free((void*)modes);

	if (*buf)
	{
		buf[strlen(buf) - 1] = 0;
		ri.Printf(PRINT_ALL, "Available modes: '%s'\n", buf);
		ri.Cvar_Set("r_availableModes", buf);
	}
}

/*
===============
GLimp_SetMode
===============
*/
static int GLimp_SetMode(int mode, qboolean fullscreen, qboolean noborder)
{
	const char* glstring;
	int sdlcolorbits;
	int colorbits, depthbits, stencilbits;
	int tcolorbits, tdepthbits, tstencilbits;
	int samples;
	int i = 0;
	Uint32 flags = SDL_WINDOW_OPENGL;
	SDL_DisplayID* displays;
	int numDisplays = 0;
	const SDL_DisplayMode* displayMode = NULL;

	ri.Printf(PRINT_ALL, "Initializing OpenGL display\n");

	if (ri.Cvar_Get("r_allowResize", "0", CVAR_ARCHIVE | CVAR_LATCH)->integer)
		flags |= SDL_WINDOW_RESIZABLE;

	if (fullscreen)
	{
		flags |= SDL_WINDOW_FULLSCREEN;
		glConfig.isFullscreen = qtrue;
	}
	else
	{
		if (noborder)
			flags |= SDL_WINDOW_BORDERLESS;

		glConfig.isFullscreen = qfalse;
	}

	// Safely get display info
	displays = SDL_GetDisplays(&numDisplays);
	if (displays && numDisplays > 0) {
		displayMode = SDL_GetDesktopDisplayMode(displays[0]);
		SDL_free(displays);
	}
	// If displays is null or numDisplays is 0, displayMode remains NULL

	if (!displayMode) {
		// Log error but continue with defaults
		ri.Printf(PRINT_WARNING, "SDL_GetDesktopDisplayMode/GetDisplays failed: %s\n", SDL_GetError());
		displayAspect = 4.0f / 3.0f;

		// Fallback resolution just for calculations, 
		// window creation will attempt default or user requested
		if (mode == -2) {
			glConfig.vidWidth = 640;
			glConfig.vidHeight = 480;
		}
	}
	else {
		displayAspect = (float)displayMode->w / (float)displayMode->h;
		ri.Printf(PRINT_ALL, "Estimated display aspect: %.3f\n", displayAspect);
	}


	ri.Printf(PRINT_ALL, "...setting mode %d:", mode);

	if (mode == -2)
	{
		// use desktop video resolution
		if (displayMode) {
			glConfig.vidWidth = displayMode->w;
			glConfig.vidHeight = displayMode->h;
		}
		// If displayMode was null, glConfig was already set to 640x480 above
		glConfig.windowAspect = (float)glConfig.vidWidth / (float)glConfig.vidHeight;
	}
	else if (!R_GetModeInfo(&glConfig.vidWidth, &glConfig.vidHeight, &glConfig.windowAspect, mode))
	{
		ri.Printf(PRINT_ALL, " invalid mode\n");
		return RSERR_INVALID_MODE;
	}
	ri.Printf(PRINT_ALL, " %d %d\n", glConfig.vidWidth, glConfig.vidHeight);

	colorbits = r_colorbits->value;
	if ((!colorbits) || (colorbits >= 32))
		colorbits = 24;

	if (!r_depthbits->value)
		depthbits = 24;
	else
		depthbits = r_depthbits->value;
	stencilbits = r_stencilbits->value;
#if !defined(HotW) || !defined(FRAMEBUFFER_AND_GLSL_SUPPORT)
	samples = r_ext_multisample->value;
#else
	// Tequila: Currently, multisample is not supported when framebuffer is used
	samples = r_ext_framebuffer->integer != 1 ? r_ext_multisample->value : 0;
#endif

	for (i = 0; i < 16; i++)
	{
		// 0 - default
		// 1 - minus colorbits
		// 2 - minus depthbits
		// 3 - minus stencil
		if ((i % 4) == 0 && i)
		{
			// one pass, reduce
			switch (i / 4)
			{
			case 2:
				if (colorbits == 24)
					colorbits = 16;
				break;
			case 1:
				if (depthbits == 24)
					depthbits = 16;
				else if (depthbits == 16)
					depthbits = 8;
				break;
			case 3:
				if (stencilbits == 24)
					stencilbits = 16;
				else if (stencilbits == 16)
					stencilbits = 8;
				break;
			}
		}

		tcolorbits = colorbits;
		tdepthbits = depthbits;
		tstencilbits = stencilbits;

		if ((i % 4) == 3)
		{ // reduce colorbits
			if (tcolorbits == 24)
				tcolorbits = 16;
		}

		if ((i % 4) == 2)
		{ // reduce depthbits
			if (tdepthbits == 24)
				tdepthbits = 16;
			else if (tdepthbits == 16)
				tdepthbits = 8;
		}

		if ((i % 4) == 1)
		{ // reduce stencilbits
			if (tstencilbits == 24)
				tstencilbits = 16;
			else if (tstencilbits == 16)
				tstencilbits = 8;
			else
				tstencilbits = 0;
		}

		sdlcolorbits = 4;
		if (tcolorbits == 24)
			sdlcolorbits = 8;

		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, sdlcolorbits);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, sdlcolorbits);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, sdlcolorbits);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, tdepthbits);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, tstencilbits);

		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, samples ? 1 : 0);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, samples);

		if (r_stereoEnabled->integer)
		{
			glConfig.stereoEnabled = qtrue;
			SDL_GL_SetAttribute(SDL_GL_STEREO, 1);
		}
		else
		{
			glConfig.stereoEnabled = qfalse;
			SDL_GL_SetAttribute(SDL_GL_STEREO, 0);
		}

		// Request a compatibility profile to allow legacy functions
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

		// Create the window
		// SDL3: SDL_CreateWindow takes (title, w, h, flags).
		sdl_window = SDL_CreateWindow(CLIENT_WINDOW_TITLE,
			glConfig.vidWidth,
			glConfig.vidHeight,
			flags);

		if (!sdl_window)
		{
			ri.Printf(PRINT_DEVELOPER, "SDL_CreateWindow failed: %s\n", SDL_GetError());
			continue;
		}

		// Handle centering
		if (ri.Cvar_Get("r_centerWindow", "0", CVAR_ARCHIVE | CVAR_LATCH)->integer) {
			SDL_SetWindowPosition(sdl_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
		}

		// Create OpenGL context
		opengl_context = SDL_GL_CreateContext(sdl_window);
		if (!opengl_context)
		{
			ri.Printf(PRINT_DEVELOPER, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
			SDL_DestroyWindow(sdl_window);
			sdl_window = NULL;
			continue;
		}

		SDL_SetWindowTitle(sdl_window, CLIENT_WINDOW_TITLE);
		SDL_HideCursor();

		ri.Printf(PRINT_ALL, "Using %d/%d/%d Color bits, %d depth, %d stencil display.\n",
			sdlcolorbits, sdlcolorbits, sdlcolorbits, tdepthbits, tstencilbits);

		glConfig.colorBits = tcolorbits;
		glConfig.depthBits = tdepthbits;
		glConfig.stencilBits = tstencilbits;
		break;
	}

	GLimp_DetectAvailableModes();

	if (!sdl_window)
	{
		ri.Printf(PRINT_ALL, "Couldn't create a window\n");
		return RSERR_INVALID_MODE;
	}

	glstring = (char*)qglGetString(GL_RENDERER);
	ri.Printf(PRINT_ALL, "GL_RENDERER: %s\n", glstring);

	return RSERR_OK;
}

/*
===============
GLimp_StartDriverAndSetMode
===============
*/
static qboolean GLimp_StartDriverAndSetMode(int mode, qboolean fullscreen, qboolean noborder)
{
	rserr_t err;

	// FIX: Use InitSubSystem to be thread-safe/ref-counted and force video init
	// irrespective of previous state, to ensure driver is loaded.
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) == -1)
	{
		ri.Printf(PRINT_ALL, "SDL_InitSubSystem( SDL_INIT_VIDEO ) FAILED (%s)\n",
			SDL_GetError());
		return qfalse;
	}

	// We can check if it's already initialized, but InitSubSystem is safe.
	{
		const char* driverNamePtr;
		char driverName[64];

		driverNamePtr = SDL_GetCurrentVideoDriver();
		if (driverNamePtr) {
			Q_strncpyz(driverName, driverNamePtr, sizeof(driverName));
		}
		else {
			Q_strncpyz(driverName, "unknown", sizeof(driverName));
		}

		ri.Printf(PRINT_ALL, "SDL using driver \"%s\"\n", driverName);
		ri.Cvar_Set("r_sdlDriver", driverName);
	}

	if (fullscreen && ri.Cvar_VariableIntegerValue("in_nograb"))
	{
		ri.Printf(PRINT_ALL, "Fullscreen not allowed with in_nograb 1\n");
		ri.Cvar_Set("r_fullscreen", "0");
		r_fullscreen->modified = qfalse;
		fullscreen = qfalse;
	}

	err = GLimp_SetMode(mode, fullscreen, noborder);

	switch (err)
	{
	case RSERR_INVALID_FULLSCREEN:
		ri.Printf(PRINT_ALL, "...WARNING: fullscreen unavailable in this mode\n");
		return qfalse;
	case RSERR_INVALID_MODE:
		ri.Printf(PRINT_ALL, "...WARNING: could not set the given mode (%d)\n", mode);
		return qfalse;
	default:
		break;
	}

	return qtrue;
}

static qboolean GLimp_HaveExtension(const char* ext)
{
	const char* ptr = Q_stristr(glConfig.extensions_string, ext);
	if (ptr == NULL)
		return qfalse;
	ptr += strlen(ext);
	return ((*ptr == ' ') || (*ptr == '\0'));  // verify it's complete string.
}


/*
===============
GLimp_InitExtensions
===============
*/
static void GLimp_InitExtensions(void)
{
	if (!r_allowExtensions->integer)
	{
		ri.Printf(PRINT_ALL, "* IGNORING OPENGL EXTENSIONS *\n");
		return;
	}

	ri.Printf(PRINT_ALL, "Initializing OpenGL extensions\n");

	glConfig.textureCompression = TC_NONE;

	// GL_EXT_texture_compression_s3tc
	if (GLimp_HaveExtension("GL_ARB_texture_compression") &&
		GLimp_HaveExtension("GL_EXT_texture_compression_s3tc"))
	{
		if (r_ext_compressed_textures->value)
		{
			glConfig.textureCompression = TC_S3TC_ARB;
			ri.Printf(PRINT_ALL, "...using GL_EXT_texture_compression_s3tc\n");
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_EXT_texture_compression_s3tc\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_EXT_texture_compression_s3tc not found\n");
	}

	// GL_S3_s3tc ... legacy extension before GL_EXT_texture_compression_s3tc.
	if (glConfig.textureCompression == TC_NONE)
	{
		if (GLimp_HaveExtension("GL_S3_s3tc"))
		{
			if (r_ext_compressed_textures->value)
			{
				glConfig.textureCompression = TC_S3TC;
				ri.Printf(PRINT_ALL, "...using GL_S3_s3tc\n");
			}
			else
			{
				ri.Printf(PRINT_ALL, "...ignoring GL_S3_s3tc\n");
			}
		}
		else
		{
			ri.Printf(PRINT_ALL, "...GL_S3_s3tc not found\n");
		}
	}


	// GL_EXT_texture_env_add
	glConfig.textureEnvAddAvailable = qfalse;
	if (GLimp_HaveExtension("EXT_texture_env_add"))
	{
		if (r_ext_texture_env_add->integer)
		{
			glConfig.textureEnvAddAvailable = qtrue;
			ri.Printf(PRINT_ALL, "...using GL_EXT_texture_env_add\n");
		}
		else
		{
			glConfig.textureEnvAddAvailable = qfalse;
			ri.Printf(PRINT_ALL, "...ignoring GL_EXT_texture_env_add\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_EXT_texture_env_add not found\n");
	}

	// GL_ARB_multitexture
	qglMultiTexCoord2fARB = NULL;
	qglActiveTextureARB = NULL;
	qglClientActiveTextureARB = NULL;
	if (GLimp_HaveExtension("GL_ARB_multitexture"))
	{
		if (r_ext_multitexture->integer)
		{
			qglMultiTexCoord2fARB = (void (APIENTRY*)(GLenum, GLfloat, GLfloat))SDL_GL_GetProcAddress("glMultiTexCoord2fARB");
			qglActiveTextureARB = (void (APIENTRY*)(GLenum))SDL_GL_GetProcAddress("glActiveTextureARB");
			qglClientActiveTextureARB = (void (APIENTRY*)(GLenum))SDL_GL_GetProcAddress("glClientActiveTextureARB");

			if (qglActiveTextureARB)
			{
				GLint glint = 0;
				qglGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &glint);
				glConfig.numTextureUnits = (int)glint;
				if (glConfig.numTextureUnits > 1)
				{
					ri.Printf(PRINT_ALL, "...using GL_ARB_multitexture\n");
				}
				else
				{
					qglMultiTexCoord2fARB = NULL;
					qglActiveTextureARB = NULL;
					qglClientActiveTextureARB = NULL;
					ri.Printf(PRINT_ALL, "...not using GL_ARB_multitexture, < 2 texture units\n");
				}
			}
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_ARB_multitexture\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_ARB_multitexture not found\n");
	}

	// GL_EXT_compiled_vertex_array
	if (GLimp_HaveExtension("GL_EXT_compiled_vertex_array"))
	{
		if (r_ext_compiled_vertex_array->integer)
		{
			ri.Printf(PRINT_ALL, "...using GL_EXT_compiled_vertex_array\n");
			qglLockArraysEXT = (void (APIENTRY*)(int, int)) SDL_GL_GetProcAddress("glLockArraysEXT");
			qglUnlockArraysEXT = (void (APIENTRY*)(void)) SDL_GL_GetProcAddress("glUnlockArraysEXT");
			if (!qglLockArraysEXT || !qglUnlockArraysEXT)
			{
				ri.Error(ERR_FATAL, "bad getprocaddress");
			}
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_EXT_compiled_vertex_array\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_EXT_compiled_vertex_array not found\n");
	}

	textureFilterAnisotropic = qfalse;
	if (GLimp_HaveExtension("GL_EXT_texture_filter_anisotropic"))
	{
		if (r_ext_texture_filter_anisotropic->integer) {
			// Changed from float to GLint
			qglGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
			if (maxAnisotropy <= 0) {
				ri.Printf(PRINT_ALL, "...GL_EXT_texture_filter_anisotropic not properly supported!\n");
				maxAnisotropy = 0;
			}
			else
			{
				ri.Printf(PRINT_ALL, "...using GL_EXT_texture_filter_anisotropic (max: %i)\n", maxAnisotropy);
				textureFilterAnisotropic = qtrue;
			}
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_EXT_texture_filter_anisotropic\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_EXT_texture_filter_anisotropic not found\n");
	}

#ifdef FRAMEBUFFER_AND_GLSL_SUPPORT
	qglGenFramebuffersEXT = NULL;
	qglBindFramebufferEXT = NULL;
	qglGenRenderbuffersEXT = NULL;
	qglBindRenderbufferEXT = NULL;
	qglRenderbufferStorageEXT = NULL;
	qglRenderbufferStorageMultisampleEXT = NULL;
	qglFramebufferRenderbufferEXT = NULL;
	qglFramebufferTexture2DEXT = NULL;
	qglCheckFramebufferStatusEXT = NULL;
	qglDeleteFramebuffersEXT = NULL;
	qglDeleteRenderbuffersEXT = NULL;
	qglBlitFramebufferEXT = NULL;

	//added fragment/vertex program extensions
	qglAttachShader = NULL;
	qglBindAttribLocation = NULL;
	qglCompileShader = NULL;
	qglCreateProgram = NULL;
	qglCreateShader = NULL;
	qglDeleteProgram = NULL;
	qglDeleteShader = NULL;
	qglShaderSource = NULL;
	qglLinkProgram = NULL;
	qglUseProgram = NULL;
	qglGetUniformLocation = NULL;
	qglUniform1f = NULL;
	qglUniform2f = NULL;
	qglUniform1i = NULL;
	qglGetProgramiv = NULL;
	qglGetProgramInfoLog = NULL;
	qglGetShaderiv = NULL;
	qglGetShaderInfoLog = NULL;

	framebufferSupported = qfalse;
	glslSupported = qfalse;
	multisampleSupported = qfalse;
	if (GLimp_HaveExtension("GL_EXT_framebuffer_object") &&
		GLimp_HaveExtension("GL_ARB_texture_non_power_of_two"))
	{
		ri.Printf(PRINT_ALL, "...using GL_EXT_framebuffer_object\n");
		ri.Printf(PRINT_ALL, "...using GL_ARB_texture_non_power_of_two\n");
		framebufferSupported = qtrue;
		qglGenFramebuffersEXT = (void (APIENTRY*)(GLsizei, GLuint*)) SDL_GL_GetProcAddress("glGenFramebuffersEXT");
		qglBindFramebufferEXT = (void (APIENTRY*)(GLenum, GLuint)) SDL_GL_GetProcAddress("glBindFramebufferEXT");
		qglGenRenderbuffersEXT = (void (APIENTRY*)(GLsizei, GLuint*)) SDL_GL_GetProcAddress("glGenRenderbuffersEXT");
		qglBindRenderbufferEXT = (void (APIENTRY*)(GLenum, GLuint)) SDL_GL_GetProcAddress("glBindRenderbufferEXT");
		qglRenderbufferStorageEXT = (void (APIENTRY*)(GLenum, GLenum, GLsizei, GLsizei)) SDL_GL_GetProcAddress("glRenderbufferStorageEXT");
		if (GLimp_HaveExtension("GL_EXT_framebuffer_multisample") &&
			GLimp_HaveExtension("GL_EXT_framebuffer_blit")) {
			ri.Printf(PRINT_ALL, "...using GL_EXT_framebuffer_multisample\n");
			ri.Printf(PRINT_ALL, "...using GL_EXT_framebuffer_blit\n");
			multisampleSupported = qtrue;
			qglRenderbufferStorageMultisampleEXT = (void (APIENTRY*)(GLenum, GLsizei, GLenum, GLsizei, GLsizei)) SDL_GL_GetProcAddress("glRenderbufferStorageMultisampleEXT");
			qglBlitFramebufferEXT = (void (APIENTRY*)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLenum, GLbitfield)) SDL_GL_GetProcAddress("glBlitFramebufferEXT");
		}
		else {
			if (!strstr(glConfig.extensions_string, "GL_EXT_framebuffer_multisample"))
				ri.Printf(PRINT_WARNING, "WARNING: GL_EXT_framebuffer_multisample is missing\n");
			if (!strstr(glConfig.extensions_string, "GL_EXT_framebuffer_blit"))
				ri.Printf(PRINT_WARNING, "WARNING: GL_EXT_framebuffer_blit is missing\n");
		}
		qglFramebufferRenderbufferEXT = (void (APIENTRY*)(GLenum, GLenum, GLenum, GLuint)) SDL_GL_GetProcAddress("glFramebufferRenderbufferEXT");
		qglFramebufferTexture2DEXT = (void (APIENTRY*)(GLenum, GLenum, GLenum, GLuint, GLint)) SDL_GL_GetProcAddress("glFramebufferTexture2DEXT");
		qglCheckFramebufferStatusEXT = (GLenum(APIENTRY*)(GLenum)) SDL_GL_GetProcAddress("glCheckFramebufferStatusEXT");
		qglDeleteFramebuffersEXT = (void (APIENTRY*)(GLsizei, const GLuint*)) SDL_GL_GetProcAddress("glDeleteFramebuffersEXT");
		qglDeleteRenderbuffersEXT = (void (APIENTRY*)(GLsizei, const GLuint*)) SDL_GL_GetProcAddress("glDeleteRenderbuffersEXT");

		if (!strstr(glConfig.extensions_string, "GL_ARB_depth_texture")) {
			ri.Printf(PRINT_WARNING, "WARNING: GL_ARB_depth_texture is missing\n");
		}
		if (!strstr(glConfig.extensions_string, "GL_EXT_packed_depth_stencil") ||
			!strstr(glConfig.extensions_string, "GL_NV_packed_depth_stencil")) {
			ri.Printf(PRINT_WARNING, "WARNING: packed_depth_stencil is missing\n");
		}

	}
	//added fragment/vertex program extensions
	if (GLimp_HaveExtension("GL_ARB_fragment_shader") &&
		GLimp_HaveExtension("GL_ARB_vertex_program") &&
		GLimp_HaveExtension("GL_ARB_vertex_shader") &&
		GLimp_HaveExtension("GL_ARB_fragment_program") &&
		GLimp_HaveExtension("GL_ARB_shading_language_100"))
	{
		ri.Printf(PRINT_ALL, "...using GL_ARB_fragment_program\n");
		ri.Printf(PRINT_ALL, "...using GL_ARB_vertex_program\n");
		ri.Printf(PRINT_ALL, "...using GL_ARB_vertex_shader\n");
		ri.Printf(PRINT_ALL, "...using GL_ARB_fragment_program\n");
		ri.Printf(PRINT_ALL, "...using GL_ARB_shading_language_100\n");
		glslSupported = qtrue;
		qglAttachShader = (void (APIENTRY*)(GLuint, GLuint)) SDL_GL_GetProcAddress("glAttachShader");
		qglBindAttribLocation = (void (APIENTRY*)(GLuint, GLuint, const GLchar*)) SDL_GL_GetProcAddress("glBindAttribLocation");
		qglCompileShader = (void (APIENTRY*)(GLuint)) SDL_GL_GetProcAddress("glCompileShader");
		qglCreateProgram = (GLuint(APIENTRY*)(void)) SDL_GL_GetProcAddress("glCreateProgram");
		qglCreateShader = (GLuint(APIENTRY*)(GLenum)) SDL_GL_GetProcAddress("glCreateShader");
		qglDeleteProgram = (void (APIENTRY*)(GLuint)) SDL_GL_GetProcAddress("glDeleteProgram");
		qglDeleteShader = (void (APIENTRY*)(GLuint)) SDL_GL_GetProcAddress("glDeleteShader");
		qglShaderSource = (void (APIENTRY*)(GLuint, GLsizei, const GLchar**, const GLint*)) SDL_GL_GetProcAddress("glShaderSource");
		qglLinkProgram = (void (APIENTRY*)(GLuint)) SDL_GL_GetProcAddress("glLinkProgram");
		qglUseProgram = (void (APIENTRY*)(GLuint)) SDL_GL_GetProcAddress("glUseProgram");
		qglGetUniformLocation = (GLint(APIENTRY*)(GLuint, const GLchar*)) SDL_GL_GetProcAddress("glGetUniformLocation");
		qglUniform1f = (void (APIENTRY*)(GLint, GLfloat)) SDL_GL_GetProcAddress("glUniform1f");
		qglUniform2f = (void (APIENTRY*)(GLint, GLfloat, GLfloat)) SDL_GL_GetProcAddress("glUniform2f");
		qglUniform1i = (void (APIENTRY*)(GLint, GLint)) SDL_GL_GetProcAddress("glUniform1i");
		qglGetProgramiv = (void (APIENTRY*)(GLuint, GLenum, GLint*)) SDL_GL_GetProcAddress("glGetProgramiv");
		qglGetProgramInfoLog = (void (APIENTRY*)(GLuint, GLsizei, GLsizei*, GLchar*)) SDL_GL_GetProcAddress("glGetProgramInfoLog");
		qglGetShaderiv = (void (APIENTRY*)(GLuint, GLenum, GLint*)) SDL_GL_GetProcAddress("glGetShaderiv");
		qglGetShaderInfoLog = (void (APIENTRY*)(GLuint, GLsizei, GLsizei*, GLchar*)) SDL_GL_GetProcAddress("glGetShaderInfoLog");
	}
#endif
}

#define R_MODE_FALLBACK 3 // 640 * 480

/*
===============
GLimp_Init

This routine is responsible for initializing the OS specific portions
of OpenGL
===============
*/
void GLimp_Init(void)
{
	// Removed cvar assignments, will use ri.Cvar_Get directly

	if (ri.Cvar_VariableIntegerValue("com_abnormalExit"))
	{
		ri.Cvar_Set("r_mode", va("%d", R_MODE_FALLBACK));
		ri.Cvar_Set("r_fullscreen", "0");
		ri.Cvar_Set("r_centerWindow", "0");
		ri.Cvar_Set("com_abnormalExit", "0");
	}

	ri.Sys_GLimpInit();

	// Create the window and set up the context
	if (GLimp_StartDriverAndSetMode(r_mode->integer, r_fullscreen->integer, r_noborder->integer))
		goto success;

	// Try again, this time in a platform specific "safe mode"
	ri.Sys_GLimpSafeInit();

	if (GLimp_StartDriverAndSetMode(r_mode->integer, r_fullscreen->integer, qfalse))
		goto success;

	// Finally, try the default screen resolution
	if (r_mode->integer != R_MODE_FALLBACK)
	{
		ri.Printf(PRINT_ALL, "Setting r_mode %d failed, falling back on r_mode %d\n",
			r_mode->integer, R_MODE_FALLBACK);

		if (GLimp_StartDriverAndSetMode(R_MODE_FALLBACK, qfalse, qfalse))
			goto success;
	}

	// Nothing worked, give up
	ri.Error(ERR_FATAL, "GLimp_Init() - could not load OpenGL subsystem");

success:
	// This values force the UI to disable driver selection
	glConfig.driverType = GLDRV_ICD;
	glConfig.hardwareType = GLHW_GENERIC;

	// SDL3 removed SetWindowGammaRamp. Gamma control should be handled by the OS or shaders.
	glConfig.deviceSupportsGamma = 0;

	if (-1 == r_ignorehwgamma->integer)
		glConfig.deviceSupportsGamma = 1;

	if (1 == r_ignorehwgamma->integer)
		glConfig.deviceSupportsGamma = 0;

	// get our config strings, checking for NULL return values
	const char* s;

	s = (const char*)qglGetString(GL_VENDOR);
	if (s) Q_strncpyz(glConfig.vendor_string, s, sizeof(glConfig.vendor_string));
	else glConfig.vendor_string[0] = '\0';

	s = (const char*)qglGetString(GL_RENDERER);
	if (s) Q_strncpyz(glConfig.renderer_string, s, sizeof(glConfig.renderer_string));
	else glConfig.renderer_string[0] = '\0';
	if (*glConfig.renderer_string && glConfig.renderer_string[strlen(glConfig.renderer_string) - 1] == '\n')
		glConfig.renderer_string[strlen(glConfig.renderer_string) - 1] = 0;

	s = (const char*)qglGetString(GL_VERSION);
	if (s) Q_strncpyz(glConfig.version_string, s, sizeof(glConfig.version_string));
	else glConfig.version_string[0] = '\0';

	s = (const char*)qglGetString(GL_EXTENSIONS);
	if (s) Q_strncpyz(glConfig.extensions_string, s, sizeof(glConfig.extensions_string));
	else glConfig.extensions_string[0] = '\0';

	// initialize extensions
	GLimp_InitExtensions();

	ri.Cvar_Get("r_availableModes", "", CVAR_ROM);

	// This depends on SDL_INIT_VIDEO, hence having it here
	ri.IN_Init();
}


/*
===============
GLimp_EndFrame

Responsible for doing a swapbuffers
===============
*/
void GLimp_EndFrame(void)
{
	// don't flip if drawing to front buffer
	if (Q_stricmp(r_drawBuffer->string, "GL_FRONT") != 0)
	{
		// Changed to use SDL2/SDL3 function with window handle
		if (sdl_window) {
			SDL_GL_SwapWindow(sdl_window);
		}
	}

	if (r_fullscreen->modified)
	{
		qboolean    fullscreen;
		qboolean    needToToggle = qtrue;

		if (sdl_window)
		{
			// В SDL3 флаг называется SDL_WINDOW_FULLSCREEN
			fullscreen = (SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_FULLSCREEN) != 0;

			if (r_fullscreen->integer && ri.Cvar_VariableIntegerValue("in_nograb"))
			{
				ri.Printf(PRINT_ALL, "Fullscreen not allowed with in_nograb 1\n");
				ri.Cvar_Set("r_fullscreen", "0");
				r_fullscreen->modified = qfalse;
			}

			needToToggle = !!r_fullscreen->integer != fullscreen;

			if (needToToggle)
			{
				// Исправлено для SDL3: используем true/false вместо SDL_TRUE/SDL_FALSE
				// Также проверяем результат как bool (типично для API SDL3)
				if (!SDL_SetWindowFullscreen(sdl_window, r_fullscreen->integer ? true : false))
				{
					// If setting fullscreen failed, fall back to vid_restart
					ri.Cmd_ExecuteText(EXEC_APPEND, "vid_restart\n");
				}
				ri.IN_Restart();
			}
		}

		r_fullscreen->modified = qfalse;
	}
}
