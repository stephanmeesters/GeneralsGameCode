#include "UI.h"

#include <afxdlgs.h>
#include <tchar.h>
#include <ShlObj.h>
#include <KnownFolders.h>
#include <windows.h>
#include <algorithm>
#include <vector>

#include "Pipe.h"

namespace SnapshotTool  {
        CSnapshotToolUI *g_mainFrame = nullptr;

    BEGIN_MESSAGE_MAP(CSnapshotToolUI, CFrameWnd)
        ON_WM_CREATE()
        ON_WM_SIZE()
        ON_WM_DESTROY()
        ON_BN_CLICKED(CSnapshotToolUI::kBtnLoad, OnLoadButton)
        ON_BN_CLICKED(CSnapshotToolUI::kBtnLoadReplay, OnLoadReplayButton)
        ON_BN_CLICKED(CSnapshotToolUI::kBtnSave, OnSaveButton)
        ON_BN_CLICKED(CSnapshotToolUI::kBtnRecord, OnRecordButton)
        ON_BN_CLICKED(CSnapshotToolUI::kBtnClear, OnClearButton)
        ON_LBN_SELCHANGE(CSnapshotToolUI::kCategoryList, OnCategoryChanged)
        ON_WM_TIMER()
        ON_WM_DRAWITEM()
        ON_NOTIFY(NM_CUSTOMDRAW, CSnapshotToolUI::kStateList, OnStateListCustomDraw)
END_MESSAGE_MAP()

    CSnapshotToolUI::CSnapshotToolUI(GameStateLogic &logic)
        : m_logic(logic) {
        g_mainFrame = this;
    }

    int CSnapshotToolUI::OnCreate(LPCREATESTRUCT lpCreateStruct) {
        if (CFrameWnd::OnCreate(lpCreateStruct) == -1) {
            return -1;
        }

        m_loadButton.Create(
            _T("Load Savegame"),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            CRect(kPadding, kPadding, kPadding + kButtonWidth, kPadding + kButtonHeight),
            this,
            kBtnLoad);

        m_loadReplayButton.Create(
            _T("Load Replay"),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            CRect(0, 0, 0, 0),
            this,
            kBtnLoadReplay);

        m_saveButton.Create(
            _T("Export State"),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            CRect(kPadding + kButtonWidth + kPadding, kPadding, kPadding + kButtonWidth * 2 + kPadding,
                  kPadding + kButtonHeight),
            this,
            kBtnSave);

        m_clearButton.Create(
            _T("Clear State"),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            CRect(0, 0, 0, 0),
            this,
            kBtnClear);

        m_recordButton.Create(
            _T(""),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
            CRect(0, 0, kButtonWidth, kButtonHeight),
            this,
            kBtnRecord);
        UpdateRecordButtonImage();

        m_categoryList.Create(
            WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY | WS_VSCROLL,
            CRect(0, 0, 0, 0),
            this,
            kCategoryList);

        m_stateList.Create(
            WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            CRect(0, 0, 0, 0),
            this,
            kStateList);
        m_stateList.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        m_stateList.InsertColumn(0, _T("Name"), LVCFMT_LEFT, 120);
        m_stateList.InsertColumn(1, _T("Value"), LVCFMT_LEFT, 220);
        m_stateList.InsertColumn(2, _T("Type"), LVCFMT_LEFT, 120);

        m_categoryDetails.Create(
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
            CRect(0, 0, 0, 0),
            this,
            kCategoryDetails);
        m_categoryDetails.SetFont(GetFont());

        SetTimer(1, 200, nullptr);
        RenderState();

        CRect client;
        GetClientRect(&client);
        LayoutControls(client.Width(), client.Height());
        return 0;
    }

    void CSnapshotToolUI::OnSize(UINT nType, int cx, int cy) {
        CFrameWnd::OnSize(nType, cx, cy);
        LayoutControls(cx, cy);
    }

