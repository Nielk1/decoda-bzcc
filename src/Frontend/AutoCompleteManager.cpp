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

#include "AutoCompleteManager.h"
#include "Symbol.h"

#include <algorithm>

AutoCompleteManager::Entry::Entry()
{
}

AutoCompleteManager::Entry::Entry(const wxString& _name, Type _type, const Project::File *file, Symbol *symbol)
  : name(_name), type(_type), file(file), symbol(symbol)
{
    lowerCaseName = name.Lower();
    scope = symbol->GetScope();
}

bool AutoCompleteManager::Entry::operator<(const Entry& entry) const
{
    return name.Cmp(entry.name) < 0;
}

void AutoCompleteManager::BuildFromProject(const Project* project)
{
    
    m_entries.clear();
    m_prefixModules.clear();
    m_prefixNames.clear();


    for (unsigned int fileIndex = 0; fileIndex < project->GetNumFiles(); ++fileIndex)
    {
        BuildFromFile(project->GetFile(fileIndex));
    }
    for (unsigned int directoryIndex = 0; directoryIndex < project->GetNumDirectories(); ++directoryIndex)
    {
      Project::Directory const *directory = project->GetDirectory(directoryIndex);

      for (unsigned int fileIndex = 0; fileIndex < directory->files.size(); ++fileIndex)
      {
        BuildFromFile(directory->files[fileIndex]);
      }
    }

    // Sort the autocompletions (necessary for binary search).
    std::sort(m_entries.begin(), m_entries.end());

}

void AutoCompleteManager::BuildFromFile(const Project::File* file)
{

    for (unsigned int symbolIndex = 0; symbolIndex < file->symbols.size(); ++symbolIndex)
    {
        Symbol* symbol = file->symbols[symbolIndex];
        if (symbol->type == SymbolType::Prefix)
        {
          if (symbol->requiredModule.IsEmpty() == false)
            m_prefixModules.push_back(Entry(symbol->name, Type_Function, file, symbol));
          else
            m_prefixNames.push_back(Entry(symbol->name, Type_Function, file, symbol));
        }
        else if (symbol->type != SymbolType::Prefix)
          m_entries.push_back(Entry(symbol->name, Type_Function, file, symbol));
    }

}

void AutoCompleteManager::ParsePrefix(wxString& prefix, const Project::File *file, int current_line) const
{
  const Entry *closest_entry = nullptr;
  unsigned closest_length = UINT_MAX;

  for (unsigned int i = 0; i < m_entries.size(); ++i)
  {
    Entry const &entry = m_entries[i];
    if (entry.file == file && entry.type == Type_Function)
    {
      int line_difference = current_line - (int)entry.symbol->line;
      if (line_difference > 0 && line_difference < closest_length)
      {
        closest_length = line_difference;
        closest_entry = &entry;
      }
    }
  }

  Symbol *module = nullptr;
  if (closest_entry != nullptr)
    module = closest_entry->symbol->GetCurrentModule();

  for (const Entry &entry : m_prefixNames)
  {
    if (prefix == entry.name)
    {
      wxString replacement;
      if (module != nullptr)
      {
        for (const Entry &module_entry : m_prefixModules)
        {
          //If the module's parent matches the matched name's symbol, we can start searching for a replacement
          if (module_entry.symbol->parent == entry.symbol && module_entry.symbol->requiredModule == module->name)
          {
            replacement = module_entry.name;
            break;
          }
        }
      }

      //If the string is empty, either we have no module or the module search failed. In either case, go with the default.
      if (replacement.IsEmpty())
      {
        for (const Entry &module_entry : m_prefixModules)
        {
          //If the module's parent matches the matched name's symbol, and the required module is the matched name, it is the deault
          if (module_entry.symbol->parent == entry.symbol && module_entry.symbol->requiredModule == entry.name)
          {
            replacement = module_entry.name;
            break;
          }
        }
      }

      if (replacement.IsEmpty() == false)
      {
        if (replacement == "__FILENAME__")
          replacement = file->fileName.GetName();
        else if (replacement == "__MODULENAME__")
        {
          if (module)
            replacement = module->name;
          else
            replacement = "nil";
        }
        
        prefix = replacement;
      }
    }
  }
}

void AutoCompleteManager::GetMatchingItems(const wxString& token, const wxString& prefix, bool member, wxString& items) const
{
    // Autocompletion selection is case insensitive so transform everything
    // to lowercase.
    wxString test = token.Lower();

    // Add the items to the list that begin with the specified prefix. This
    // could be done much fater with a binary search since our items are in
    // alphabetical order.

    wxVector<const Entry *> matches;

    for (unsigned int i = 0; i < m_entries.size(); ++i)
    {
      // Check that the scope is correct.
      if (m_entries[i].symbol->type == SymbolType::Module)
        continue;

        bool inScope = false;

        if (/**/true)
        {
            // We've got no way of knowing the type of the variable in Lua (since
            // variables don't have types, only values have types), so we display
            // all members if the prefix contains a member selection operator (. or :)
            if (!m_entries[i].scope.IsEmpty() && member)
            {
              inScope = m_entries[i].scope == prefix;
            }
            else
            {
              inScope = m_entries[i].scope.IsEmpty() != member;
            }
        }
        
        if (inScope && m_entries[i].lowerCaseName.StartsWith(test))
        {
          bool canPush = true;
          for (const Entry *entry : matches)
          {
            if (entry->name == m_entries[i].name)
            {
              canPush = false;
              break;
            }
          }

          if (canPush)
            matches.push_back(&m_entries[i]);
        }
    }

    for (const Entry *entry : matches)
    {
      items += entry->name;

      // Add the appropriate icon for the type of the identifier.
      if (entry->type != Type_Unknown)
      {
        items += "?";
        items += '0' + entry->type;
      }

      items += ' ';
    }

}