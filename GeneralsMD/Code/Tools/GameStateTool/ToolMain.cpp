#include <afxwin.h>
#include <afxcmn.h>
#include <tchar.h>
#include <vector>
#include <windows.h>
#include <mutex>
#include <thread>
#include <csignal>

static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType);
static void SignalHandler(int sig);
static void InstallSignalHandlers();
static class CGameStateToolFrame *g_mainFrame = nullptr;

#pragma warning(push)
#pragma warning(disable : 4996) // Match legacy MFC init used elsewhere (Enable3dControls).

struct Property
{
    CString name;
    CString value;
};

struct GameObject
{
    CString name;
    std::vector<Property> properties;
};

struct Category
{
    CString name;
    std::vector<GameObject> objects;
};

struct GameStateSnapshot
{
    std::vector<Category> categories;
};

static GameStateSnapshot CreateStubGameState()
{
    GameStateSnapshot state;

    Category defenses;
    defenses.name = _T("Defenses");
    defenses.objects.push_back(GameObject{
        _T("Patriot Missile"),
        {
            {_T("Health"), _T("1000")},
            {_T("Ammo"), _T("12")},
            {_T("Owner"), _T("USA")},
        },
    });
    defenses.objects.push_back(GameObject{
        _T("Gattling Cannon"),
        {
            {_T("Health"), _T("900")},
            {_T("Ammo"), _T("Infinite")},
            {_T("Owner"), _T("China")},
        },
    });

    Category economy;
    economy.name = _T("Economy");
    economy.objects.push_back(GameObject{
        _T("Supply Center"),
        {
            {_T("Health"), _T("1500")},
            {_T("Collectors"), _T("2")},
            {_T("Income/sec"), _T("120")},
        },
    });

    state.categories.push_back(defenses);
    state.categories.push_back(economy);



















    return state;
}

class CGameStateToolFrame : public CFrameWnd
{
public:
    CGameStateToolFrame()
    {
        Create(nullptr, _T("GameStateTool - Hello MFC"));
        m_state = CreateStubGameState();
        m_pipeStopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        m_pipeThread = std::thread(&CGameStateToolFrame::PipeThreadProc, this);
    }
    ~CGameStateToolFrame() override = default;
    void RequestShutdownFromSignal();

protected:
    afx_msg void OnPaint();
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnSimulatePipeUpdate();
    afx_msg void OnDestroy();
    afx_msg LRESULT OnPipeMessage(WPARAM wParam, LPARAM lParam);
    DECLARE_MESSAGE_MAP()

private:
    void RenderState();
    void AppendPipeMessage(const CString &message);
    void PostPipeMessageFromWorker(const char *message);
    static void PipeThreadProc(CGameStateToolFrame *self);
    void ShutdownPipeThread();

    static constexpr UINT kSimulateButtonId = 1001;
    static constexpr UINT kPipeMessageId = WM_APP + 42;
    CButton m_simulateButton;
    CTreeCtrl m_stateTree;
    GameStateSnapshot m_state;
    HANDLE m_pipeStopEvent = nullptr;
    std::thread m_pipeThread;
    std::mutex m_pipeMutex;
    CString m_lastPipeMessage;
};

BEGIN_MESSAGE_MAP(CGameStateToolFrame, CFrameWnd)
ON_WM_PAINT()
ON_WM_CREATE()
ON_WM_SIZE()
ON_BN_CLICKED(kSimulateButtonId, OnSimulatePipeUpdate)
ON_WM_DESTROY()
ON_MESSAGE(kPipeMessageId, OnPipeMessage)
END_MESSAGE_MAP()