    void CSnapshotToolUI::LayoutControls(int cx, int cy) {
        const int treeTop = kPadding * 2 + kButtonHeight;
        const int availableHeight = cy - treeTop - kPadding;
        const int rightLeft = kPadding * 2 + kCategoryWidth;
        const int rightWidth = cx - rightLeft - kPadding;
        const int detailsHeight = (std::max)(0, availableHeight / 5);
        const int treeHeight = (std::max)(0, availableHeight - detailsHeight - kPadding);
        const int detailsTop = treeTop + treeHeight + kPadding;
        const int buttonTop = kPadding;
        const int leftButtonLeft = kPadding;
        const int leftButtonRight = leftButtonLeft + kButtonWidth;
        const int loadReplayLeft = leftButtonRight + kPadding;
        const int recordWidth = kButtonWidth;
        const int recordLeft = cx - kPadding - recordWidth;
        const int clearWidth = 120;
        const int saveWidth = kButtonWidth;
        const int leftClusterRight = loadReplayLeft + kButtonWidth;
        const int middleSpacing = kPadding;
        const int middleTotalWidth = saveWidth + middleSpacing + clearWidth;
        int middleLeft = (cx - middleTotalWidth) / 2;
        const int minMiddleLeft = leftClusterRight + kPadding;
        int maxMiddleLeft = recordLeft - kPadding - middleTotalWidth;
        if (maxMiddleLeft < minMiddleLeft) {
            maxMiddleLeft = minMiddleLeft;
        }
        if (middleLeft < minMiddleLeft) {
            middleLeft = minMiddleLeft;
        }
        if (middleLeft > maxMiddleLeft) {
            middleLeft = maxMiddleLeft;
        }
        // Center the export and clear buttons together, but keep them away from left and right clusters.
        const int saveLeft = middleLeft;
        const int clearLeft = middleLeft + saveWidth + middleSpacing;

        if (m_loadButton.GetSafeHwnd()) {
            m_loadButton.MoveWindow(
                leftButtonLeft,
                buttonTop,
                kButtonWidth,
                kButtonHeight);
        }

        if (m_loadReplayButton.GetSafeHwnd()) {
            m_loadReplayButton.MoveWindow(
                loadReplayLeft,
                buttonTop,
                kButtonWidth,
                kButtonHeight);
        }

        if (m_saveButton.GetSafeHwnd()) {
            m_saveButton.MoveWindow(
                saveLeft,
                buttonTop,
                saveWidth,
                kButtonHeight);
        }

        if (m_clearButton.GetSafeHwnd()) {
            m_clearButton.MoveWindow(
                clearLeft,
                buttonTop,
                clearWidth,
                kButtonHeight);
        }

        if (m_recordButton.GetSafeHwnd()) {
            m_recordButton.MoveWindow(
                recordLeft,
                buttonTop,
                recordWidth,
                kButtonHeight);
        }

        if (m_categoryList.GetSafeHwnd()) {
            m_categoryList.MoveWindow(
                kPadding,
                treeTop,
                kCategoryWidth,
                availableHeight);
        }

        if (m_stateList.GetSafeHwnd()) {
            m_stateList.MoveWindow(
                rightLeft,
                treeTop,
                rightWidth,
                treeHeight);
            const int typeWidth = (std::max)(80, rightWidth / 5);
            const int nameWidth = (std::max)(120, rightWidth / 3);
            int valueWidth = rightWidth - nameWidth - typeWidth;
            if (valueWidth < 80) {
                valueWidth = 80;
            }
            int finalTypeWidth = rightWidth - nameWidth - valueWidth;
            if (finalTypeWidth < 60) {
                finalTypeWidth = 60;
            }
            m_stateList.SetColumnWidth(0, nameWidth);
            m_stateList.SetColumnWidth(1, valueWidth);
            m_stateList.SetColumnWidth(2, finalTypeWidth);
        }

        if (m_categoryDetails.GetSafeHwnd()) {
            m_categoryDetails.MoveWindow(
                rightLeft,
                detailsTop,
                rightWidth,
                detailsHeight);
        }
    }

