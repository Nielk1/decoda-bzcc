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

#include "ProjectExplorerWindow.h"
#include "SearchTextCtrl.h"
#include "ProjectFileInfoCtrl.h"
//#include "ProjectFilterPopup.h"
#include "Tokenizer.h"
#include "StlUtility.h"
#include "Symbol.h"
#include "DebugFrontend.h"

#include <wx/file.h>
#include <wx/listctrl.h>
#include <wx/dir.h>
#include <wx/stack.h>

#include <unordered_map>

#include "res/explorer.xpm"
#include "res/filter_bitmap.xpm"

DECLARE_EVENT_TYPE( wxEVT_TREE_CONTEXT_MENU, -1 )

typedef void (wxEvtHandler::*wxTreeContextMenuEvent)(wxTreeEvent&);

#define EVT_TREE_CONTEXT_MENU(id, fn) \
    DECLARE_EVENT_TABLE_ENTRY( wxEVT_TREE_CONTEXT_MENU, id, -1, \
    (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) (wxNotifyEventFunction) \
    wxStaticCastEvent( wxTreeContextMenuEvent, & fn ), (wxObject *) NULL ),

DEFINE_EVENT_TYPE( wxEVT_TREE_CONTEXT_MENU )

BEGIN_EVENT_TABLE(ProjectExplorerWindow, wxPanel)

    EVT_TEXT( ID_Filter,                OnFilterTextChanged )

    //EVT_BUTTON( ID_FilterButton,        OnFilterButton )

    EVT_TREE_ITEM_EXPANDING(wxID_ANY,   OnTreeItemExpanding )
    EVT_TREE_ITEM_COLLAPSING(wxID_ANY,  OnTreeItemCollapsing )
    EVT_TREE_ITEM_ACTIVATED(wxID_ANY,   OnTreeItemActivated )
    EVT_TREE_ITEM_MENU(wxID_ANY,        OnTreeItemContextMenu)
    EVT_TREE_SEL_CHANGED(wxID_ANY,      OnTreeItemSelectionChanged)

    EVT_TREE_CONTEXT_MENU(wxID_ANY,     OnMenu)

END_EVENT_TABLE()

ProjectExplorerWindow::ProjectExplorerWindow(wxWindow* parent, wxWindowID winid)
    : wxPanel(parent, winid), first_click(false)
{
    SetSize(250, 300);

    m_stopExpansion = 0;

    // Load the bitmaps for the tree view.

    wxBitmap bitmap(explorer, wxBITMAP_TYPE_XPM);

    wxImageList* imageList = new wxImageList(18, 17);
    imageList->Add(bitmap, wxColour(0xFF, 0x9B, 0x77));

    m_root = 0;

	wxFlexGridSizer* gSizer1;
	gSizer1 = new wxFlexGridSizer( 3, 1, 0, 0 );
	
    gSizer1->AddGrowableCol( 0 );
    gSizer1->AddGrowableRow( 1 );
    gSizer1->SetFlexibleDirection( wxBOTH );

	wxFlexGridSizer* gSizer2;
	gSizer2 = new wxFlexGridSizer( 1, 2, 0, 0 );
	
    gSizer2->AddGrowableCol( 0 );
    gSizer2->AddGrowableRow( 0 );
    gSizer2->SetFlexibleDirection( wxBOTH );

    m_searchBox = new SearchTextCtrl(this, ID_Filter, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_RICH);
    
    gSizer2->Add( m_searchBox, 1, wxALL | wxEXPAND, 4);
    gSizer1->Add( gSizer2, 1, wxEXPAND, 5  );

    m_tree = new wxProjectTree(this, winid, wxDefaultPosition, wxDefaultSize, wxTR_NO_LINES | wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_HIDE_ROOT | wxTR_MULTIPLE | wxTR_FULL_ROW_HIGHLIGHT);
    m_tree->AssignImageList(imageList);

    // The "best size" for the tree component must be 0 so that the tree control doesn't
    // attempt to expand to fit all of the items in it (which would cause the info box to
    // be nudged out of the window.
    m_tree->SetInitialSize(wxSize(0,0));

    m_searchBox->SetNextWindow(m_tree);

    m_infoBox = new ProjectFileInfoCtrl(this);

    gSizer1->Add( m_tree, 1, wxALL|wxEXPAND, 4 );
    gSizer1->Add( m_infoBox, 1, wxALL | wxEXPAND, 4 );

	SetSizer( gSizer1 );
	Layout();

    m_project = NULL;

    m_contextMenu = NULL;
    m_temporaryContextMenu = NULL;
    m_directoryContextMenu = NULL;
    m_filterMatchAnywhere = false;
    m_hasFilter = false;
}

