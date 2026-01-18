#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#ifndef DEDICATED
#ifdef USE_LOCAL_HEADERS
#   include "SDL.h"
#else
#   include <SDL3/SDL.h>
#   include <SDL3/SDL_main.h> 
#endif
#endif

#include "sys_local.h"
#include "sys_loadlib.h"

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

#ifdef HotW
#include "../qcommon/sdk_shared.h"
#ifdef SDK_DIFF
#include "../../build/sdk_diff.h"
#endif
#endif

static char binaryPath[MAX_OSPATH] = { 0 };
static char installPath[MAX_OSPATH] = { 0 };

void Sys_SetBinaryPath(const char* path)
{
	Q_strncpyz(binaryPath, path, sizeof(binaryPath));
}

char* Sys_BinaryPath(void)
{
	return binaryPath;
}

void Sys_SetDefaultInstallPath(const char* path)
{
#ifndef HotW
	Q_strncpyz(installPath, path, sizeof(installPath));
#else
	Q_strncpyz(installPath, Sys_GetSystemInstallPath(path), sizeof(installPath));
#endif
}

char* Sys_DefaultInstallPath(void)
{
	if (*installPath)
		return installPath;
	else
		return Sys_Cwd();
}

char* Sys_DefaultAppPath(void)
{
	return Sys_BinaryPath();
}

void Sys_In_Restart_f(void)
{
	IN_Restart();
}

char* Sys_ConsoleInput(void)
{
	return CON_Input();
}

#ifdef DEDICATED
#   define PID_FILENAME PRODUCT_NAME "_server.pid"
#else
#   define PID_FILENAME PRODUCT_NAME ".pid"
#endif

static char* Sys_PIDFileName(void)
{
	const char* homePath = Sys_DefaultHomePath();

	if (*homePath != '\0')
		return va("%s/%s", homePath, PID_FILENAME);

	return NULL;
}

qboolean Sys_WritePIDFile(void)
{
	char* pidFile = Sys_PIDFileName();
	FILE* f = NULL;
	qboolean  stale = qfalse;

	if (pidFile == NULL)
		return qfalse;

#ifdef _MSC_VER
	if (fopen_s(&f, pidFile, "r") == 0 && f != NULL)
#else
	f = fopen(pidFile, "r");
	if (f != NULL)
#endif
	{
		char  pidBuffer[64] = { 0 };
		size_t bytesRead = fread(pidBuffer, sizeof(char), sizeof(pidBuffer) - 1, f);
		fclose(f);

		if (bytesRead > 0)
		{
			int pid = atoi(pidBuffer);
			if (!Sys_PIDIsRunning(pid))
				stale = qtrue;
		}
		else
			stale = qtrue;
	}

#ifdef _MSC_VER
	if (fopen_s(&f, pidFile, "w") == 0 && f != NULL)
#else
	f = fopen(pidFile, "w");
	if (f != NULL)
#endif
	{
		fprintf(f, "%d", Sys_PID());
		fclose(f);
	}
	else
		Com_Printf(S_COLOR_YELLOW "Couldn't write %s.\n", pidFile);

	return stale;
}

static __attribute__((noreturn)) void Sys_Exit(int exitCode)
{
	CON_Shutdown();

#ifndef DEDICATED
	SDL_Quit();
#endif

	if (exitCode < 2)
	{
		char* pidFile = Sys_PIDFileName();

		if (pidFile != NULL)
			remove(pidFile);
	}

	NET_Shutdown();

	Sys_PlatformExit();

	exit(exitCode);
}

void Sys_Quit(void)
{
	Sys_Exit(0);
}

cpuFeatures_t Sys_GetProcessorFeatures(void)
{
	cpuFeatures_t features = 0;

#ifndef DEDICATED
	// SDL3 removed HasRDTSC and Has3DNow as obsolete
	// We only check for modern extensions supported by SDL3
	if (SDL_HasMMX())    features |= CF_MMX;
	if (SDL_HasSSE())    features |= CF_SSE;
	if (SDL_HasSSE2())   features |= CF_SSE2;
	if (SDL_HasAltiVec()) features |= CF_ALTIVEC;
#endif

	return features;
}

