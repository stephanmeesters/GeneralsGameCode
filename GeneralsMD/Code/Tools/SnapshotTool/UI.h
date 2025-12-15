#pragma once

#include <afxcmn.h>
#include <afxwin.h>
#include <vector>

#include "Logic.h"

namespace SnapshotTool {
class SnapshotPipeServer;

class CSnapshotToolUI : public CFrameWnd {
public:
    explicit CSnapshotToolUI(GameStateLogic &logic);
    ~CSnapshotToolUI() override = default;

    static CSnapshotToolUI *CreateInstance(GameStateLogic &logic, SnapshotPipeServer *pipe);
    static CSnapshotToolUI *Get();

    void RequestShutdownFromSignal();

protected:
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnDestroy();
    afx_msg void OnLoadButton();
    afx_msg void OnLoadReplayButton();
    afx_msg void OnSaveButton();
    afx_msg void OnRecordButton();
    afx_msg void OnClearButton();
    afx_msg void OnCategoryChanged();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
    DECLARE_MESSAGE_MAP()

private:
    void UpdateFromLogic();
    void RenderState();
    void RenderSelectedCategory();
    void ShowErrorMessage(const CString &message);
    void UpdateRecordButtonImage();
    void LayoutControls(int cx, int cy);
    static CString RemoveChunkPrefix(const CString &name);

    static constexpr UINT kBtnLoad = 2001;
    static constexpr UINT kBtnSave = 2002;
    static constexpr UINT kBtnRecord = 2003;
    static constexpr UINT kBtnClear = 2004;
    static constexpr UINT kBtnLoadReplay = 2005;
    static constexpr UINT kCategoryList = 3001;
    static constexpr UINT kCategoryDetails = 3002;
    static constexpr UINT kStateList = 3003;
    static constexpr int kButtonHeight = 26;
    static constexpr int kButtonWidth = 160;
    static constexpr int kCategoryWidth = 200;
    static constexpr int kPadding = 8;

    CButton m_loadButton;
    CButton m_loadReplayButton;
    CButton m_saveButton;
    CButton m_recordButton;
    CButton m_clearButton;
    CListBox m_categoryList;
    bool m_isRecording = false;
    CString m_selectedCategoryName;
    std::vector<CString> m_categoryNames;
    GameStateSnapshot m_displayState;
    CListCtrl m_stateList;
    CEdit m_categoryDetails;
    GameStateLogic &m_logic;
    SnapshotPipeServer *m_pipe = nullptr;

    afx_msg void OnStateListCustomDraw(NMHDR *pNMHDR, LRESULT *pResult);
};
} // namespace SnapshotTool
