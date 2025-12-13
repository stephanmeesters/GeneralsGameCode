#include <afxwin.h>
#include <afxcmn.h>
#include <tchar.h>
#include <vector>
#include <windows.h>
#include <csignal>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <algorithm>
#include <afxdlgs.h>

static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType);
static void SignalHandler(int sig);
static void InstallSignalHandlers();
static class CGameStateToolFrame *g_mainFrame = nullptr;

#pragma warning(push)
#pragma warning(disable : 4996) // Match legacy MFC init used elsewhere (Enable3dControls).

#include "Lib/BaseType.h"
#include "Common/AsciiString.h"
#include "Common/UnicodeString.h"
#include "Common/XferLoadBuffer.h"
#include "GeneratedSnapshotSchema.h"

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
    std::vector<GameObject> objects;
};

static GameStateSnapshot CreateStubGameState()
{
    return GameStateSnapshot();
}


class CGameStateToolFrame : public CFrameWnd
{
public:
    CGameStateToolFrame()
    {
        Create(nullptr, _T("GameStateTool"));
        m_state = CreateStubGameState();
        BuildMenu();
    }
    ~CGameStateToolFrame() override = default;
    void RequestShutdownFromSignal();

protected:
    afx_msg void OnPaint();
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnDestroy();
    afx_msg void OnFileOpen();
    afx_msg void OnFileExit();
    DECLARE_MESSAGE_MAP()

private:
    void BuildMenu();
    void LoadSnapshotFromFile(const CString &path);
    std::string SerializeSnapshot(XferLoadBuffer &xfer, SnapshotSchemaView schema);
    void BuildStateFromSerialized(GameStateSnapshot &target, const CString &blockName, const std::string &serialized);
    void RenderState();

    static constexpr UINT kCmdFileOpen = 2001;
    static constexpr UINT kCmdFileExit = 2002;
    CMenu m_menuBar;
    CMenu m_fileMenu;
    CTreeCtrl m_stateTree;
    GameStateSnapshot m_state;
};

BEGIN_MESSAGE_MAP(CGameStateToolFrame, CFrameWnd)
ON_WM_PAINT()
ON_WM_CREATE()
ON_WM_SIZE()
ON_WM_DESTROY()
ON_COMMAND(kCmdFileOpen, OnFileOpen)
ON_COMMAND(kCmdFileExit, OnFileExit)
END_MESSAGE_MAP()