void Sys_Init(void)
{
	Cmd_AddCommand("in_restart", Sys_In_Restart_f);
	Cvar_Set("arch", OS_STRING " " ARCH_STRING);
	Cvar_Set("username", Sys_GetCurrentUser());
}

void Sys_AnsiColorPrint(const char* msg)
{
	static char buffer[MAXPRINTMSG];
	int         length = 0;
	static int  q3ToAnsi[NUM_COLORS] =
	{
		30, // COLOR_BLACK
		31, // COLOR_RED
		32, // COLOR_GREEN
		33, // COLOR_YELLOW
		34, // COLOR_BLUE
		36, // COLOR_CYAN
		35, // COLOR_MAGENTA
		0   // COLOR_WHITE
	};

	while (*msg)
	{
		if (Q_IsColorString(msg) || *msg == '\n')
		{
			if (length > 0)
			{
				buffer[length] = '\0';
				fputs(buffer, stderr);
				length = 0;
			}

			if (*msg == '\n')
			{
				fputs("\033[0m\n", stderr);
				msg++;
			}
			else
			{
				Com_sprintf(buffer, sizeof(buffer), "\033[%dm",
					q3ToAnsi[ColorIndex(*(msg + 1))]);
				fputs(buffer, stderr);
				msg += 2;
			}
		}
		else
		{
			if (length >= MAXPRINTMSG - 1)
				break;

			buffer[length] = *msg;
			length++;
			msg++;
		}
	}

	if (length > 0)
	{
		buffer[length] = '\0';
		fputs(buffer, stderr);
	}
}

void Sys_Print(const char* msg)
{
	CON_LogWrite(msg);
	CON_Print(msg);
}

void Sys_Error(const char* error, ...)
{
	va_list argptr;
	char    string[1024];

	va_start(argptr, error);
	Q_vsnprintf(string, sizeof(string), error, argptr);
	va_end(argptr);

	Sys_ErrorDialog(string);

	Sys_Exit(3);
}

int Sys_FileTime(char* path)
{
	struct stat buf;

	if (stat(path, &buf) == -1)
		return -1;

	return (int)buf.st_mtime;
}

void Sys_UnloadDll(void* dllHandle)
{
	if (!dllHandle)
	{
		Com_Printf("Sys_UnloadDll(NULL)\n");
		return;
	}

	Sys_UnloadLibrary(dllHandle);
}

void* Sys_LoadDll(const char* name, qboolean useSystemLib)
{
	void* dllhandle;

	if (useSystemLib)
		Com_Printf("Trying to load \"%s\"...\n", name);

	if (!useSystemLib || !(dllhandle = Sys_LoadLibrary(name)))
	{
		const char* topDir;
		char libPath[MAX_OSPATH];

		topDir = Sys_BinaryPath();

		if (!*topDir)
			topDir = ".";

		Com_Printf("Trying to load \"%s\" from \"%s\"...\n", name, topDir);
		Com_sprintf(libPath, sizeof(libPath), "%s%c%s", topDir, PATH_SEP, name);

		if (!(dllhandle = Sys_LoadLibrary(libPath)))
		{
			const char* basePath = Cvar_VariableString("fs_basepath");

			if (!basePath || !*basePath)
				basePath = ".";

			if (FS_FilenameCompare(topDir, basePath))
			{
				Com_Printf("Trying to load \"%s\" from \"%s\"...\n", name, basePath);
				Com_sprintf(libPath, sizeof(libPath), "%s%c%s", basePath, PATH_SEP, name);
				dllhandle = Sys_LoadLibrary(libPath);
			}

			if (!dllhandle)
				Com_Printf("Loading \"%s\" failed\n", name);
		}
	}

	return dllhandle;
}

