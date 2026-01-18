#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sys_local.h"

#include <windows.h>
#include <lmerr.h>
#include <lmcons.h>
#include <lmwksta.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>
#include <wincrypt.h>
#include <shlobj.h>
#include <psapi.h>
#include <float.h>
#include <stdlib.h> // Необходимо для _dupenv_s, _controlfp_s

// Used to determine where to store user-specific files
static char homePath[MAX_OSPATH] = { 0 };

#ifndef DEDICATED
static UINT timerResolution = 0;
#endif

/*
================
Sys_SetFPUCW
Set FPU control word to default value
================
*/

#ifndef _RC_CHOP
// mingw doesn't seem to have these defined :(

#define _MCW_EM	0x0008001fU
#define _MCW_RC	0x00000300U
#define _MCW_PC	0x00030000U
#define _RC_NEAR      0x00000000U
#define _PC_53	0x00010000U

unsigned int _controlfp(unsigned int new, unsigned int mask);
#endif

#define FPUCWMASK1 (_MCW_RC | _MCW_EM)
#define FPUCW (_RC_NEAR | _MCW_EM | _PC_53)

#if idx64
#define FPUCWMASK	(FPUCWMASK1)
#else
#define FPUCWMASK	(FPUCWMASK1 | _MCW_PC)
#endif

void Sys_SetFloatEnv(void)
{
	// Исправлено предупреждение C4996: _controlfp -> _controlfp_s
	unsigned int currentControl;
	_controlfp_s(&currentControl, FPUCW, FPUCWMASK);
}

/*
================
Sys_DefaultHomePath
================
*/
char* Sys_DefaultHomePath(void)
{
	TCHAR szPath[MAX_PATH];
	FARPROC qSHGetFolderPath;
	HMODULE shfolder = LoadLibrary("shfolder.dll");

	if (shfolder == NULL)
	{
		Com_Printf("Unable to load SHFolder.dll\n");
		return NULL;
	}

	if (!*homePath && com_homepath)
	{
		qSHGetFolderPath = GetProcAddress(shfolder, "SHGetFolderPathA");
		if (qSHGetFolderPath == NULL)
		{
			Com_Printf("Unable to find SHGetFolderPath in SHFolder.dll\n");
			FreeLibrary(shfolder);
			return NULL;
		}

		if (!SUCCEEDED(qSHGetFolderPath(NULL, CSIDL_APPDATA,
			NULL, 0, szPath)))
		{
			Com_Printf("Unable to detect CSIDL_APPDATA\n");
			FreeLibrary(shfolder);
			return NULL;
		}

		Com_sprintf(homePath, sizeof(homePath), "%s%c", szPath, PATH_SEP);

		if (com_homepath->string[0])
			Q_strcat(homePath, sizeof(homePath), com_homepath->string);
		else
			Q_strcat(homePath, sizeof(homePath), HOMEPATH_NAME_WIN);
	}

	FreeLibrary(shfolder);
	return homePath;
}

/*
==================
Sys_GetSystemInstallPath
==================
*/
#ifdef HotW
const char* Sys_GetSystemInstallPath(const char* path)
{
	return path;
}
#endif

/*
================
Sys_Milliseconds
================
*/
int sys_timeBase;
int Sys_Milliseconds(void)
{
	int             sys_curtime;
	static qboolean initialized = qfalse;

	if (!initialized) {
		sys_timeBase = timeGetTime();
		initialized = qtrue;
	}
	sys_curtime = timeGetTime() - sys_timeBase;

	return sys_curtime;
}

