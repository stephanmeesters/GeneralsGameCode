#pragma once

#include <afxcmn.h>
#include <afxwin.h>

#include "Logic.h"

class CGameStateToolUI : public CFrameWnd
{
public:
    explicit CGameStateToolUI(GameStateLogic &logic);
    ~CGameStateToolUI() override = default;

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
    void RenderState();
    void ShowErrorMessage(const CString &message);

    static constexpr UINT kCmdFileOpen = 2001;
    static constexpr UINT kCmdFileExit = 2002;

    CMenu m_menuBar;
    CMenu m_fileMenu;
    CTreeCtrl m_stateTree;
    GameStateLogic &m_logic;
};