void* Sys_LoadGameDll(const char* name,
	intptr_t(QDECL** entryPoint)(int, ...),
	intptr_t(*systemcalls)(intptr_t, ...))
{
	void* libHandle;
	void (*dllEntry)(intptr_t(*syscallptr)(intptr_t, ...));

	assert(name);

	Com_Printf("Loading DLL file: %s\n", name);
	libHandle = Sys_LoadLibrary(name);

	if (!libHandle)
	{
		Com_Printf("Sys_LoadGameDll(%s) failed:\n\"%s\"\n", name, Sys_LibraryError());
		return NULL;
	}

	// Cast function pointers to match SDL3/Modern C standards to avoid warnings
	dllEntry = (void (*)(intptr_t(*)(intptr_t, ...)))Sys_LoadFunction(libHandle, "dllEntry");
	*entryPoint = (intptr_t(QDECL*)(int, ...))Sys_LoadFunction(libHandle, "vmMain");

	if (!*entryPoint || !dllEntry)
	{
		Com_Printf("Sys_LoadGameDll(%s) failed to find vmMain function:\n\"%s\" !\n", name, Sys_LibraryError());
		Sys_UnloadLibrary(libHandle);

		return NULL;
	}

	Com_Printf("Sys_LoadGameDll(%s) found vmMain function at %p\n", name, *entryPoint);
	dllEntry(systemcalls);

	return libHandle;
}


/*
=================
Sys_ParseArgs
=================
*/
void Sys_ParseArgs(int argc, char** argv)
{
	if (argc == 2)
	{
		if (!strcmp(argv[1], "--version") ||
			!strcmp(argv[1], "-v"))
		{
			const char* date = __DATE__;
#ifdef HotW
			const char* time = __TIME__;
			fprintf(stdout, "%s\n", argv[0]);
#ifdef DEDICATED
			fprintf(stdout, Q3_VERSION " dedicated server (%s, %s)\n", date, time);
#else
			fprintf(stdout, Q3_VERSION " client (%s, %s)\n", date, time);
#endif
			fprintf(stdout, "Release: " PRODUCT_RELEASE "\n");
			fprintf(stdout, "Flavour: " OS_STRING " " ARCH_STRING "\n\n");
			Sys_BinaryEngineComment();
#else
#ifdef DEDICATED
			fprintf(stdout, Q3_VERSION " dedicated server (%s)\n", date);
#else
			fprintf(stdout, Q3_VERSION " client (%s)\n", date);
#endif
#endif
			Sys_Exit(0);
		}
	}
}

#ifndef DEFAULT_BASEDIR
#   ifdef MACOS_X
#       define DEFAULT_BASEDIR Sys_StripAppBundle(Sys_BinaryPath())
#   else
#       define DEFAULT_BASEDIR Sys_BinaryPath()
#   endif
#endif

void Sys_SigHandler(int signal)
{
	static qboolean signalcaught = qfalse;

	if (signalcaught)
	{
		fprintf(stderr, "DOUBLE SIGNAL FAULT: Received signal %d, exiting...\n",
			signal);
	}
	else
	{
		signalcaught = qtrue;
		VM_Forced_Unload_Start();
#ifndef DEDICATED
		CL_Shutdown(va("Received signal %d", signal), qtrue, qtrue);
#endif
		SV_Shutdown(va("Received signal %d", signal));
		VM_Forced_Unload_Done();
	}

	if (signal == SIGTERM || signal == SIGINT)
		Sys_Exit(1);
	else
		Sys_Exit(2);
}

