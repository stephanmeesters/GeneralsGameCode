#include <afxwin.h>
#include <afxcmn.h>
#include <tchar.h>
#include <vector>

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

static void ApplySimulatedPipeUpdate(GameStateSnapshot &state)
{
    // Simulate receiving a named-pipe delta by tweaking one property and adding another.
    if (!state.categories.empty() && !state.categories[0].objects.empty())
    {
        GameObject &obj = state.categories[0].objects[0];
        if (!obj.properties.empty())
        {
            obj.properties[0].value = _T("950"); // pretend health reduced
        }
        obj.properties.push_back({_T("LastUpdated"), _T("PipeEvent#1")});
    }
}

class CGameStateToolFrame : public CFrameWnd
{
public:
    CGameStateToolFrame()
    {
        Create(nullptr, _T("GameStateTool - Hello MFC"));
        m_state = CreateStubGameState();
    }

protected:
    afx_msg void OnPaint();
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnSimulatePipeUpdate();
    DECLARE_MESSAGE_MAP()

private:
    void RenderState();

    static constexpr UINT kSimulateButtonId = 1001;
    CButton m_simulateButton;
    CTreeCtrl m_stateTree;
    GameStateSnapshot m_state;
};

BEGIN_MESSAGE_MAP(CGameStateToolFrame, CFrameWnd)
ON_WM_PAINT()
ON_WM_CREATE()
ON_WM_SIZE()
ON_BN_CLICKED(kSimulateButtonId, OnSimulatePipeUpdate)
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
    ApplySimulatedPipeUpdate(m_state);
    RenderState();
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
        m_pMainWnd = frame;
        frame->ShowWindow(SW_SHOW);
        frame->UpdateWindow();
        return TRUE;
    }
};

CGameStateToolApp theApp;

#pragma warning(pop)
