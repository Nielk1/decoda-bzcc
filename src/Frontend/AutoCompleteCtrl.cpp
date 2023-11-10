/*

Decoda
Copyright (C) 2007-2013 Unknown Worlds Entertainment, Inc.

This file is part of Decoda.

Decoda is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Decoda is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Decoda.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "AutoCompleteCtrl.h"
#include "Tokenizer.h"
#include "DebugFrontend.h"
#include "XmlUtility.h"

#include <wx/sstream.h>
#include <wx/xml/xml.h>

BEGIN_EVENT_TABLE(AutoCompleteCtrl, wxTreeListCtrl)
EVT_SIZE(AutoCompleteCtrl::OnSize)
EVT_LIST_COL_END_DRAG(wxID_ANY, AutoCompleteCtrl::OnColumnEndDrag)
END_EVENT_TABLE()

AutoCompleteCtrl::AutoCompleteCtrl(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style, const wxValidator& validator, const wxString& name)
    : wxTreeListCtrl(parent, id, pos, size, style, validator, name)
{

    AddColumn(_("Name"), 0, wxALIGN_LEFT);
    SetColumnEditable(0, false);
    //SetColumnPickType(0, wxTR_COLUMN_TEXT);
    AddColumn(_("Scope"), 0, wxALIGN_LEFT);
    SetColumnEditable(1, false);
    AddColumn(_("Type"), 0, wxALIGN_LEFT);
    SetColumnEditable(2, false);
    AddColumn(_("SymbolType"), 0, wxALIGN_LEFT);
    SetColumnEditable(3, false);
    AddColumn(_("RHS"), 0, wxALIGN_LEFT);
    SetColumnEditable(4, false);
    AddColumn(_("Line"), 0, wxALIGN_LEFT);
    SetColumnEditable(5, false);
    AddColumn(_("File"), 0, wxALIGN_LEFT);
    SetColumnEditable(6, false);

    m_columnSize[0] = 0.3f;
    m_columnSize[1] = 0.2f;
    m_columnSize[2] = 0.1f;
    m_columnSize[3] = 0.1f;
    m_columnSize[4] = 0.1f;
    m_columnSize[5] = 0.1f;
    m_columnSize[6] = -100;

    UpdateColumnSizes();

    m_vm = 0;
    m_stackLevel = 0;
}

void AutoCompleteCtrl::SetFontColorSettings(const FontColorSettings& settings)
{
    SetBackgroundColour(settings.GetColors(FontColorSettings::DisplayItem_Window).backColor);
    m_fontColor = settings.GetColors(FontColorSettings::DisplayItem_Window).foreColor;
}

void AutoCompleteCtrl::SetValueFont(const wxFont& font)
{

    m_valueFont = font;

    // Update all of the items.
    UpdateFont(GetRootItem());

}

void AutoCompleteCtrl::UpdateFont(wxTreeItemId item)
{

    if (item.IsOk())
    {

        SetItemFont(item, m_valueFont);
        UpdateFont(GetNextSibling(item));
        SetItemTextColour(item, m_fontColor);

        wxTreeItemIdValue cookie;
        wxTreeItemId child = GetFirstChild(item, cookie);

        while (child.IsOk())
        {
            UpdateFont(child);
            child = GetNextChild(item, cookie);
        }

    }

}

void AutoCompleteCtrl::UpdateColumnSizes()
{
    // We subtract two off of the size to avoid generating scroll bars on the window.
    int totalSize = GetClientSize().x - 2;

    int columnSize[s_numColumns];
    GetColumnSizes(totalSize, columnSize);

    for (unsigned int i = 0; i < s_numColumns; ++i)
    {
        SetColumnWidth(i, columnSize[i]);
    }

}

void AutoCompleteCtrl::GetColumnSizes(int totalSize, int columnSize[s_numColumns]) const
{
    int fixedSize = 0;
    for (unsigned int i = 0; i < s_numColumns; ++i)
    {
        if (m_columnSize[i] < 0.0f)
        {
            columnSize[i] = static_cast<int>(-m_columnSize[i]);
            fixedSize += columnSize[i];
        }
    }

    // Set the size of the proportional columns.
    for (unsigned int i = 0; i < s_numColumns; ++i)
    {
        if (m_columnSize[i] >= 0.0f)
        {
            columnSize[i] = static_cast<int>(m_columnSize[i] * (totalSize - fixedSize) + 0.5f);
        }
    }

    // Make sure the total size is exactly correct by resizing the final column.
    for (unsigned int i = 0; i < s_numColumns - 1; ++i)
    {
        totalSize -= columnSize[i];
    }
    columnSize[s_numColumns - 1] = totalSize - 20;
}

void AutoCompleteCtrl::SetContext(unsigned int vm, unsigned int stackLevel)
{
    m_vm = vm;
    m_stackLevel = stackLevel;
}

void AutoCompleteCtrl::OnColumnEndDrag(wxListEvent& event)
{

    // Resize all of the columns to eliminate the scroll bar.

    int totalSize = GetClientSize().x;
    int column = event.GetColumn();

    int columnSize[s_numColumns];
    GetColumnSizes(totalSize, columnSize);

    int x = event.GetPoint().x;

    int newColumnSize = GetColumnWidth(column);
    int delta = columnSize[column] - newColumnSize;

    if (column + 1 < s_numColumns)
    {
        columnSize[column] = newColumnSize;
        columnSize[column + 1] += delta;
    }

    // Update the proportions of the columns.

    int fixedSize = 0;

    for (unsigned int i = 0; i < s_numColumns; ++i)
    {
        if (m_columnSize[i] < 0.0f)
        {
            m_columnSize[i] = -static_cast<float>(columnSize[i]);
            fixedSize += columnSize[i];
        }
    }

    for (unsigned int i = 0; i < s_numColumns; ++i)
    {
        if (m_columnSize[i] >= 0.0f)
        {
            m_columnSize[i] = static_cast<float>(columnSize[i]) / (totalSize - fixedSize);
        }
    }

    UpdateColumnSizes();

}

void AutoCompleteCtrl::OnSize(wxSizeEvent& event)
{
    UpdateColumnSizes();
    event.Skip();
}