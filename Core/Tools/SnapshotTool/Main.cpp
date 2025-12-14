#include <afxcmn.h>
#include <afxwin.h>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <windows.h>

#include "Logic.h"
#include "UI.h"

static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType);
static void SignalHandler(int sig);
static void InstallSignalHandlers();

#pragma warning(push)
#pragma warning(disable : 4996) // Match legacy MFC init used elsewhere (Enable3dControls).

// Globals expected by linked engine code
HINSTANCE ApplicationHInstance = nullptr;
HWND ApplicationHWnd = nullptr;
const Char *g_strFile = "data\\Generals.str";
const Char *g_csfFile = "data\\%s\\Generals.csf";
extern "C" char *TheCurrentIgnoreCrashPtr = nullptr;
extern "C" void DebugCrash(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
    va_end(args);
}

CSnapshotToolUI *g_mainFrame = nullptr;

class CSnapshotToolApp : public CWinApp
{
public:
    BOOL InitInstance() override
    {
        CWinApp::InitInstance();

        INITCOMMONCONTROLSEX icc = {};
        icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icc.dwICC = ICC_TREEVIEW_CLASSES;
        InitCommonControlsEx(&icc);

#ifdef _AFXDLL
        Enable3dControls(); // Match WorldBuilder/wdump initialization style.
#else
        Enable3dControlsStatic();
#endif

        CSnapshotToolUI *frame = new CSnapshotToolUI(m_logic);
        g_mainFrame = frame;
        InstallSignalHandlers();
        m_pMainWnd = frame;
        frame->ShowWindow(SW_SHOW);
        frame->UpdateWindow();
        return TRUE;
    }

private:
    GameStateLogic m_logic;
};

CSnapshotToolApp theApp;

static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType)
{
    switch (ctrlType)
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        if (g_mainFrame)
        {
            g_mainFrame->RequestShutdownFromSignal();
            return TRUE;
        }
        break;
    default:
        break;
    }
    return FALSE;
}

static void SignalHandler(int)
{
    if (g_mainFrame)
    {
        g_mainFrame->RequestShutdownFromSignal();
    }
}

static void InstallSignalHandlers()
{
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    signal(SIGINT, SignalHandler);
    signal(SIGABRT, SignalHandler);
#ifdef SIGTERM
    signal(SIGTERM, SignalHandler);
#endif
}

#pragma warning(pop)