ProjectExplorerWindow::~ProjectExplorerWindow()
{
}


void ProjectExplorerWindow::SetFontColorSettings(const FontColorSettings& settings)
{
  SetBackgroundColour(settings.GetColors(FontColorSettings::DisplayItem_Window).backColor);
  m_tree->SetBackgroundColour(settings.GetColors(FontColorSettings::DisplayItem_Window).backColor);

  m_searchBox->SetBackgroundColour(settings.GetColors(FontColorSettings::DisplayItem_Window).backColor);
  m_searchBox->SetDefaultStyle(wxTextAttr(settings.GetColors(FontColorSettings::DisplayItem_Window).foreColor));

  m_infoBox->SetFontColorSettings(settings);

  m_itemColor = settings.GetColors(FontColorSettings::DisplayItem_WindowMargin).foreColor;
  m_itemSelectBackground = settings.GetColors(FontColorSettings::DisplayItem_WindowMargin).backColor;

  Rebuild();
}

void ProjectExplorerWindow::SetFocusToTree()
{
    m_tree->SetFocus();
}

void ProjectExplorerWindow::SetFocusToFilter()
{
    // Select all of the text in the box (makes it easy to clear).
    m_searchBox->SetSelection(-1, -1);
    m_searchBox->SetFocus();
}

void ProjectExplorerWindow::SetProject(Project* project)
{
    m_project = project;
    m_expandedIds.clear();
    Rebuild();
}

ProjectExplorerWindow::ItemData* ProjectExplorerWindow::GetDataForItem(wxTreeItemId id) const
{
  ItemData* data = nullptr;
  if (id.IsOk())
    data =static_cast<ItemData*>(m_tree->GetItemData(id));
  
  return data;
}

wxTreeItemId ProjectExplorerWindow::GetSelection() const
{

    wxArrayTreeItemIds selectedIds;
    m_tree->GetSelections(selectedIds);

    if (selectedIds.Count() > 0)
    {
        return selectedIds[0];
    }
    else
    {
        return wxTreeItemId();
    }

}

void ProjectExplorerWindow::ClearSelection()
{
    m_tree->UnselectAll();
}

void ProjectExplorerWindow::ColapseOrExpandSelectedItem()
{
    wxArrayTreeItemIds selectedIds;
    m_tree->GetSelections(selectedIds);
    if (selectedIds.size() != 1)
        return;
    wxTreeItemId id = selectedIds[0];
    if (id.IsOk() && m_tree->ItemHasChildren(id))
    {
        if (m_tree->IsExpanded(id))
            m_tree->Collapse(id);
        else
            m_tree->Expand(id);
    }
}

bool ProjectExplorerWindow::IsTreeFrozen()
{
    return m_tree->IsFrozen();
}

bool ProjectExplorerWindow::HasFilter()
{
    return m_hasFilter;
}