    void CSnapshotToolUI::RenderState() {
        if (!m_categoryList.GetSafeHwnd() || !m_stateList.GetSafeHwnd()) {
            return;
        }

        CString previousSelection = m_selectedCategoryName;
        if (previousSelection.IsEmpty()) {
            int curSel = m_categoryList.GetCurSel();
            if (curSel != LB_ERR && curSel >= 0 && static_cast<size_t>(curSel) < m_categoryNames.size()) {
                previousSelection = m_categoryNames[static_cast<size_t>(curSel)];
            }
        }

        m_categoryNames.clear();
        m_categoryList.ResetContent();

        for (const GameObject &obj: m_displayState.objects) {
            m_categoryNames.push_back(obj.name);
            CString displayName = RemoveChunkPrefix(obj.name);
            m_categoryList.AddString(displayName);
        }

        int selectedIndex = -1;
        if (!previousSelection.IsEmpty()) {
            for (size_t i = 0; i < m_categoryNames.size(); ++i) {
                if (m_categoryNames[i] == previousSelection) {
                    selectedIndex = static_cast<int>(i);
                    break;
                }
            }
        }

        if (selectedIndex == -1 && !m_categoryNames.empty()) {
            selectedIndex = 0;
        }

        if (selectedIndex != -1) {
            m_selectedCategoryName = m_categoryNames[static_cast<size_t>(selectedIndex)];
            m_categoryList.SetCurSel(selectedIndex);
        } else {
            m_selectedCategoryName.Empty();
            m_categoryList.SetCurSel(-1);
        }

        RenderSelectedCategory();
    }

    CString CSnapshotToolUI::RemoveChunkPrefix(const CString &name) {
        static const CString prefix = _T("CHUNK_");
        if (name.GetLength() >= prefix.GetLength() && name.Left(prefix.GetLength()).CompareNoCase(prefix) == 0) {
            return name.Mid(prefix.GetLength());
        }
        return name;
    }

    void CSnapshotToolUI::RenderSelectedCategory() {
        if (!m_stateList.GetSafeHwnd()) {
            return;
        }

        m_stateList.SetRedraw(FALSE);
        m_stateList.DeleteAllItems();

        const GameObject *selected = nullptr;
        if (!m_selectedCategoryName.IsEmpty()) {
            for (const GameObject &obj: m_displayState.objects) {
                if (obj.name == m_selectedCategoryName) {
                    selected = &obj;
                    break;
                }
            }
        }

        if (selected != nullptr) {
            for (size_t i = 0; i < selected->properties.size(); ++i) {
                const Property &prop = selected->properties[i];
                const int row = static_cast<int>(i);
                m_stateList.InsertItem(row, prop.name);
                m_stateList.SetItemText(row, 1, prop.value);
                m_stateList.SetItemText(row, 2, prop.type);
            }

            if (m_categoryDetails.GetSafeHwnd()) {
                m_categoryDetails.SetWindowText(selected->debugInfo);
                m_categoryDetails.SetSel(0, 0);
            }
        } else if (m_categoryDetails.GetSafeHwnd()) {
            m_categoryDetails.SetWindowText(_T(""));
        }

        m_stateList.SetRedraw(TRUE);
        m_stateList.Invalidate();
    }

    void CSnapshotToolUI::OnLoadButton() {
        CFileDialog dlg(TRUE, _T("sav"), nullptr, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
                        _T("Save Files (*.sav)|*.sav|All Files (*.*)|*.*||"));
        if (dlg.DoModal() == IDOK) {
            CString errorMessage;
            if (!m_logic.LoadSnapshotFromFile(dlg.GetPathName(), errorMessage)) {
                ShowErrorMessage(errorMessage);
            }
        }
    }