#ifdef HotW
void Sys_BinaryEngineComment(void) {
	const char* version = Q3_VERSION;
	const char* release = PRODUCT_RELEASE;
	const char* platform = PLATFORM_STRING;
	const char* date = __DATE__;
	const char* time = __TIME__;
	const char* comment = SDK_COMMENT;
	const char* contact = SDK_CONTACT;
	char* sum;
	char string[MAX_STRING_CHARS] = "";

	Com_sprintf(string, sizeof(string), "%s (%s - %s %s), %s",
		version, release, date, time, platform);

	if (strlen(comment))
		Q_strcat(string, sizeof(string), va("\ncomment: %s", comment));
	if (strlen(contact))
		Q_strcat(string, sizeof(string), va("\ncontact: %s", contact));

	fprintf(stdout, "engine : %s\n", string);

	/* strlen returns size_t; Com_MD5Text* expect int length → cast to int */
#ifndef SDK_DIFF
	{
		int slen = (int)strlen(string);
		sum = Com_MD5Text(string, slen, NULL, 0);
	}
#else
	{
		int slen = (int)strlen(string);
		sum = Com_MD5TextArray(sdk_diff, sdk_diff_size, string, slen);
	}
#endif

	fprintf(stdout, "md5sum: %s\n", sum);

	/* Avoid segfault if engine is called with --version argument */
	if (!com_version)
		return;

	Cvar_Set("cl_md5", va("%s", sum));

#ifndef SDK_DIFF
	Cvar_Set("sdk_engine_comment",
		va("\\version\\%s\\release\\%s\\platform\\%s\\date\\%s %s\\md5\\%s\\comment\\%s\\contact\\%s",
			version, release, platform, date, time, sum, comment, contact));
#else
	Cvar_Set("sdk_engine_comment",
		va("\\version\\%s\\release\\%s\\platform\\%s\\date\\%s %s\\md5\\%s\\diff\\%i\\comment\\%s\\contact\\%s",
			version, release, platform, date, time, sum, sdk_diff_format, comment, contact));
#endif
}
#endif

int main(int argc, char** argv)
{
	int  i;
	char  commandLine[MAX_STRING_CHARS] = { 0 };

#if defined HotW && defined DEDICATED
	char* cv_name, * cv_value;
	qboolean qdaemon = qfalse;
	char* quser = NULL, * qjail = NULL;
#endif

#ifndef DEDICATED
	// SDL3 Initialization
	// We MUST initialize SDL here in the main executable, not just in the renderer DLL.
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
		Sys_Dialog(DT_ERROR, va("SDL_Init failed: %s", SDL_GetError()), "SDL3 Initialization Error");
		return 1;
	}
#endif

	Sys_PlatformInit();

	Sys_Milliseconds();

	Sys_ParseArgs(argc, argv);
	Sys_SetBinaryPath(Sys_Dirname(argv[0]));
	Sys_SetDefaultInstallPath(DEFAULT_BASEDIR);

	for (i = 1; i < argc; i++)
	{
		const qboolean containsSpaces = strchr(argv[i], ' ') != NULL;
		if (containsSpaces)
			Q_strcat(commandLine, sizeof(commandLine), "\"");

		Q_strcat(commandLine, sizeof(commandLine), argv[i]);

		if (containsSpaces)
			Q_strcat(commandLine, sizeof(commandLine), "\"");

		Q_strcat(commandLine, sizeof(commandLine), " ");
#if defined HotW && defined DEDICATED
		if (!strcmp(argv[i], "+set") && i + 2 < argc) {
			cv_name = argv[i + 1];
			cv_value = argv[i + 2];
			if (!strcmp(cv_name, "sv_chroot"))
				qjail = cv_value;
			else if (!strcmp(cv_name, "sv_user"))
				quser = cv_value;
			else if (!strcmp(cv_name, "sv_daemon") && atoi(cv_value))
				qdaemon = qtrue;
		}
	}
	Sys_LockMyself(qjail, quser);

	if (qdaemon) {
		Sys_Daemonize();
	}
#else
}
#endif

	Com_Init(commandLine);
	NET_Init();

	CON_Init();

	signal(SIGILL, Sys_SigHandler);
	signal(SIGFPE, Sys_SigHandler);
	signal(SIGSEGV, Sys_SigHandler);
	signal(SIGTERM, Sys_SigHandler);
	signal(SIGINT, Sys_SigHandler);

#ifdef HotW
	Sys_PlatformPostInit(argv[0]);
	Cvar_Get("sa_engine_inuse", "1", CVAR_ROM);
#endif

	while (1)
	{
		IN_Frame();
		Com_Frame();
	}

#ifndef DEDICATED
	SDL_Quit();
#endif

	return 0;
}