void ProjectExplorerWindow::OnFilterTextChanged(wxCommandEvent& event)
{
    //No previous filter
    if (m_hasFilter == false)
    {
      SaveExpansion();
      m_hasFilter = true;
    }

    m_filter = m_searchBox->GetValue();

    if (m_filter.size() == 0)
      m_hasFilter = false;

    // If the filter begins with a space, we'll match the text anywhere in the
    // the symbol name.
    unsigned int filterLength = m_filter.Length();
    m_filter.Trim(false);
    m_filterMatchAnywhere = (m_filter.Length() < filterLength);

    // Make the filter lowercase since matching with be case independent.
    m_filter.MakeLower();
    
    Rebuild();

    if (filterLength > 0)
    {
      //Expand all directories
      TraverseTree(m_root, [&, this](wxTreeItemId const &item)
      {
        ItemData* data = static_cast<ItemData*>(m_tree->GetItemData(item));
        if (!data || data->isFile == false)
          m_tree->Expand(item);
      });
    }
    else
    {
      LoadExpansion();
    }
    m_searchBox->SetFocus();
}

void ProjectExplorerWindow::Rebuild()
{

    m_tree->Freeze();

    m_tree->DeleteAllItems();
    m_root = m_tree->AddRoot("Root");

    if (m_project != NULL)
    {
        for (unsigned int i = 0; i < m_project->GetNumDirectories(); ++i)
        {
           RebuildForDirectory(m_project->GetDirectory(i));
        }

        for (unsigned int i = 0; i < m_project->GetNumFiles(); ++i)
        {
            RebuildForFile(m_root, m_project->GetFile(i));
        }
    }

    SortTree(m_tree->GetRootItem());

    
    /*TraverseTree(m_root, [&,this](wxTreeItemId const &id)
    {
      m_tree->SetItemSelectedColour(id, m_itemSelectBackground);
    });*/ //todo

    // For whatever reason the unselect event isn't reported properly
    // after deleting all items, so explicitly clear out the info box.
    m_infoBox->SetFile(NULL);

    m_tree->Thaw();
}

void ProjectExplorerWindow::RebuildForDirectory(Project::Directory *directory)
{
  ItemData* data = new ItemData;
  data->file = directory;
  data->symbol = NULL;
  data->isFile = false;

  wxTreeItemId node = m_tree->AppendItem(m_root, directory->name, 8, 8, data);
  m_tree->SetItemTextColour(node, m_itemColor);

  for (Project::File *file : directory->files)
  {
    RebuildForFile(node, file);
  }
}

void ProjectExplorerWindow::RebuildForFile(wxTreeItemId node, Project::File* file)
{

        if (m_filter.IsEmpty())
        {
            // Just include the files like the standard Visual Studio project view.
            AddFile(node, file);
        }
        else
        {

            // Filter all of the symbols, modules and file name.
                
            // Check to see if the file name matches the filter.
          if (MatchesFilter(file->fileName.GetFullName(), m_filter) || MatchesFilter(file->localPath, m_filter))
            {
                AddFile(node, file);
            }

            for (unsigned int j = 0; j < file->symbols.size(); ++j)
            {

                Symbol* symbol = file->symbols[j];
                
                // Check to see if the symbol matches the filter.
                if (MatchesFilter(symbol->name, m_filter))
                {
                    AddSymbol(node, file, symbol);
                }

            }
        
        }
}

bool ProjectExplorerWindow::MatchesFilter(const wxString& string, const wxString& filter) const
{

    if (m_filterMatchAnywhere)
    {
        return string.Lower().Find(m_filter) != wxNOT_FOUND;
    }
    else
    {

        if (filter.Length() > string.Length())
        {
            return false;
        }

        for (unsigned int i = 0; i < filter.Length(); ++i)
        {
            if ((char)tolower(string[i]) != filter[i])
            {
                return false;
            }
        }

        return true;
    
    }

}

