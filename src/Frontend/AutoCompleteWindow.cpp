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

#include "AutoCompleteWindow.h"
#include "Tokenizer.h"

BEGIN_EVENT_TABLE(AutoCompleteWindow, AutoCompleteCtrl)
//EVT_TREE_BEGIN_LABEL_EDIT(wxID_ANY, AutoCompleteWindow::OnBeginLabelEdit)
//EVT_TREE_END_LABEL_EDIT(wxID_ANY, AutoCompleteWindow::OnEndLabelEdit)
//EVT_TREE_ITEM_ACTIVATED(wxID_ANY, AutoCompleteWindow::OnItemSelected)
//EVT_TREE_KEY_DOWN(wxID_ANY, AutoCompleteWindow::OnKeyDown)
END_EVENT_TABLE()

AutoCompleteWindow::AutoCompleteWindow(wxWindow* parent, wxWindowID winid, AutoCompleteManager* autoCompleteManager)
    : AutoCompleteCtrl(parent, winid, wxDefaultPosition, wxDefaultSize)
{

    m_root = AddRoot(_T("Root"));
    SetItemText(m_root, 1, _T("Root"));

    //SetDropTarget(new WatchDropTarget(this));

    //CreateEmptySlotIfNeeded();

    //m_editing = false;

    m_autoCompleteManager = autoCompleteManager;
}

void AutoCompleteWindow::UpdateItems()
{
    m_acEntries.clear();
    m_autoCompleteManager->GetAllItems(m_acEntries);

    DeleteChildren(m_root);
    //PrependItem(m_root, "");
    for (int i = 0, e = m_acEntries.size(); i < e; ++i)
    {
        //AddWatch(watches[i]);
        //wxString temp = expression;
        //temp.Trim().Trim(false);
        
        //wxString temp = m_acEntries[i].name;
         wxString temp = m_acEntries[i].symbol->GetTooltip();

        //if (!temp.empty())
        {
        
            wxTreeItemIdValue cookie;
            wxTreeItemId lastItem = GetLastChild(m_root, cookie);
        
            // The last item should be our blank item.
            //assert(lastItem.IsOk());
            //lastItem = GetPrevSibling(lastItem);
        
            wxTreeItemId item;
        
            if (lastItem.IsOk())
            {
                item = InsertItem(m_root, lastItem, temp);
            }
            else
            {
                item = PrependItem(m_root, temp);
            }
        
            SetItemText(item, 1, m_acEntries[i].scope);
            switch (m_acEntries[i].type)
            {
            case AutoCompleteManager::Type_Unknown: SetItemText(item, 2, "Unknown"); break;
            case AutoCompleteManager::Type_Function: SetItemText(item, 2, "Function"); break;
            case AutoCompleteManager::Type_Class: SetItemText(item, 2, "Class"); break;
            case AutoCompleteManager::Type_Variable: SetItemText(item, 2, "Variable"); break;
            }
            wxString symbolType = "";
            if (m_acEntries[i].symbol->type == SymbolType::Unknown  ) symbolType = "Unknown";
            if (m_acEntries[i].symbol->type & SymbolType::Function  ) if (symbolType == "") symbolType = "Unknown"   ; else symbolType += "|Function"  ;
            if (m_acEntries[i].symbol->type & SymbolType::Type      ) if (symbolType == "") symbolType = "Type"      ; else symbolType += "|Type"      ;
            if (m_acEntries[i].symbol->type & SymbolType::Module    ) if (symbolType == "") symbolType = "Module"    ; else symbolType += "|Module"    ;
            if (m_acEntries[i].symbol->type & SymbolType::Variable  ) if (symbolType == "") symbolType = "Variable"  ; else symbolType += "|Variable"  ;
            if (m_acEntries[i].symbol->type & SymbolType::Prefix    ) if (symbolType == "") symbolType = "Prefix"    ; else symbolType += "|Prefix"    ;
            if (m_acEntries[i].symbol->type & SymbolType::Assignment) if (symbolType == "") symbolType = "Assignment"; else symbolType += "|Assignment";
            SetItemText(item, 3, symbolType);
            SetItemText(item, 4, m_acEntries[i].symbol->rhs);
            SetItemText(item, 5, wxString::Format(wxT("%i"), m_acEntries[i].symbol->line));
            SetItemText(item, 6, m_acEntries[i].file->GetDisplayName());

            //UpdateItem(item);
        }
    }

    /*wxTreeItemIdValue cookie;
    wxTreeItemId item = GetFirstChild(m_root, cookie);

    while (item.IsOk())
    {
        UpdateItem(item);
        item = GetNextSibling(item);
    }*/
}

void AutoCompleteWindow::SetEditorSettings(const EditorSettings& settings)
{
    m_enableAutoComplete = settings.GetEnableAutoComplete();
}