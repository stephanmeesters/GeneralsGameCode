#include "UI.h"

#include <afxdlgs.h>
#include <tchar.h>

// Shared pointer used by signal handlers in Main.cpp to request shutdown.
extern CGameStateToolUI *g_mainFrame;

BEGIN_MESSAGE_MAP(CGameStateToolUI, CFrameWnd)
ON_WM_PAINT()
ON_WM_CREATE()
ON_WM_SIZE()
ON_WM_DESTROY()
ON_COMMAND(CGameStateToolUI::kCmdFileOpen, OnFileOpen)
ON_COMMAND(CGameStateToolUI::kCmdFileExit, OnFileExit)
END_MESSAGE_MAP()

CGameStateToolUI::CGameStateToolUI(GameStateLogic &logic)
    : m_logic(logic)
{
    Create(nullptr, _T("GameStateTool"));
    BuildMenu();
}

void CGameStateToolUI::OnPaint()
{
    CPaintDC dc(this);
    CRect rect;
    GetClientRect(&rect);
    dc.DrawText(_T("Use File -> Open Save... to load a snapshot"), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

int CGameStateToolUI::OnCreate(LPCREATESTRUCT lpCreateStruct)
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

void CGameStateToolUI::OnSize(UINT nType, int cx, int cy)
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

void CGameStateToolUI::RenderState()
{
    m_stateTree.DeleteAllItems();

    for (const GameObject &obj : m_logic.GetState().objects)
    {
        HTREEITEM hObj = m_stateTree.InsertItem(obj.name);
        for (const Property &prop : obj.properties)
        {
            CString propLine;
            propLine.Format(_T("%s: %s"), prop.name.GetString(), prop.value.GetString());
            m_stateTree.InsertItem(propLine, hObj);
        }
        m_stateTree.Expand(hObj, TVE_EXPAND);
    }
}

void CGameStateToolUI::BuildMenu()
{
    m_menuBar.CreateMenu();
    m_fileMenu.CreatePopupMenu();
    m_fileMenu.AppendMenu(MF_STRING, kCmdFileOpen, _T("Open Save..."));
    m_fileMenu.AppendMenu(MF_SEPARATOR);
    m_fileMenu.AppendMenu(MF_STRING, kCmdFileExit, _T("Exit"));
    m_menuBar.AppendMenu(MF_POPUP, (UINT_PTR)m_fileMenu.m_hMenu, _T("File"));
    SetMenu(&m_menuBar);
}

void CGameStateToolUI::OnFileOpen()
{
    CFileDialog dlg(TRUE, _T("sav"), nullptr, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, _T("Save Files (*.sav)|*.sav|All Files (*.*)|*.*||"));
    if (dlg.DoModal() == IDOK)
    {
        CString errorMessage;
        if (!m_logic.LoadSnapshotFromFile(dlg.GetPathName(), errorMessage))
        {
            ShowErrorMessage(errorMessage);
            RenderState();
            return;
        }

        RenderState();
    }
}

void CGameStateToolUI::OnFileExit()
{
    PostMessage(WM_CLOSE);
}

void CGameStateToolUI::RequestShutdownFromSignal()
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

void CGameStateToolUI::OnDestroy()
{
    g_mainFrame = nullptr;
    CFrameWnd::OnDestroy();
}

void CGameStateToolUI::ShowErrorMessage(const CString &message)
{
    AfxMessageBox(message);
}