void ProjectExplorerWindow::AddFile(wxTreeItemId parent, Project::File* file)
{
    ItemData* data = new ItemData;
    data->file      = file;
    data->symbol    = NULL;
    data->isFile    = true;

    int image = GetImageForFile(file);
    wxTreeItemIdValue cookie;

    bool found = false;
    wxString path = file->localPath;
    wxString directory = path.BeforeFirst('\\');
    wxTreeItemId node;
    wxTreeItemId parentNode = parent;

    if (directory.IsEmpty())
      node = parent;
    else
    {
      while (!directory.IsEmpty())
      {
        found = false;

        //Add needed directories for file
        node = m_tree->GetFirstChild(parentNode, cookie);
        while (node.IsOk())
        {
          if (m_tree->GetItemText(node) == directory)
          {
            found = true;
            break;
          }

          node = m_tree->GetNextChild(parentNode, cookie);
        }

        if (!found)
        {
          node = m_tree->AppendItem(parentNode, directory, 8, 8, nullptr);
          m_tree->SetItemTextColour(node, m_itemColor);
        }

        path.Remove(0, directory.size() + 1);

        directory = path.BeforeFirst('\\');

        parentNode = node;
      }

    }

    wxTreeItemId fileNode = m_tree->AppendItem(node, file->GetDisplayName(), image, image, data);
    m_tree->SetItemTextColour(fileNode, m_itemColor);
    // Add the symbols.

    std::unordered_map<Symbol *, wxTreeItemId> groups;

    for (unsigned int i = 0; i < file->symbols.size(); ++i)
    {
      //Only add standard symbols
      if (!(file->symbols[i]->type & Symbol::Type_Standard))
        continue;

        wxTreeItemId node = fileNode;

        if (file->symbols[i]->parent != nullptr)
        {
          Symbol *parent = file->symbols[i]->parent;
          std::unordered_map<Symbol *, wxTreeItemId>::const_iterator iterator;
          while (parent != nullptr)
          {
            iterator = groups.find(parent);
            if (iterator == groups.end())
            {
              node = AddSymbol(node, file, parent);
              iterator = groups.insert(std::make_pair(parent, node)).first;
            }

            parent = parent->parent;
          }

          iterator = groups.find(file->symbols[i]->parent);
          node = iterator->second;
        }

        std::unordered_map<Symbol *, wxTreeItemId>::const_iterator iterator;
        iterator = groups.find(file->symbols[i]);
        if (iterator == groups.end())
        {
          node = AddSymbol(node, file, file->symbols[i]);
          groups.insert(std::make_pair(file->symbols[i], node));
        }
    }

}

wxTreeItemId ProjectExplorerWindow::AddSymbol(wxTreeItemId parent, Project::File* file, Symbol* symbol)
{
    ItemData* data = new ItemData;
    data->file      = file;
    data->symbol    = symbol;
    data->isFile    = true;

    wxString fullName = symbol->name;

    //if (symbol->parent != nullptr)
    //{
    //  fullName += " - " + symbol->GetModuleName();
    //}

    int image = 0;
    if (symbol->type == SymbolType::Function)
    {
      image = Image_Function;
      fullName += "()";
    }
    if (symbol->type == SymbolType::Module)
    {
      image = Image_Module;
    }

    wxTreeItemId node = m_tree->AppendItem(parent, fullName, image, image, data);
    m_tree->SetItemTextColour(node, m_itemColor);

    return node;
}

void ProjectExplorerWindow::OnTreeItemExpanding(wxTreeEvent& event)
{
    if (event.GetItem() == m_stopExpansion)
    {
        event.Veto();
        m_stopExpansion = 0;
    }
}

void ProjectExplorerWindow::OnTreeItemCollapsing(wxTreeEvent& event)
{
    if (event.GetItem() == m_stopExpansion)
    {
        event.Veto();
        m_stopExpansion = 0;
    }
}

void ProjectExplorerWindow::OnTreeItemActivated(wxTreeEvent& event)
{
    
    // This is a bit hacky, but to prevent the control from expanding an item
    // when we activate it, we store the id and then veto the
    // expansion/contraction event.
    m_stopExpansion = event.GetItem();

    // Keep processing the event.
    event.Skip(true);

}

