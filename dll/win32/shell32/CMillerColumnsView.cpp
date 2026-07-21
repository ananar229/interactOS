/*
 * PROJECT:     shell32
 * LICENSE:     LGPL-2.1+ (https://spdx.org/licenses/LGPL-2.1+)
 * PURPOSE:     Miller Columns (cascading column browser) folder view
 */

#include "precomp.h"

WINE_DEFAULT_DEBUG_CHANNEL(shell);

// Frees the per-item PIDLs stashed in lParam (mirrors ownership convention
// of CDefView::fill_list / FillList: a raw pidl straight out of
// IEnumIDList::Next is owned by whoever stores it, freed with SHFree) and
// empties the listview.
static void
MillerColumn_ClearItems(HWND hWndList)
{
    int cItems = ListView_GetItemCount(hWndList);
    for (int i = 0; i < cItems; i++)
    {
        LVITEMW lvItem = { LVIF_PARAM };
        lvItem.iItem = i;
        if (ListView_GetItem(hWndList, &lvItem) && lvItem.lParam)
            SHFree(reinterpret_cast<LPVOID>(lvItem.lParam));
    }
    ListView_DeleteAllItems(hWndList);
}

CMillerColumnsView::CMillerColumnsView()
    : m_hWndParent(NULL)
    , m_hWndPreview(NULL)
    , m_bPreviewVisible(false)
{
}

CMillerColumnsView::~CMillerColumnsView()
{
    Destroy();
}

BOOL CMillerColumnsView::Create(HWND hWndParent, IShellFolder *psfRoot)
{
    Destroy();

    m_hWndParent = hWndParent;
    return CreateColumn(psfRoot);
}

void CMillerColumnsView::Destroy()
{
    for (int i = 0; i < m_Columns.GetSize(); i++)
    {
        HWND hWnd = m_Columns[i].hWnd;
        if (::IsWindow(hWnd))
        {
            MillerColumn_ClearItems(hWnd);
            ::DestroyWindow(hWnd);
        }
    }
    m_Columns.RemoveAll();

    if (::IsWindow(m_hWndPreview))
        ::DestroyWindow(m_hWndPreview);
    m_hWndPreview = NULL;
    m_bPreviewVisible = false;
}

void CMillerColumnsView::ShowHide(BOOL bShow)
{
    for (int i = 0; i < m_Columns.GetSize(); i++)
        ::ShowWindow(m_Columns[i].hWnd, bShow ? SW_SHOW : SW_HIDE);

    if (m_hWndPreview)
        ::ShowWindow(m_hWndPreview, (bShow && m_bPreviewVisible) ? SW_SHOW : SW_HIDE);
}

void CMillerColumnsView::Layout(int cx, int cy, int scrollX)
{
    UNREFERENCED_PARAMETER(cx);

    int i = 0;
    for (; i < m_Columns.GetSize(); i++)
    {
        int x = i * COLUMN_WIDTH - scrollX;
        ::MoveWindow(m_Columns[i].hWnd, x, 0, COLUMN_WIDTH, cy, TRUE);
    }

    if (m_bPreviewVisible && m_hWndPreview)
    {
        int x = i * COLUMN_WIDTH - scrollX;
        ::MoveWindow(m_hWndPreview, x, 0, PREVIEW_WIDTH, cy, TRUE);
    }
}

int CMillerColumnsView::GetContentWidth() const
{
    int cx = m_Columns.GetSize() * COLUMN_WIDTH;
    if (m_bPreviewVisible)
        cx += PREVIEW_WIDTH;
    return cx;
}

HWND CMillerColumnsView::GetColumnWindow(int iColumn) const
{
    return (iColumn >= 0 && iColumn < m_Columns.GetSize()) ? m_Columns[iColumn].hWnd : NULL;
}

int CMillerColumnsView::FindColumn(HWND hWnd) const
{
    for (int i = 0; i < m_Columns.GetSize(); i++)
    {
        if (m_Columns[i].hWnd == hWnd)
            return i;
    }
    return -1;
}

CComPtr<IShellFolder> CMillerColumnsView::GetColumnFolder(int iColumn) const
{
    CComPtr<IShellFolder> psf;
    if (iColumn >= 0 && iColumn < m_Columns.GetSize())
        psf = m_Columns[iColumn].pFolder;
    return psf;
}

PCUITEMID_CHILD CMillerColumnsView::GetItemPidl(int iColumn, int iItem) const
{
    HWND hWndList = GetColumnWindow(iColumn);
    if (!hWndList || iItem < 0)
        return NULL;

    LVITEMW lvItem = { LVIF_PARAM };
    lvItem.iItem = iItem;
    if (!ListView_GetItem(hWndList, &lvItem))
        return NULL;

    return reinterpret_cast<PCUITEMID_CHILD>(lvItem.lParam);
}

