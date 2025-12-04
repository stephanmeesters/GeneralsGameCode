#include <windows.h>
#include <stdlib.h>
#include <crtdbg.h>

#include "Lib/BaseType.h"

#include "Common/CommandLine.h"
#include "Common/CriticalSection.h"
#include "Common/GameEngine.h"
#include "Common/GameMemory.h"
#include "Common/GlobalData.h"
#include "Common/StackDump.h"
#include "Common/version.h"

#include "GameClient/ClientInstance.h"

#include "Win32Device/Common/Win32GameEngine.h"

#include "BuildVersion.h"
#include "GeneratedVersion.h"

#ifdef RTS_PROFILE
#include <rts/profile.h>
#endif

// Globals required by the engine (mirrors Main/WinMain.cpp where needed).
HINSTANCE ApplicationHInstance = NULL;
HWND ApplicationHWnd = NULL;

class Win32Mouse;
Win32Mouse *TheWin32Mouse = NULL;

DWORD TheMessageTime = 0;

const Char *g_strFile = "data\\Generals.str";
const Char *g_csfFile = "data\\%s\\Generals.csf";
const char *gAppPrefix = "";

static Bool isWinMainActive = TRUE;

// Critical sections used by core string/memory systems.
static CriticalSection critSec1;
static CriticalSection critSec2;
static CriticalSection critSec3;
static CriticalSection critSec4;
static CriticalSection critSec5;

static LONG WINAPI UnHandledExceptionFilter(struct _EXCEPTION_POINTERS *e_info)
{
	DumpExceptionInfo(e_info->ExceptionRecord->ExceptionCode, e_info);
	return EXCEPTION_EXECUTE_HANDLER;
}

// Factory for the platform-specific GameEngine instance.
GameEngine *CreateGameEngine(void)
{
	Win32GameEngine *engine = NEW Win32GameEngine;
	engine->setIsActive(isWinMainActive);
	return engine;
}

int main(int /*argc*/, char ** /*argv*/)
{
	Int exitcode = 1;

#ifdef RTS_PROFILE
	Profile::StartRange("init");
#endif

	try
	{
		SetUnhandledExceptionFilter(UnHandledExceptionFilter);

		TheAsciiStringCriticalSection = &critSec1;
		TheUnicodeStringCriticalSection = &critSec2;
		TheDmaCriticalSection = &critSec3;
		TheMemoryPoolCriticalSection = &critSec4;
		TheDebugLogCriticalSection = &critSec5;

		// Initialize the memory manager early.
		initMemoryManager();

		// Set working directory to the directory of the executable.
		Char buffer[_MAX_PATH];
		GetModuleFileName(NULL, buffer, sizeof(buffer));
		if (Char *pEnd = strrchr(buffer, '\\'))
		{
			*pEnd = 0;
		}
		::SetCurrentDirectory(buffer);

#ifdef RTS_DEBUG
		// Turn on Memory heap tracking.
		int tmpFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
		tmpFlag |= (_CRTDBG_LEAK_CHECK_DF | _CRTDBG_ALLOC_MEM_DF);
		tmpFlag &= ~_CRTDBG_CHECK_CRT_DF;
		_CrtSetDbgFlag(tmpFlag);
#endif

		// Parse common startup switches (-headless, -replay, -jobs, ...).
		CommandLine::parseCommandLineForStartup();

		// Force headless mode for this tool.
		TheWritableGlobalData->m_headless = TRUE;
		TheWritableGlobalData->m_playIntro = FALSE;
		TheWritableGlobalData->m_afterIntro = TRUE;
		TheWritableGlobalData->m_playSizzle = FALSE;

		// Provide the instance handle for GameEngine / COM module.
		ApplicationHInstance = GetModuleHandle(NULL);

		// Set up version info (used by networking / CRC helpers).
		TheVersion = NEW Version;
		TheVersion->setVersion(
		    VERSION_MAJOR,
		    VERSION_MINOR,
		    VERSION_BUILDNUM,
		    VERSION_LOCALBUILDNUM,
		    AsciiString(VERSION_BUILDUSER),
		    AsciiString(VERSION_BUILDLOC),
		    AsciiString(__TIME__),
		    AsciiString(__DATE__));

		// Ensure we are the only Zero Hour instance for this profile.
		if (!rts::ClientInstance::initialize())
		{
			delete TheVersion;
			TheVersion = NULL;
			shutdownMemoryManager();
			return exitcode;
		}

		// Run the main game loop / replay simulation in headless modea.
		printf("started.");
		exitcode = GameMain();

		delete TheVersion;
		TheVersion = NULL;

#ifdef MEMORYPOOL_DEBUG
		TheMemoryPoolFactory->debugMemoryReport(REPORT_POOLINFO | REPORT_POOL_OVERFLOW | REPORT_SIMPLE_LEAKS, 0, 0);
#endif
#if defined(RTS_DEBUG)
		TheMemoryPoolFactory->memoryPoolUsageReport("AAAMemStats");
#endif

		shutdownMemoryManager();
	}
	catch (...)
	{
		// Unhandled exceptions are reported via UnHandledExceptionFilter.
	}

	TheUnicodeStringCriticalSection = NULL;
	TheDmaCriticalSection = NULL;
	TheMemoryPoolCriticalSection = NULL;

	return exitcode;
}