void ProjectExplorerWindow::OnTreeItemContextMenu(wxTreeEvent& event)
{
    m_tree->SelectItem(event.GetItem(), true);

    if (m_contextMenu != NULL)
    {

        ItemData* itemData = GetDataForItem(event.GetItem());

        // If this is a file, display the context menu.
        if (itemData != NULL && itemData->file != NULL && itemData->symbol == NULL)
        {
            wxTreeEvent copy(event);
            copy.SetInt(0);
            Project::File *f = static_cast<Project::File *>(itemData->file);
            if (f->temporary && m_temporaryContextMenu != NULL)
                copy.SetInt(2);
            copy.SetEventType(wxEVT_TREE_CONTEXT_MENU);
            AddPendingEvent(copy);
        }
        //Is a basic directory
        else if (itemData == NULL)
        {
          wxTreeEvent copy(event);
          copy.SetInt(1);
          copy.SetEventType(wxEVT_TREE_CONTEXT_MENU);
          AddPendingEvent(copy);
        }

    }

}

void ProjectExplorerWindow::OnTreeItemSelectionChanged(wxTreeEvent& event)
{
    if (!first_click)
    {
        first_click  = true;
        return; // fix wxWidgets bug
    }

    // HATE: So when windows deletes a treeview item it sends a ItemSelectionChange
    // event which in this code will cause the SelectItem function to be called
    // this will cause the DeleteAllItems function to bail but will not unfreeze
    // the tree when we rebuild the list which will then cause it to crash when
    // when we try to change focus to the list. So we just ignore these events if
    // the tree is frozen. This would not happen if Microsoft did not LIE in its
    // documentation.
    if (m_tree->IsFrozen()) {
        event.Skip(false);
        return;
    }

    Project::File* file = NULL;
    wxTreeItemId item = event.GetItem();

    if (item.IsOk())
    {
        m_tree->Freeze();
        m_tree->SelectItem(item);
        m_tree->Thaw();
        ItemData* itemData = GetDataForItem(item);

        if (itemData != NULL && itemData->file != NULL && itemData->isFile)
        {
            file = (Project::File *)itemData->file;
        }

    }
    else
    {
        int a = 0;
    }

    m_infoBox->SetFile(file);
    event.Skip(true);
}

void ProjectExplorerWindow::GetSelectedFiles(std::vector<Project::File*>& selectedFiles) const
{
    wxArrayTreeItemIds selectedIds;
    m_tree->GetSelections(selectedIds);

    for (unsigned int i = 0; i < selectedIds.Count(); ++i)
    {

        ItemData* itemData = GetDataForItem(selectedIds[i]);

        // Only add the selected item if it's a file.
        if (itemData != NULL && itemData->file != NULL && itemData->symbol == NULL && itemData->isFile)
        {
            selectedFiles.push_back((Project::File *)itemData->file);
        }

    }
}

void ProjectExplorerWindow::GetSelectedDirectories(std::vector<Project::Directory*>& selectedDirectories) const
{
  wxArrayTreeItemIds selectedIds;
  m_tree->GetSelections(selectedIds);

  for (unsigned int i = 0; i < selectedIds.Count(); ++i)
  {

    ItemData* itemData = GetDataForItem(selectedIds[i]);

    // Only add the selected item if it's a file.
    if (itemData != NULL && itemData->file != NULL && itemData->isFile == false)
    {
      selectedDirectories.push_back((Project::Directory *)itemData->file);
    }

  }
  
}

void ProjectExplorerWindow::InsertDirectory(Project::Directory* directory)
{
  RebuildForDirectory(directory);
  SortTree(m_tree->GetRootItem());
}

void ProjectExplorerWindow::RemoveDirectory(Project::Directory* directory)
{
  for (Project::File *file : directory->files)
  {
    RemoveFileSymbols(m_tree->GetRootItem(), file);

    if (m_infoBox->GetFile() == file)
    {
      m_infoBox->SetFile(NULL);
    }
  }

  RemoveFileSymbols(m_tree->GetRootItem(), (Project::File *)directory);
}