void CMillerColumnsView::TruncateAfter(int iColumn)
{
    HidePreview();

    // Always remove from the tail so CSimpleArray never has to shift any
    // surviving element (it can only ever destruct+memmove the removed
    // element itself, which is safe for a CComPtr-bearing element).
    for (int i = m_Columns.GetSize() - 1; i > iColumn; i--)
    {
        HWND hWnd = m_Columns[i].hWnd;
        if (::IsWindow(hWnd))
        {
            MillerColumn_ClearItems(hWnd);
            ::DestroyWindow(hWnd);
        }
        m_Columns.RemoveAt(i);
    }
}

BOOL CMillerColumnsView::AppendFolderColumn(IShellFolder *psf)
{
    HidePreview();
    return CreateColumn(psf);
}

void CMillerColumnsView::ShowPreview(IShellFolder *psf, PCUITEMID_CHILD pidl)
{
    if (!m_hWndPreview)
    {
        m_hWndPreview = ::CreateWindowExW(WS_EX_CLIENTEDGE,
                                           L"STATIC",
                                           L"",
                                           WS_CHILD | SS_LEFT | SS_NOPREFIX,
                                           0, 0, 0, 0,
                                           m_hWndParent,
                                           NULL,
                                           _AtlBaseModule.GetModuleInstance(),
                                           NULL);
    }
    if (!m_hWndPreview)
        return;

    WCHAR szText[1024];
    if (FAILED(DisplayNameOfW(psf, pidl, SHGDN_NORMAL, szText, _countof(szText))))
        szText[0] = UNICODE_NULL;

    PWSTR pszTip = NULL;
    if (SUCCEEDED(SHELL_QueryInfoTipAlloc(psf, QITIPF_DEFAULT, pidl, &pszTip)) && pszTip)
    {
        StringCchCatW(szText, _countof(szText), L"\r\n\r\n");
        StringCchCatW(szText, _countof(szText), pszTip);
        SHFree(pszTip);
    }

    ::SetWindowTextW(m_hWndPreview, szText);
    m_bPreviewVisible = true;
    ::ShowWindow(m_hWndPreview, SW_SHOW);
}

void CMillerColumnsView::HidePreview()
{
    m_bPreviewVisible = false;
    if (m_hWndPreview)
        ::ShowWindow(m_hWndPreview, SW_HIDE);
}

BOOL CMillerColumnsView::CreateColumn(IShellFolder *psf)
{
    DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP |
                    LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_SINGLESEL |
                    LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS | LVS_EDITLABELS;
    DWORD dwExStyle = WS_EX_CLIENTEDGE;

    int iColumn = m_Columns.GetSize();
    HWND hWndList = ::CreateWindowExW(dwExStyle,
                                       WC_LISTVIEWW,
                                       L"",
                                       dwStyle,
                                       iColumn * COLUMN_WIDTH, 0, COLUMN_WIDTH, 0,
                                       m_hWndParent,
                                       NULL,
                                       _AtlBaseModule.GetModuleInstance(),
                                       NULL);
    if (!hWndList)
        return FALSE;

    ListView_SetExtendedListViewStyle(hWndList, LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);

    HIMAGELIST hSmall = NULL;
    Shell_GetImageLists(NULL, &hSmall);
    ListView_SetImageList(hWndList, hSmall, LVSIL_SMALL);

    LVCOLUMNW lvc = { LVCF_FMT | LVCF_WIDTH };
    lvc.fmt = LVCFMT_LEFT;
    lvc.cx = COLUMN_WIDTH - GetSystemMetrics(SM_CXVSCROLL) - 4;
    ListView_InsertColumn(hWndList, 0, &lvc);

    COLUMN col;
    col.hWnd = hWndList;
    col.pFolder = psf;
    m_Columns.Add(col);

    FillColumn(hWndList, psf);
    return TRUE;
}

HRESULT CMillerColumnsView::FillColumn(HWND hWndList, IShellFolder *psf)
{
    MillerColumn_ClearItems(hWndList);

    CComPtr<IEnumIDList> pEnum;
    HRESULT hr = psf->EnumObjects(m_hWndParent, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &pEnum);
    if (hr != S_OK)
        return FAILED(hr) ? hr : S_OK;

    ListView_SetRedraw(hWndList, FALSE);

    PITEMID_CHILD pidl;
    DWORD dwFetched;
    int iItem = 0;
    while (pEnum->Next(1, &pidl, &dwFetched) == S_OK && dwFetched)
    {
        WCHAR szName[MAX_PATH];
        if (FAILED(DisplayNameOfW(psf, pidl, SHGDN_NORMAL, szName, _countof(szName))))
            szName[0] = UNICODE_NULL;

        LVITEMW lvItem = { LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM };
        lvItem.iItem = iItem;
        lvItem.pszText = szName;
        lvItem.iImage = SHMapPIDLToSystemImageListIndex(psf, pidl, 0);
        lvItem.lParam = reinterpret_cast<LPARAM>(pidl); // ownership moves to the listview item
        ListView_InsertItem(hWndList, &lvItem);
        iItem++;
    }

    ListView_SetRedraw(hWndList, TRUE);
    return S_OK;
}