void CGameStateToolFrame::OnPaint()
{
    CPaintDC dc(this);
    CRect rect;
    GetClientRect(&rect);
    dc.DrawText(_T("Use File -> Open Save... to load a snapshot"), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

int CGameStateToolFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    CRect client;
    GetClientRect(&client);
    const int padding = 8;

    m_stateTree.Create(
        WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
        CRect(
            padding,
            padding,
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
    const int padding = 8;
    if (m_stateTree.GetSafeHwnd())
    {
        m_stateTree.MoveWindow(
            padding,
            padding,
            cx - padding * 2,
            cy - padding * 2);
    }
}

void CGameStateToolFrame::RenderState()
{
    m_stateTree.DeleteAllItems();

    for (const GameObject &obj : m_state.objects)
    {
        CString objLine;
        objLine.Format(_T("%s"), obj.name.GetString());
        HTREEITEM hObj = m_stateTree.InsertItem(objLine);
        for (const Property &prop : obj.properties)
        {
            CString propLine;
            propLine.Format(_T("%s: %s"), prop.name.GetString(), prop.value.GetString());
            m_stateTree.InsertItem(propLine, hObj);
        }
        m_stateTree.Expand(hObj, TVE_EXPAND);
    }
}

void CGameStateToolFrame::BuildMenu()
{
    m_menuBar.CreateMenu();
    m_fileMenu.CreatePopupMenu();
    m_fileMenu.AppendMenu(MF_STRING, kCmdFileOpen, _T("Open Save..."));
    m_fileMenu.AppendMenu(MF_SEPARATOR);
    m_fileMenu.AppendMenu(MF_STRING, kCmdFileExit, _T("Exit"));
    m_menuBar.AppendMenu(MF_POPUP, (UINT_PTR)m_fileMenu.m_hMenu, _T("File"));
    SetMenu(&m_menuBar);
}

std::string CGameStateToolFrame::SerializeSnapshot(XferLoadBuffer &xfer, SnapshotSchemaView schema)
{
    std::ostringstream out;
    bool first = true;
    for (const SnapshotSchemaField &field : schema)
    {
        if (!first)
        {
            out << "\n";
        }
        first = false;

        out << field.name << ": ";
        std::string_view type(field.type);
        if (type == "UnsignedByte")
        {
            UnsignedByte value{};
            xfer.xferUnsignedByte(&value);
            out << static_cast<unsigned>(value);
        }
        else if (type == "Byte")
        {
            Byte value{};
            xfer.xferByte(&value);
            out << static_cast<int>(value);
        }
        else if (type == "Bool")
        {
            Bool value{};
            xfer.xferBool(&value);
            out << (value ? "true" : "false");
        }
        else if (type == "Short")
        {
            Short value{};
            xfer.xferShort(&value);
            out << value;
        }
        else if (type == "UnsignedShort")
        {
            UnsignedShort value{};
            xfer.xferUnsignedShort(&value);
            out << value;
        }
        else if (type == "Int")
        {
            Int value{};
            xfer.xferInt(&value);
            out << value;
        }
        else if (type == "UnsignedInt")
        {
            UnsignedInt value{};
            xfer.xferUnsignedInt(&value);
            out << value;
        }
        else if (type == "Int64")
        {
            Int64 value{};
            xfer.xferInt64(&value);
            out << static_cast<long long>(value);
        }
        else if (type == "Real")
        {
            Real value{};
            xfer.xferReal(&value);
            out << value;
        }
        else if (type == "AsciiString")
        {
            AsciiString value;
            xfer.xferAsciiString(&value);
            out << value.str();
        }
        else if (type == "UnicodeString")
        {
            UnicodeString unicodeValue;
            xfer.xferUnicodeString(&unicodeValue);
            AsciiString asciiValue;
            asciiValue.translate(unicodeValue);
            out << asciiValue.str();
        }
        else if (type == "BlockSize")
        {
            Int blockSize = xfer.beginBlock();
            out << blockSize;
        }
        else if (type == "EndBlock")
        {
            xfer.endBlock();
            out << "<end-block>";
        }
        else
        {
            out << "<unknown>";
        }
    }
    return out.str();
}

void CGameStateToolFrame::BuildStateFromSerialized(GameStateSnapshot &target, const CString &blockName, const std::string &serialized)
{
    GameObject object;
    object.name = blockName;

    std::istringstream lines(serialized);
    std::string line;
    while (std::getline(lines, line))
    {
        CStringA lineA(line.c_str());
        int colon = lineA.Find(':');
        if (colon == -1)
        {
            continue;
        }
        CString name = CString(lineA.Left(colon)).Trim();
        CString value = CString(lineA.Mid(colon + 1)).Trim();
        object.properties.push_back(Property{name, value});
    }

    target.objects.push_back(object);
}

void CGameStateToolFrame::LoadSnapshotFromFile(const CString &path)
{
    CStringA pathA(path);
    std::ifstream input(pathA.GetString(), std::ios::binary);
    if (!input)
    {
        AfxMessageBox(_T("Failed to open file."));
        return;
    }

    std::vector<Byte> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();
    if (bytes.empty())
    {
        AfxMessageBox(_T("File is empty."));
        return;
    }

    static const Char *SAVE_FILE_EOF = "SG_EOF";
    const std::string chunkTag = "CHUNK_";
    std::vector<size_t> chunkOffsets;
    for (size_t i = 0; i + chunkTag.size() <= bytes.size(); ++i)
    {
        if (std::equal(chunkTag.begin(), chunkTag.end(), bytes.begin() + static_cast<long>(i)))
        {
            chunkOffsets.push_back(i > 0 ? i - 1 : i);
        }
    }

    GameStateSnapshot loaded;
    for (size_t offset : chunkOffsets)
    {
        XferLoadBuffer xfer;
        xfer.open(_T("save"), bytes);
        xfer.skip(static_cast<Int>(offset));

        AsciiString token;
        xfer.xferAsciiString(&token);
        if (token.isEmpty())
        {
            xfer.close();
            continue;
        }

        int blockSize = xfer.beginBlock();

        CString blockName = token.str();
        auto it = SnapshotSchema::SNAPSHOT_BLOCK_SCHEMAS.find(token.str());
        if (it != SnapshotSchema::SNAPSHOT_BLOCK_SCHEMAS.end())
        {
            std::string serialized = SerializeSnapshot(xfer, it->second);
            BuildStateFromSerialized(loaded, CString(it->first.data()), serialized);
        }
        else
        {
            xfer.skip(blockSize);
        }
        // xfer.endBlock(); // not needed for this targeted parse
        xfer.close();

        if (token.compareNoCase(SAVE_FILE_EOF) == 0)
        {
            break;
        }
    }

    m_state = loaded;
    RenderState();
}

void CGameStateToolFrame::OnFileOpen()
{
    CFileDialog dlg(TRUE, _T("sav"), nullptr, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, _T("Save Files (*.sav)|*.sav|All Files (*.*)|*.*||"));
    if (dlg.DoModal() == IDOK)
    {
        m_state.objects.clear();
        LoadSnapshotFromFile(dlg.GetPathName());
    }
}

void CGameStateToolFrame::OnFileExit()
{
    PostMessage(WM_CLOSE);
}

void CGameStateToolFrame::RequestShutdownFromSignal()
{
    if (GetSafeHwnd())
    {
        PostMessage(WM_CLOSE);
    }
    else
    {
        PostQuitMessage(0);
    }
}

void CGameStateToolFrame::OnDestroy()
{
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