void ProjectExplorerWindow::InsertFile(Project::File* file)
{
    RebuildForFile(m_root, file);
    SortTree(m_tree->GetRootItem());
}

void ProjectExplorerWindow::RemoveFile(Project::File* file)
{
    RemoveFileSymbols(m_tree->GetRootItem(), file);
    if (m_infoBox->GetFile() == file)
    {
        m_infoBox->SetFile(NULL);
    }

}

void ProjectExplorerWindow::RemoveFiles(const std::vector<Project::File*>& files)
{

    std::unordered_set<Project::File*> fileSet;

    for (unsigned int i = 0; i < files.size(); ++i)
    {
        fileSet.insert(files[i]);
    }

    RemoveFileSymbols(m_tree->GetRootItem(), fileSet);

}

void ProjectExplorerWindow::RemoveFileSymbols(wxTreeItemId node, Project::File* file)
{
    ItemData* data = static_cast<ItemData*>(m_tree->GetItemData(node));
    if (data != NULL && data->file == file)
    {
        m_tree->Collapse(node);
        m_tree->Delete(node);
    }
    else
    {

        // Recurse into the children.

        wxTreeItemIdValue cookie;
        wxTreeItemId child = m_tree->GetFirstChild(node, cookie);

        while (child.IsOk())
        {
            wxTreeItemId next = m_tree->GetNextChild(node, cookie);
            RemoveFileSymbols(child, file);
            child = next;
        }

    }
    
}

void ProjectExplorerWindow::RemoveFileSymbols(wxTreeItemId node, const std::unordered_set<Project::File*>& fileSet)
{

    ItemData* data = static_cast<ItemData*>(m_tree->GetItemData(node));

    if (data != NULL && data->isFile && fileSet.find((Project::File *)data->file) != fileSet.end())
    {
        m_tree->Delete(node);
    }
    else
    {

        // Recurse into the children.

        wxTreeItemIdValue cookie;
        wxTreeItemId child = m_tree->GetFirstChild(node, cookie);

        while (child.IsOk())
        {
            wxTreeItemId next = m_tree->GetNextChild(node, cookie);
            RemoveFileSymbols(child, fileSet);
            child = next;
        }

    }
    
}

void ProjectExplorerWindow::SetFileContextMenu(wxMenu* contextMenu)
{
    m_contextMenu = contextMenu;
}

void ProjectExplorerWindow::SetTemporaryFileContextMenu(wxMenu* contextMenu)
{
    m_temporaryContextMenu = contextMenu;
}


void ProjectExplorerWindow::SetDirectoryContextMenu(wxMenu* contextMenu)
{
    m_directoryContextMenu = contextMenu;
}

int ProjectExplorerWindow::GetImageForFile(const Project::File* file) const
{

    int image = Image_File;
    if (file->temporary)
    {
       image = Image_FileTemp;
    }
  
    return image;
}

void ProjectExplorerWindow::UpdateFileImages()
{

    m_tree->Freeze();

    wxTreeItemIdValue cookie;
    wxTreeItemId item = m_tree->GetFirstChild(m_root, cookie);

    while (item.IsOk())
    {

        ItemData* data = GetDataForItem(item);
        
        if (data != NULL && data->file != NULL && data->symbol == NULL && data->isFile)
        {
            int image = GetImageForFile((Project::File *)data->file);
            m_tree->SetItemImage(item, image);
            m_tree->SetItemImage(item, image, wxTreeItemIcon_Selected);
        }

        item = m_tree->GetNextSibling(item);

    }

    m_tree->Thaw();

}