void CGameStateToolFrame::OnPaint()
{
    CPaintDC dc(this);
    CRect rect;
    GetClientRect(&rect);
    dc.DrawText(_T("GameStateTool (MFC) - Named pipe stub view"), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

int CGameStateToolFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    CRect client;
    GetClientRect(&client);
    const int buttonHeight = 28;
    const int padding = 8;

    m_simulateButton.Create(
        _T("Simulate Pipe Update"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(padding, padding, client.Width() - padding, padding + buttonHeight),
        this,
        kSimulateButtonId);

    m_stateTree.Create(
        WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
        CRect(
            padding,
            padding * 2 + buttonHeight,
            client.Width() - padding,
            client.Height() - padding),
        this,
        0);

    RenderState();
    return 0;
}

void CGameStateToolFrame::OnSize(UINT nType, int cx, int cy)
{
    CFrameWnd::OnSize(nType, cx, cy);
    const int buttonHeight = 28;
    const int padding = 8;
    if (m_simulateButton.GetSafeHwnd())
    {
        m_simulateButton.MoveWindow(padding, padding, cx - padding * 2, buttonHeight);
    }
    if (m_stateTree.GetSafeHwnd())
    {
        m_stateTree.MoveWindow(
            padding,
            padding * 2 + buttonHeight,
            cx - padding * 2,
            cy - (padding * 3 + buttonHeight));
    }
}

void CGameStateToolFrame::OnSimulatePipeUpdate()
{
    AppendPipeMessage(_T("Manual test trigger"));
}

void CGameStateToolFrame::RenderState()
{
    m_stateTree.DeleteAllItems();

    for (const Category &category : m_state.categories)
    {
        HTREEITEM hCat = m_stateTree.InsertItem(category.name);
        for (const GameObject &obj : category.objects)
        {
            CString objLine;
            objLine.Format(_T("%s"), obj.name.GetString());
            HTREEITEM hObj = m_stateTree.InsertItem(objLine, hCat);
            for (const Property &prop : obj.properties)
            {
                CString propLine;
                propLine.Format(_T("%s: %s"), prop.name.GetString(), prop.value.GetString());
                m_stateTree.InsertItem(propLine, hObj);
            }
            m_stateTree.Expand(hObj, TVE_EXPAND);
        }
        m_stateTree.Expand(hCat, TVE_EXPAND);
    }
}

void CGameStateToolFrame::AppendPipeMessage(const CString &message)
{
    Category pipeCategory;
    pipeCategory.name = _T("Pipe Message");
    pipeCategory.objects.push_back(GameObject{
        _T("Payload"),
        {
            {_T("Data"), message},
        },
    });

    m_state.categories.push_back(pipeCategory);
    RenderState();
}

void CGameStateToolFrame::PostPipeMessageFromWorker(const char *message)
{
    CString msg;
    msg.Format(_T("%hs"), message);
    {
        std::lock_guard<std::mutex> lock(m_pipeMutex);
        m_lastPipeMessage = msg;
    }
    PostMessage(kPipeMessageId);
}

void CGameStateToolFrame::PipeThreadProc(CGameStateToolFrame *self)
{
    constexpr wchar_t kPipeName[] = L"\\\\.\\pipe\\my_pipe";

    while (WaitForSingleObject(self->m_pipeStopEvent, 0) == WAIT_TIMEOUT)
    {
        HANDLE pipe = CreateNamedPipeW(
            kPipeName,
            PIPE_ACCESS_INBOUND,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1,
            512,
            512,
            0,
            nullptr);

        if (pipe == INVALID_HANDLE_VALUE)
        {
            Sleep(200);
            continue;
        }

        BOOL connected = ConnectNamedPipe(pipe, nullptr);
        if (!connected && GetLastError() != ERROR_PIPE_CONNECTED)
        {
            CloseHandle(pipe);
            continue;
        }

        char buffer[256] = {};
        DWORD bytesRead = 0;
        BOOL readOk = ReadFile(pipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr);
        if (readOk && bytesRead > 0)
        {
            buffer[bytesRead] = '\0';
            self->PostPipeMessageFromWorker(buffer);
        }

        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
}

void CGameStateToolFrame::ShutdownPipeThread()
{
    const bool wasRunning = m_pipeThread.joinable();
    if (m_pipeStopEvent)
    {
        SetEvent(m_pipeStopEvent);
    }

    if (wasRunning)
    {
        // Nudge the listener out of ConnectNamedPipe so it can exit.
        CreateFileA(
            "\\\\.\\pipe\\my_pipe",
            GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        m_pipeThread.join();
    }

    if (m_pipeStopEvent)
    {
        CloseHandle(m_pipeStopEvent);
        m_pipeStopEvent = nullptr;
    }
}

void CGameStateToolFrame::RequestShutdownFromSignal()
{
    ShutdownPipeThread();
    if (GetSafeHwnd())
    {
        PostMessage(WM_CLOSE);
    }
    else
    {
        PostQuitMessage(0);
    }
}

LRESULT CGameStateToolFrame::OnPipeMessage(WPARAM, LPARAM)
{
    CString msg;
    {
        std::lock_guard<std::mutex> lock(m_pipeMutex);
        msg = m_lastPipeMessage;
    }
    if (!msg.IsEmpty())
    {
        AppendPipeMessage(msg);
    }
    return 0;
}

void CGameStateToolFrame::OnDestroy()
{
    ShutdownPipeThread();
    g_mainFrame = nullptr;
    CFrameWnd::OnDestroy();
}

class CGameStateToolApp : public CWinApp
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

        CGameStateToolFrame *frame = new CGameStateToolFrame();
        g_mainFrame = frame;
        InstallSignalHandlers();
        m_pMainWnd = frame;
        frame->ShowWindow(SW_SHOW);
        frame->UpdateWindow();
        return TRUE;
    }
};

CGameStateToolApp theApp;

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