/*
================
Sys_RandomBytes
================
*/
qboolean Sys_RandomBytes(byte* string, int len)
{
	HCRYPTPROV  prov;

	if (!CryptAcquireContext(&prov, NULL, NULL,
		PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {

		return qfalse;
	}

	if (!CryptGenRandom(prov, len, (BYTE*)string)) {
		CryptReleaseContext(prov, 0);
		return qfalse;
	}
	CryptReleaseContext(prov, 0);
	return qtrue;
}

/*
================
Sys_GetCurrentUser
================
*/
char* Sys_GetCurrentUser(void)
{
	static char s_userName[1024];
	unsigned long size = sizeof(s_userName);

	if (!GetUserName(s_userName, &size))
		Q_strncpyz(s_userName, "player", sizeof(s_userName));

	if (!s_userName[0])
	{
		Q_strncpyz(s_userName, "player", sizeof(s_userName));
	}

	return s_userName;
}

/*
================
Sys_GetClipboardData
================
*/
char* Sys_GetClipboardData(void)
{
	char* data = NULL;
	char* cliptext;

	if (OpenClipboard(NULL) != 0) {
		HANDLE hClipboardData;

		if ((hClipboardData = GetClipboardData(CF_TEXT)) != 0) {
			if ((cliptext = (char*)GlobalLock(hClipboardData)) != 0) {
				// Получаем размер и явно приводим к int, чтобы убрать C4244
				// SIZE_T в x64 — это 64 бита, Z_Malloc ожидает int (32 бита)
				int clipLength = (int)GlobalSize(hClipboardData);

				data = (char*)Z_Malloc(clipLength + 1);
				if (data) {
					Q_strncpyz(data, cliptext, clipLength + 1);
					GlobalUnlock(hClipboardData);

					// Исправлено предупреждение C4996: strtok -> strtok_s
					char* nextToken = NULL;
					strtok_s(data, "\n\r\b", &nextToken);
				}
				else {
					GlobalUnlock(hClipboardData);
				}
			}
		}
		CloseClipboard();
	}
	return data;
}

#define MEM_THRESHOLD 96*1024*1024

/*
==================
Sys_LowPhysicalMemory
==================
*/
qboolean Sys_LowPhysicalMemory(void)
{
	MEMORYSTATUS stat;
	GlobalMemoryStatus(&stat);
	return (stat.dwTotalPhys <= MEM_THRESHOLD) ? qtrue : qfalse;
}

/*
==============
Sys_Basename
==============
*/
const char* Sys_Basename(char* path)
{
	static char base[MAX_OSPATH] = { 0 };
	int length;

	// Исправлено предупреждение C4267: приведение size_t к int
	length = (int)strlen(path) - 1;

	// Skip trailing slashes
	while (length > 0 && path[length] == '\\')
		length--;

	while (length > 0 && path[length - 1] != '\\')
		length--;

	Q_strncpyz(base, &path[length], sizeof(base));

	// Исправлено предупреждение C4267: приведение size_t к int
	length = (int)strlen(base) - 1;

	// Strip trailing slashes
	while (length > 0 && base[length] == '\\')
		base[length--] = '\0';

	return base;
}

/*
==============
Sys_Dirname
==============
*/
const char* Sys_Dirname(char* path)
{
	static char dir[MAX_OSPATH] = { 0 };
	int length;

	Q_strncpyz(dir, path, sizeof(dir));
	// Исправлено предупреждение C4267: приведение size_t к int
	length = (int)strlen(dir) - 1;

	while (length > 0 && dir[length] != '\\')
		length--;

	dir[length] = '\0';

	return dir;
}

/*
==============
Sys_FOpen
==============
*/
FILE* Sys_FOpen(const char* ospath, const char* mode) {
	// Исправлено предупреждение C4996: fopen -> fopen_s
	FILE* f = NULL;
	if (fopen_s(&f, ospath, mode) != 0) {
		return NULL;
	}
	return f;
}

/*
==============
Sys_Mkdir
==============
*/
qboolean Sys_Mkdir(const char* path)
{
	if (!CreateDirectory(path, NULL))
	{
		if (GetLastError() != ERROR_ALREADY_EXISTS)
			return qfalse;
	}

	return qtrue;
}

/*
==================
Sys_Mkfifo
Noop on windows because named pipes do not function the same way
==================
*/
FILE* Sys_Mkfifo(const char* ospath)
{
	return NULL;
}

/*
==============
Sys_Cwd
==============
*/
char* Sys_Cwd(void) {
	static char cwd[MAX_OSPATH];

	_getcwd(cwd, sizeof(cwd) - 1);
	cwd[MAX_OSPATH - 1] = 0;

	return cwd;
}

/*
==============================================================

DIRECTORY SCANNING

==============================================================
*/

#define MAX_FOUND_FILES 0x1000

/*
==============
Sys_ListFilteredFiles
==============
*/
void Sys_ListFilteredFiles(const char* basedir, char* subdirs, char* filter, char** list, int* numfiles)
{
	char		search[MAX_OSPATH], newsubdirs[MAX_OSPATH];
	char		filename[MAX_OSPATH];
	intptr_t	findhandle;
	struct _finddata_t findinfo;

	if (*numfiles >= MAX_FOUND_FILES - 1) {
		return;
	}

	if (strlen(subdirs)) {
		Com_sprintf(search, sizeof(search), "%s\\%s\\*", basedir, subdirs);
	}
	else {
		Com_sprintf(search, sizeof(search), "%s\\*", basedir);
	}

	findhandle = _findfirst(search, &findinfo);
	if (findhandle == -1) {
		return;
	}

	do {
		if (findinfo.attrib & _A_SUBDIR) {
			if (Q_stricmp(findinfo.name, ".") && Q_stricmp(findinfo.name, "..")) {
				if (strlen(subdirs)) {
					Com_sprintf(newsubdirs, sizeof(newsubdirs), "%s\\%s", subdirs, findinfo.name);
				}
				else {
					Com_sprintf(newsubdirs, sizeof(newsubdirs), "%s", findinfo.name);
				}
				Sys_ListFilteredFiles(basedir, newsubdirs, filter, list, numfiles);
			}
		}
		if (*numfiles >= MAX_FOUND_FILES - 1) {
			break;
		}
		Com_sprintf(filename, sizeof(filename), "%s\\%s", subdirs, findinfo.name);
		if (!Com_FilterPath(filter, filename, qfalse))
			continue;
		list[*numfiles] = CopyString(filename);
		(*numfiles)++;
	} while (_findnext(findhandle, &findinfo) != -1);

	_findclose(findhandle);
}

/*
==============
strgtr
==============
*/
static qboolean strgtr(const char* s0, const char* s1)
{
	int l0, l1, i;

	// Исправлено предупреждение C4267: приведение size_t к int
	l0 = (int)strlen(s0);
	l1 = (int)strlen(s1);

	if (l1 < l0) {
		l0 = l1;
	}

	for (i = 0; i < l0; i++) {
		if (s1[i] > s0[i]) {
			return qtrue;
		}
		if (s1[i] < s0[i]) {
			return qfalse;
		}
	}
	return qfalse;
}

/*
==============
Sys_ListFiles
==============
*/
char** Sys_ListFiles(const char* directory, const char* extension, char* filter, int* numfiles, qboolean wantsubs)
{
	char		search[MAX_OSPATH];
	int			nfiles;
	char** listCopy;
	char* list[MAX_FOUND_FILES];
	struct _finddata_t findinfo;
	intptr_t		findhandle;
	int			flag;
	int			i;

	if (filter) {

		nfiles = 0;
		Sys_ListFilteredFiles(directory, "", filter, list, &nfiles);

		list[nfiles] = 0;
		*numfiles = nfiles;

		if (!nfiles)
			return NULL;

		listCopy = Z_Malloc((nfiles + 1) * sizeof(*listCopy));
		for (i = 0; i < nfiles; i++) {
			listCopy[i] = list[i];
		}
		listCopy[i] = NULL;

		return listCopy;
	}

	if (!extension) {
		extension = "";
	}

	// passing a slash as extension will find directories
	if (extension[0] == '/' && extension[1] == 0) {
		extension = "";
		flag = 0;
	}
	else {
		flag = _A_SUBDIR;
	}

	Com_sprintf(search, sizeof(search), "%s\\*%s", directory, extension);

	// search
	nfiles = 0;

	findhandle = _findfirst(search, &findinfo);
	if (findhandle == -1) {
		*numfiles = 0;
		return NULL;
	}

	do {
		if ((!wantsubs && flag ^ (findinfo.attrib & _A_SUBDIR)) || (wantsubs && findinfo.attrib & _A_SUBDIR)) {
			if (nfiles == MAX_FOUND_FILES - 1) {
				break;
			}
			list[nfiles] = CopyString(findinfo.name);
			nfiles++;
		}
	} while (_findnext(findhandle, &findinfo) != -1);

	list[nfiles] = 0;

	_findclose(findhandle);

	// return a copy of the list
	*numfiles = nfiles;

	if (!nfiles) {
		return NULL;
	}

	listCopy = Z_Malloc((nfiles + 1) * sizeof(*listCopy));
	for (i = 0; i < nfiles; i++) {
		listCopy[i] = list[i];
	}
	listCopy[i] = NULL;

	do {
		flag = 0;
		for (i = 1; i < nfiles; i++) {
			if (strgtr(listCopy[i - 1], listCopy[i])) {
				char* temp = listCopy[i];
				listCopy[i] = listCopy[i - 1];
				listCopy[i - 1] = temp;
				flag = 1;
			}
		}
	} while (flag);

	return listCopy;
}

/*
==============
Sys_FreeFileList
==============
*/
void Sys_FreeFileList(char** list)
{
	int i;

	if (!list) {
		return;
	}

	for (i = 0; list[i]; i++) {
		Z_Free(list[i]);
	}

	Z_Free(list);
}


/*
==============
Sys_Sleep

Block execution for msec or until input is received.
==============
*/
void Sys_Sleep(int msec)
{
	if (msec == 0)
		return;

#ifdef DEDICATED
	if (msec < 0)
		WaitForSingleObject(GetStdHandle(STD_INPUT_HANDLE), INFINITE);
	else
		WaitForSingleObject(GetStdHandle(STD_INPUT_HANDLE), msec);
#else
	// Client Sys_Sleep doesn't support waiting on stdin
	if (msec < 0)
		return;

	Sleep(msec);
#endif
}

/*
==============
Sys_ErrorDialog

Display an error message
==============
*/
void Sys_ErrorDialog(const char* error)
{
	if (Sys_Dialog(DT_YES_NO, va("%s. Copy console log to clipboard?", error),
		"Error") == DR_YES)
	{
		HGLOBAL memoryHandle;
		char* clipMemory;

		memoryHandle = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, CON_LogSize() + 1);
		clipMemory = (char*)GlobalLock(memoryHandle);

		if (clipMemory)
		{
			char* p = clipMemory;
			char buffer[1024];
			unsigned int size;

			while ((size = CON_LogRead(buffer, sizeof(buffer))) > 0)
			{
				Com_Memcpy(p, buffer, size);
				p += size;
			}

			*p = '\0';

			if (OpenClipboard(NULL) && EmptyClipboard())
				SetClipboardData(CF_TEXT, memoryHandle);

			GlobalUnlock(clipMemory);
			CloseClipboard();
		}
	}
}

/*
==============
Sys_Dialog

Display a win32 dialog box
==============
*/
dialogResult_t Sys_Dialog(dialogType_t type, const char* message, const char* title)
{
	UINT uType;

	switch (type)
	{
	default:
	case DT_INFO:      uType = MB_ICONINFORMATION | MB_OK; break;
	case DT_WARNING:   uType = MB_ICONWARNING | MB_OK; break;
	case DT_ERROR:     uType = MB_ICONERROR | MB_OK; break;
	case DT_YES_NO:    uType = MB_ICONQUESTION | MB_YESNO; break;
	case DT_OK_CANCEL: uType = MB_ICONWARNING | MB_OKCANCEL; break;
	}

	switch (MessageBox(NULL, message, title, uType))
	{
	default:
	case IDOK:      return DR_OK;
	case IDCANCEL:  return DR_CANCEL;
	case IDYES:     return DR_YES;
	case IDNO:      return DR_NO;
	}
}

#ifndef DEDICATED
static qboolean SDL_VIDEODRIVER_externallySet = qfalse;
#endif

/*
==============
Sys_GLimpSafeInit

Windows specific "safe" GL implementation initialisation
==============
*/
void Sys_GLimpSafeInit(void)
{
#ifndef DEDICATED
	if (!SDL_VIDEODRIVER_externallySet)
	{
		// Here, we want to let SDL decide what do to unless
		// explicitly requested otherwise
		_putenv_s("SDL_VIDEODRIVER", "");
	}
#endif
}

/*
==============
Sys_GLimpInit

Windows specific GL implementation initialisation
==============
*/
void Sys_GLimpInit(void)
{
#ifndef DEDICATED
	if (!SDL_VIDEODRIVER_externallySet)
	{
		// It's a little bit weird having in_mouse control the
		// video driver, but from ioq3's point of view they're
		// virtually the same except for the mouse input anyway
		if (Cvar_VariableIntegerValue("in_mouse") == -1)
		{
			// Use the windib SDL backend, which is closest to
			// the behaviour of idq3 with in_mouse set to -1
			_putenv_s("SDL_VIDEODRIVER", "windib");
		}
		else
		{
			// Use the DirectX SDL backend
			_putenv_s("SDL_VIDEODRIVER", "directx");
		}
	}
#endif
}

/*
==============
Sys_PlatformInit

Windows specific initialisation
==============
*/
void Sys_PlatformInit(void)
{
#ifndef DEDICATED
	TIMECAPS ptc;
	// Исправлено предупреждение C4996: getenv -> _dupenv_s
	char* SDL_VIDEODRIVER = NULL;
	size_t sz = 0;
	if (_dupenv_s(&SDL_VIDEODRIVER, &sz, "SDL_VIDEODRIVER") != 0)
	{
		SDL_VIDEODRIVER = NULL; // Ошибка или не найдено
	}
#endif

	Sys_SetFloatEnv();

#ifndef DEDICATED
	if (SDL_VIDEODRIVER)
	{
		Com_Printf("SDL_VIDEODRIVER is externally set to \"%s\", "
			"in_mouse -1 will have no effect\n", SDL_VIDEODRIVER);
		SDL_VIDEODRIVER_externallySet = qtrue;
		free(SDL_VIDEODRIVER); // _dupenv_s выделяет память, нужно освободить
	}
	else
		SDL_VIDEODRIVER_externallySet = qfalse;

	if (timeGetDevCaps(&ptc, sizeof(ptc)) == MMSYSERR_NOERROR)
	{
		timerResolution = ptc.wPeriodMin;

		if (timerResolution > 1)
		{
			Com_Printf("Warning: Minimum supported timer resolution is %ums "
				"on this system, recommended resolution 1ms\n", timerResolution);
		}

		timeBeginPeriod(timerResolution);
	}
	else
		timerResolution = 0;
#endif
}

/*
==============
Sys_PlatformExit

Windows specific initialisation
==============
*/
void Sys_PlatformExit(void)
{
#ifndef DEDICATED
	if (timerResolution)
		timeEndPeriod(timerResolution);
#endif
}

/*
==============
Sys_SetEnv

set/unset environment variables (empty value removes it)
==============
*/
void Sys_SetEnv(const char* name, const char* value)
{
	// Исправлено C4996: _putenv -> _putenv_s
	if (value)
		_putenv_s(name, value);
	else
		_putenv_s(name, "");
}

/*
==============
Sys_GetEnv

get environment variables
==============
*/
#ifdef HotW
char* Sys_GetEnv(const char* name)
{
	// Исправлено C4996: getenv -> getenv_s
	// Используем статический буфер, чтобы сохранить семантику возврата указателя
	// без необходимости вызывать free() вызывающей стороной.
	static char envBuf[4096];
	size_t requiredSize = 0;

	if (getenv_s(&requiredSize, envBuf, sizeof(envBuf), name) == 0 && requiredSize > 0)
	{
		return envBuf;
	}
	return NULL;
}
#endif

/*
==============
Sys_PID
==============
*/
int Sys_PID(void)
{
	return GetCurrentProcessId();
}

/*
==============
Sys_PIDIsRunning
==============
*/
qboolean Sys_PIDIsRunning(int pid)
{
	DWORD processes[1024];
	DWORD numBytes, numProcesses;
	int i;

	if (!EnumProcesses(processes, sizeof(processes), &numBytes))
		return qfalse; // Assume it's not running

	numProcesses = numBytes / sizeof(DWORD);

	// Search for the pid
	for (i = 0; i < numProcesses; i++)
	{
		// Исправлено предупреждение C4389: несоответствие signed/unsigned
		if (processes[i] == (DWORD)pid)
			return qtrue;
	}

	return qfalse;
}

/*
==================
Sys_PlatformPostInit
==================
*/
#if defined HotW
void Sys_PlatformPostInit(char* progname)
{
}
#endif

/*
==================
Sys_LockMyself

Not implemented for WIN32
==================
*/
#if defined HotW && defined DEDICATED
void Sys_LockMyself(const char* qjail, const char* quser)
{
}

/*
==================
Sys_Daemonize

Not implemented for WIN32
==================
*/
void Sys_Daemonize(void)
{
}
#endif