void ProjectExplorerWindow::OnMenu(wxTreeEvent& event)
{
  if (event.GetInt() == 0)
  {
      m_tree->PopupMenu(m_contextMenu, event.GetPoint());
  }
  else if (event.GetInt() == 2)
  {
      m_tree->PopupMenu(m_temporaryContextMenu, event.GetPoint());
  }
  else if (event.GetInt() == 1)
  {
      wxArrayTreeItemIds selectedItems;
      m_tree->GetSelections(selectedItems);
      if (selectedItems.size() == 1)
      {
         wxTreeItemId item = selectedItems[0];
         wxString name = m_tree->GetItemText(item);
         if (name == "Debug Temporary Files")
             return;
      }

      m_tree->PopupMenu(m_directoryContextMenu, event.GetPoint());
  }
}

void ProjectExplorerWindow::UpdateFile(Project::File* file)
{
    int scroll_pos = m_tree->GetScrollPos(wxVERTICAL);
    m_tree->Freeze();

    wxArrayTreeItemIds selectedItems;
    m_tree->GetSelections(selectedItems);

    wxTreeItemId fileNode = FindFile(m_tree->GetRootItem(), file);
    bool isSelected = m_tree->IsSelected(fileNode);
    if (isSelected)
      m_tree->UnselectItem(fileNode);

    RemoveFileSymbols(m_tree->GetRootItem(), file);

    //If only one item is selected, need to remove it later
    if (isSelected && selectedItems.size() == 1)
    {
      selectedItems.clear();
      m_tree->GetSelections(selectedItems);
      //fileNode = m_tree->UnselectItem
    }

    wxTreeItemId node = m_root;
    if (file->directoryPath.IsEmpty() == false)
    {
      wxStack<wxTreeItemId> stack;

      wxTreeItemIdValue cookie;
      wxTreeItemId temp = m_tree->GetFirstChild(node, cookie);
      wxString target = file->directoryPath;
      
      while (temp.IsOk())
      {
        if (m_tree->GetItemText(temp) == target)
          break;

        temp = m_tree->GetNextChild(temp, cookie);
      }

      if (temp.IsOk() == false)
        temp = m_root;

      node = temp;
    }

    RebuildForFile(node, file);

    SortTree(m_tree->GetRootItem());

    m_tree->SetScrollPos(wxVERTICAL, scroll_pos);
    m_tree->Thaw();
}

void ProjectExplorerWindow::SortTree(wxTreeItemId node)
{
  m_tree->SortChildren(node);

  //Sort all childen recursively
  wxTreeItemIdValue cookie;
  wxTreeItemId temp = m_tree->GetFirstChild(node, cookie);
  while (temp.IsOk())
  {
    if (m_tree->HasChildren(temp))
    {
      SortTree(temp);
    }

    temp = m_tree->GetNextChild(temp, cookie);
  }
}

#include <iostream>
#include <sstream>

#define DBOUT( s )            \
{                             \
   std::wostringstream os_;    \
   os_ << s;                   \
   OutputDebugStringW( os_.str().c_str() );  \
}


wxTreeItemId ProjectExplorerWindow::FindFile(wxTreeItemId node, Project::File *file)
{
  ItemData* data = static_cast<ItemData*>(m_tree->GetItemData(node));
  if (data && data->isFile && data->file == file)
  {
    return node;
  }

  //Sort all childen recursively
  wxTreeItemIdValue cookie;
  wxTreeItemId temp = m_tree->GetFirstChild(node, cookie);
  while (temp.IsOk())
  {
    wxTreeItemId node = FindFile(temp, file);
    if (node.IsOk())
      return node;

    temp = m_tree->GetNextChild(temp, cookie);
  }

  return wxTreeItemId();
}

void ProjectExplorerWindow::TraverseTree(wxTreeItemId node, std::function<void(wxTreeItemId const &)> function) const
{
  //Go through all children in depth first order.
  wxTreeItemIdValue cookie;
  wxTreeItemId temp = m_tree->GetFirstChild(node, cookie);
  while (temp.IsOk())
  {
    TraverseTree(temp, function);
    function(temp);

    temp = m_tree->GetNextChild(temp, cookie);
  }
}


