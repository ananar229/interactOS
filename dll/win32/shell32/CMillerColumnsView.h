/*
 * PROJECT:     shell32
 * LICENSE:     LGPL-2.1+ (https://spdx.org/licenses/LGPL-2.1+)
 * PURPOSE:     Miller Columns (cascading column browser) folder view
 */

#pragma once

// Hosts a row of WC_LISTVIEW child windows inside CDefView's own HWND, one
// per visible folder level (macOS Finder / NeXTSTEP "Miller columns" style
// browsing), plus one trailing metadata preview pane for the last selected
// file. Column 0 always shows the folder CDefView itself is bound to;
// selecting a subfolder in column N reveals column N+1 to the right, while
// selecting a non-folder item replaces anything to the right with the
// preview pane instead.
//
// This is a plain helper object (not a COM class, not its own window class)
// so that CDefView keeps sole ownership of the outer HWND, its message
// dispatch, and all shell integration (drag&drop target registration,
// context menus, rename) - the per-column listviews are just further
// full-bleed-height children of CDefView::m_hWnd, exactly like the classic
// single m_ListView is today. CDefView is responsible for reacting to
// selection-change notifications from the column listviews (by hwndFrom)
// and driving the cascade/preview/layout calls below.
class CMillerColumnsView
{
public:
    static const int COLUMN_WIDTH = 190;
    static const int PREVIEW_WIDTH = 260;

    CMillerColumnsView();
    ~CMillerColumnsView();

    // Creates column 0 for psfRoot. hWndParent is CDefView's own m_hWnd.
    BOOL Create(HWND hWndParent, IShellFolder *psfRoot);

    // Destroys all column child windows and releases their folder bindings.
    void Destroy();

    // Shows/hides every column window and the preview pane, if any (used
    // when switching view modes; does not change which columns exist or
    // whether the preview is logically active).
    void ShowHide(BOOL bShow);

    // Repositions/sizes all columns and the preview pane (if active) to
    // fill a cy-tall client area, offset horizontally by scrollX.
    void Layout(int cx, int cy, int scrollX);

    // Total logical width of all columns plus the preview pane if active;
    // used by CDefView to compute horizontal scrollbar range.
    int GetContentWidth() const;

    BOOL IsActive() const { return m_Columns.GetSize() > 0; }
    int GetColumnCount() const { return m_Columns.GetSize(); }
    HWND GetColumnWindow(int iColumn) const;

    // Returns the column index owning hWnd, or -1 if hWnd is none of ours.
    int FindColumn(HWND hWnd) const;

    // The IShellFolder bound to column iColumn (AddRef'd copy), or NULL.
    CComPtr<IShellFolder> GetColumnFolder(int iColumn) const;

    // The child pidl of the iItem'th row in column iColumn, or NULL. Owned
    // by the listview item - valid only until that item is removed/changed.
    PCUITEMID_CHILD GetItemPidl(int iColumn, int iItem) const;

    // Destroys every column after iColumn and hides the preview pane -
    // called right before revealing a new column or preview at iColumn + 1.
    void TruncateAfter(int iColumn);

    // Appends a new folder column (bound to psf) after the current last
    // column, hiding the preview pane if it was showing.
    BOOL AppendFolderColumn(IShellFolder *psf);

    // Shows/updates the trailing metadata preview pane for a non-folder
    // item (icon-less: display name plus IQueryInfo::GetInfoTip if any).
    void ShowPreview(IShellFolder *psf, PCUITEMID_CHILD pidl);
    void HidePreview();

private:
    struct COLUMN
    {
        HWND hWnd;
        CComPtr<IShellFolder> pFolder;
    };

    HWND m_hWndParent;
    CSimpleArray<COLUMN> m_Columns;
    HWND m_hWndPreview;
    bool m_bPreviewVisible;

    BOOL CreateColumn(IShellFolder *psf);
    HRESULT FillColumn(HWND hWndList, IShellFolder *psf);
};
