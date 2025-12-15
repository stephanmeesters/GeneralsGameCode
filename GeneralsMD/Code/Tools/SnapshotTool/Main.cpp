#include <afxcmn.h>
#include <afxwin.h>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <windows.h>
#include <memory>

#include "Logic.h"
#include "UI.h"
#include "Pipe.h"

using namespace SnapshotTool;

static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType);

static void SignalHandler(int sig);

static void InstallSignalHandlers();

#pragma warning(push)
#pragma warning(disable : 4996)

// Globals expected by linked engine code
HINSTANCE ApplicationHInstance = nullptr;
HWND ApplicationHWnd = nullptr;
const Char *g_strFile = "data\\Generals.str";
const Char *g_csfFile = "data\\%s\\Generals.csf";
extern "C" char *TheCurrentIgnoreCrashPtr = nullptr;

extern "C" void DebugCrash(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
    va_end(args);
}

class CSnapshotToolApp : public CWinApp {
public:
    ~CSnapshotToolApp() override {
        if (m_pipe) {
            m_pipe->Stop();
        }
        m_logic.Stop();
    }

    BOOL InitInstance() override {
        CWinApp::InitInstance();

        INITCOMMONCONTROLSEX icc = {};
        icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icc.dwICC = ICC_TREEVIEW_CLASSES;
        InitCommonControlsEx(&icc);

        m_pipe = std::make_unique<SnapshotPipeServer>(m_logic);
        m_pipe->Start(false);
        CSnapshotToolUI *frame = CSnapshotToolUI::CreateInstance(m_logic, m_pipe.get());
        m_logic.Start();
        InstallSignalHandlers();
        m_pMainWnd = frame;
        frame->ShowWindow(SW_SHOW);
        frame->UpdateWindow();
        return TRUE;
    }

private:
    GameStateLogic m_logic;
    std::unique_ptr<SnapshotPipeServer> m_pipe;
};

CSnapshotToolApp theApp;

static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    switch (ctrlType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            if (CSnapshotToolUI *frame = CSnapshotToolUI::Get()) {
                frame->RequestShutdownFromSignal();
                return TRUE;
            }
            break;
        default:
            break;
    }
    return FALSE;
}

static void SignalHandler(int) {
    if (CSnapshotToolUI *frame = CSnapshotToolUI::Get()) {
        frame->RequestShutdownFromSignal();
    }
}

static void InstallSignalHandlers() {
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    signal(SIGINT, SignalHandler);
    signal(SIGABRT, SignalHandler);
#ifdef SIGTERM
    signal(SIGTERM, SignalHandler);
#endif
}

#pragma warning(pop)