void ProjectExplorerWindow::SaveExpansion()
{
  m_expandedIds.clear();

  //Save all nodes that are expanded
  TraverseTree(m_root, [&, this](wxTreeItemId const &item)
  {
    wxTreeItemId node = m_tree->GetItemParent(item);
    ItemData* data = static_cast<ItemData*>(m_tree->GetItemData(item));

    //If the file has a parent and it is expanded, add it to the list
    if (node != m_root && node.IsOk() && data && data->isFile && m_tree->IsExpanded(node))
    {
      m_expandedIds.push_back(data->file);
    }
  });
}

void ProjectExplorerWindow::LoadExpansion()
{
  //Only expand the previously expanded items.
  TraverseTree(m_root, [&, this](wxTreeItemId const &item)
  {
    ItemData* data = static_cast<ItemData*>(m_tree->GetItemData(item));
    for (void* expand_file : m_expandedIds)
    {
      if (data && data->file == expand_file)
      {
        wxStack<wxTreeItemId> treeStack;

        //Collect all parent nodes
        wxTreeItemId node = m_tree->GetItemParent(item);
        while (node.IsOk())
        {
          if (node != m_root)
            treeStack.push(node);

          node = m_tree->GetItemParent(node);
        }

        //Expand them in reverse order
        while (treeStack.size() > 0)
        {
          node = treeStack.top();
          treeStack.pop();

          m_tree->Expand(node);
        }

        m_tree->Collapse(item);
      }
    }
  });
}

void ProjectExplorerWindow::ExpandFromFile(Project::File *file)
{
  //Only expand the previously expanded items.
  TraverseTree(m_root, [&, this](wxTreeItemId const &item)
  {
    ItemData* data = static_cast<ItemData*>(m_tree->GetItemData(item));
    if (data && data->file == file)
    {
      wxStack<wxTreeItemId> treeStack;

      //Collect all parent nodes
      wxTreeItemId node = m_tree->GetItemParent(item);
      while (node.IsOk())
      {
        if (node != m_root)
          treeStack.push(node);

        node = m_tree->GetItemParent(node);
      }

      //Expand them in reverse order
      while (treeStack.size() > 0)
      {
        node = treeStack.top();
        treeStack.pop();

        m_tree->Expand(node);
      }

      m_tree->Collapse(item);
    }
  });
}

wxString ProjectExplorerWindow::GetSelectedDirectoryName()
{
  wxArrayTreeItemIds array;
  m_tree->GetSelections(array);

  if (array.size() != 1)
    return wxString();

  wxTreeItemId selection = array[0];

  if (selection.IsOk())
  {
    ItemData* itemData = GetDataForItem(selection);

    if (itemData == NULL)
    {
      wxStack<wxTreeItemId> treeStack;

      treeStack.push(selection);

      //Collect all parent nodes
      wxTreeItemId node = m_tree->GetItemParent(selection);
      while (node.IsOk())
      {
        if (node != m_root)
          treeStack.push(node);

        node = m_tree->GetItemParent(node);
      }

      wxString directory;
      
      //Expand them in reverse order
      while (treeStack.size() > 0)
      {
        node = treeStack.top();
        treeStack.pop();

        directory += m_tree->GetItemText(node) + '\\';
      }

      return directory;
    }
  }

  return wxString();
}


IMPLEMENT_DYNAMIC_CLASS(wxProjectTree, wxTreeCtrl);
int wxProjectTree::OnCompareItems(const wxTreeItemId &item1, const wxTreeItemId &item2)
{
  ProjectExplorerWindow::ItemData* data1 = static_cast<ProjectExplorerWindow::ItemData*>(GetItemData(item1));
  ProjectExplorerWindow::ItemData* data2 = static_cast<ProjectExplorerWindow::ItemData*>(GetItemData(item2));

  //if (data1 && data2)
  //{
  if (data1 && data2)
    return GetItemText(item1).Cmp(GetItemText(item2));
  else if (data1 || data2)
  {
    if (data1) return 1;
    else return -1;
  }
  //}

  return GetItemText(item1).Cmp(GetItemText(item2));
}