    void CSnapshotToolUI::OnLoadReplayButton() {
        CString initialDir;
        PWSTR docsPath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docsPath))) {
            initialDir = docsPath;
            CoTaskMemFree(docsPath);
            if (!initialDir.IsEmpty() && initialDir[initialDir.GetLength() - 1] != _T('\\')) {
                initialDir += _T("\\");
            }
            initialDir += _T("Command and Conquer Generals Zero Hour Data\\Replays");
        } else {
            TCHAR userProfile[MAX_PATH] = {0};
            const DWORD len = GetEnvironmentVariable(_T("USERPROFILE"), userProfile, MAX_PATH);
            if (len > 0 && len < MAX_PATH) {
                initialDir = userProfile;
                if (!initialDir.IsEmpty() && initialDir[initialDir.GetLength() - 1] != _T('\\')) {
                    initialDir += _T("\\");
                }
                initialDir += _T("Documents\\Command and Conquer Generals Zero Hour Data\\Replays");
            }
        }

        CFileDialog dlg(TRUE, _T("rep"), nullptr, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
                        _T("Replay Files (*.rep)|*.rep|All Files (*.*)|*.*||"));
        if (!initialDir.IsEmpty()) {
            dlg.m_ofn.lpstrInitialDir = const_cast<LPTSTR>(initialDir.GetString());
        }
        if (dlg.DoModal() == IDOK) {
            // Enable recording while launching the replay, then turn it off afterward.
            m_isRecording = true;
            UpdateRecordButtonImage();
            if (m_pipe) {
                m_pipe->SetRecording(true);
            }

            CString errorMessage;
            if (!m_logic.LoadReplayFromFile(dlg.GetPathName(), errorMessage)) {
                if (errorMessage.IsEmpty()) {
                    errorMessage = _T("Failed to load replay.");
                }
                ShowErrorMessage(errorMessage);
            }

            m_isRecording = false;
            UpdateRecordButtonImage();
            if (m_pipe) {
                m_pipe->SetRecording(false);
            }
        }
    }

    void CSnapshotToolUI::OnTimer(UINT_PTR nIDEvent) {
        if (nIDEvent == 1) {
            UpdateFromLogic();
        }
        CFrameWnd::OnTimer(nIDEvent);
    }

    void CSnapshotToolUI::RequestShutdownFromSignal() {
        PostMessage(WM_CLOSE);
    }

    void CSnapshotToolUI::OnDestroy() {
        KillTimer(1);
        g_mainFrame = nullptr;
        CFrameWnd::OnDestroy();
    }

    void CSnapshotToolUI::OnSaveButton() {
        CFileDialog dlg(
            FALSE,
            _T("txt"),
            _T("snapshot.txt"),
            OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
            _T("Text Files (*.txt)|*.txt|All Files (*.*)|*.*||"));
        if (dlg.DoModal() == IDOK) {
            CString errorMessage;
            if (!m_logic.SaveStateToFile(dlg.GetPathName(), errorMessage)) {
                ShowErrorMessage(errorMessage);
            }
        }
    }

    void CSnapshotToolUI::ShowErrorMessage(const CString &message) {
        AfxMessageBox(message);
    }

    void CSnapshotToolUI::OnRecordButton() {
        m_isRecording = !m_isRecording;
        UpdateRecordButtonImage();
        if (m_pipe) {
            m_pipe->SetRecording(m_isRecording);
        }
    }

    void CSnapshotToolUI::OnClearButton() {
        const int response = AfxMessageBox(_T("Are you sure you want to clear the state?"), MB_ICONQUESTION | MB_YESNO);
        if (response != IDYES) {
            return;
        }
        m_logic.Clear();
        m_displayState.objects.clear();
        m_selectedCategoryName.Empty();
        m_categoryNames.clear();
        if (m_categoryList.GetSafeHwnd()) {
            m_categoryList.ResetContent();
        }
        if (m_stateList.GetSafeHwnd()) {
            m_stateList.DeleteAllItems();
        }
        RenderState();
    }

    void CSnapshotToolUI::OnCategoryChanged() {
        int sel = m_categoryList.GetCurSel();
        if (sel == LB_ERR || sel < 0 || static_cast<size_t>(sel) >= m_categoryNames.size()) {
            return;
        }
        m_selectedCategoryName = m_categoryNames[static_cast<size_t>(sel)];
        RenderSelectedCategory();
    }

    void CSnapshotToolUI::UpdateRecordButtonImage() {
        if (!m_recordButton.GetSafeHwnd()) {
            return;
        }
        m_recordButton.SetWindowText(m_isRecording ? _T("Stop recording") : _T("Start recording"));
        m_recordButton.Invalidate();
    }

    void CSnapshotToolUI::UpdateFromLogic() {
        GameStateSnapshot latest;
        if (!m_logic.ConsumeState(latest)) {
            return;
        }
        m_displayState = std::move(latest);
        RenderState();
    }

    CSnapshotToolUI *CSnapshotToolUI::CreateInstance(GameStateLogic &logic, SnapshotPipeServer *pipe) {
        CSnapshotToolUI *ui = new CSnapshotToolUI(logic);
        ui->m_pipe = pipe;
        ui->CFrameWnd::Create(nullptr, _T("SnapshotTool (ZH only)"));
        return ui;
    }

    CSnapshotToolUI *CSnapshotToolUI::Get() {
        return g_mainFrame;
    }

    void CSnapshotToolUI::OnStateListCustomDraw(NMHDR *pNMHDR, LRESULT *pResult) {
        LPNMLVCUSTOMDRAW pLVCD = reinterpret_cast<LPNMLVCUSTOMDRAW>(pNMHDR);
        switch (pLVCD->nmcd.dwDrawStage) {
        case CDDS_PREPAINT:
            *pResult = CDRF_NOTIFYITEMDRAW;
            return;
        case CDDS_ITEMPREPAINT:
            *pResult = CDRF_NOTIFYSUBITEMDRAW;
            return;
        case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
            if (pLVCD->iSubItem == 2) {
                pLVCD->clrText = RGB(128, 128, 128);
            }
            *pResult = CDRF_DODEFAULT;
            return;
        default:
            *pResult = CDRF_DODEFAULT;
            return;
        }
    }

    void CSnapshotToolUI::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct) {
        if (nIDCtl == kBtnRecord && lpDrawItemStruct) {
            CDC dc;
            dc.Attach(lpDrawItemStruct->hDC);
            CRect rect = lpDrawItemStruct->rcItem;

            const bool isPressed = (lpDrawItemStruct->itemState & ODS_SELECTED) != 0;
            const bool isDisabled = (lpDrawItemStruct->itemState & ODS_DISABLED) != 0;
            COLORREF bgColor = m_isRecording ? RGB(200, 0, 0) : GetSysColor(COLOR_BTNFACE);
            if (isPressed) {
                bgColor = m_isRecording ? RGB(170, 0, 0) : GetSysColor(COLOR_3DSHADOW);
            }
            COLORREF textColor = isDisabled
                                     ? GetSysColor(COLOR_GRAYTEXT)
                                     : (m_isRecording ? RGB(255, 255, 255) : GetSysColor(COLOR_BTNTEXT));

            dc.FillSolidRect(&rect, bgColor);
            dc.DrawEdge(&rect, isPressed ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT);
            rect.DeflateRect(4, 4);

            CString caption;
            m_recordButton.GetWindowText(caption);
            CFont *font = m_recordButton.GetFont();
            CFont *oldFont = nullptr;
            if (font) {
                oldFont = dc.SelectObject(font);
            }
            dc.SetBkMode(TRANSPARENT);
            dc.SetTextColor(textColor);
            dc.DrawText(caption, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            if (oldFont) {
                dc.SelectObject(oldFont);
            }
            dc.Detach();
            return;
        }
        CFrameWnd::OnDrawItem(nIDCtl, lpDrawItemStruct);
    }
}// namespace SnapshotTool
