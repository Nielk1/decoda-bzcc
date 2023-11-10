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
#include <wx/tokenzr.h>
#include "Tokenizer.h"

AutoCompleteManager::Entry::Entry(const wxString& _name, Type _type, const Project::File *file, Symbol *symbol)
  : name(_name), type(_type), file(file), symbol(symbol)
{
    lowerCaseName = name.Lower();
    if (symbol)
     scope = symbol->GetScope();
}

AutoCompleteManager::Entry::~Entry()
{
}

bool AutoCompleteManager::Entry::operator<(const Entry& entry) const
{
    return name.Cmp(entry.name) < 0;
}

AutoCompleteManager::AutoCompleteManager()
{
}
 
AutoCompleteManager::~AutoCompleteManager()
{
    ClearAllEntries();
}

void AutoCompleteManager::ClearEntries(const Project::File *file)
{
   wxVector<Symbol *> removedSymbols;
   auto ClearFromEntries = [&removedSymbols](std::vector<Entry> &entries, const Project::File *file){
    for (size_t i = 0; i < entries.size();)
    {
      if (entries[i].file == file)
      {
        removedSymbols.push_back(entries[i].symbol);
        std::swap(entries[i], entries[entries.size() - 1]);
        entries.pop_back();
      }
      else
        ++i;
    }
  };

  //Clear entries
  ClearFromEntries(m_entries, file);
  ClearFromEntries(m_prefixModules, file);
  ClearFromEntries(m_prefixNames, file);
  ClearFromEntries(m_assignments, file);
  //ClearFromEntries(m_languageEntries, file);

  if (removedSymbols.empty())
      return;

#ifdef _DEBUG
   wxString temp("Unload symbols: ");
   temp.Append(file->fileName.GetFullName());
   temp.Append("\r\n");
   OutputDebugString(temp);
#endif

  //Remove symbols from the deleted entries
  for (size_t i = 0; i < m_symbols.size();)
  {
    bool deleted = false;
    for (Symbol *s : removedSymbols)
    {
      if (m_symbols[i] == s)
      {
        delete s;
        std::swap(m_symbols[i], m_symbols[m_symbols.size() - 1]);
        m_symbols.pop_back();
        deleted = true;

        if (i == m_symbols.size())
          break;
      }
    }
    if (!deleted)
      ++i;
   }
}

void AutoCompleteManager::ClearAllEntries()
{
    m_entries.clear();
    m_prefixModules.clear();
    m_prefixNames.clear();
    m_assignments.clear();
    std::vector<Symbol *>::iterator it = m_symbols.begin(), it_end = m_symbols.end();
    for(;it!=it_end;++it) {delete (*it);};
    m_symbols.clear();
}

void AutoCompleteManager::BuildFromProject(const Project* project)
{
  if (m_firstBuild)
  {
    m_languageEntries.push_back(Entry("and", Type_Function, nullptr, nullptr));
    m_languageEntries.push_back(Entry("break", Type_Function, nullptr, nullptr));
    m_languageEntries.push_back(Entry("do", Type_Function, nullptr, nullptr));
    m_languageEntries.push_back(Entry("else", Type_Function, nullptr, nullptr));
    m_languageEntries.push_back(Entry("elseif", Type_Function, nullptr, nullptr));
    m_languageEntries.push_back(Entry("end", Type_Function, nullptr, nullptr));
    m_languageEntries.push_back(Entry("false", Type_Function, nullptr, nullptr));
    m_languageEntries.push_back(Entry("for", Type_Function, nullptr, nullptr));
    m_languageEntries.push_back(Entry("function", Type_Function, nullptr, nullptr));
    m_languageEntries.push_back(Entry("if", Type_Function, nullptr, nullptr));
    m_languageEntries.push_back(Entry("in", Type_Function, nullptr, nullptr));
    m_languageEntries.push_back(Entry("local", Type_Function, nullptr, nullptr));
    m_languageEntries.push_back(Entry("nil", Type_Function, nullptr, nullptr));
    m_languageEntries.push_back(Entry("not", Type_Function, nullptr, nullptr));
    m_languageEntries.push_back(Entry("or", Type_Function, nullptr, nullptr));
    m_languageEntries.push_back(Entry("repeat", Type_Function, nullptr, nullptr));
    m_languageEntries.push_back(Entry("return", Type_Function, nullptr, nullptr));
    m_languageEntries.push_back(Entry("then", Type_Function, nullptr, nullptr));
    m_languageEntries.push_back(Entry("true", Type_Function, nullptr, nullptr));
    m_languageEntries.push_back(Entry("until", Type_Function, nullptr, nullptr));
    m_languageEntries.push_back(Entry("while", Type_Function, nullptr, nullptr));
    m_firstBuild = false;
  }

  wxVector<Project::File *> updatedFiles;
  for (unsigned int fileIndex = 0; fileIndex < project->GetNumFiles(); ++fileIndex)
  {
    Project::File *file = (Project::File *)project->GetFile(fileIndex);
    if (file->symbolsUpdated)
      updatedFiles.push_back(file);
  }

  for (unsigned int directoryIndex = 0; directoryIndex < project->GetNumDirectories(); ++directoryIndex)
  {
    Project::Directory const *directory = project->GetDirectory(directoryIndex);
    for (unsigned int fileIndex = 0; fileIndex < directory->files.size(); ++fileIndex)
    {
      Project::File *file = (Project::File *)directory->files[fileIndex];
      if (file->symbolsUpdated)
        updatedFiles.push_back(file);
    }
  }

  //Clear entries
  for (const Project::File *file : updatedFiles)
  {
    ClearEntries(file);
  }

#ifdef _DEBUG
  if (!updatedFiles.empty())
  {
      for (const Project::File *file : updatedFiles)
      {
        wxString temp("Load symbols: ");
        temp.Append(file->fileName.GetFullName());
        temp.Append("\r\n");
        OutputDebugString(temp);
      }
  }
#endif

  //Rebuild symbols
  for (const Project::File *file : updatedFiles)
  {
    BuildFromFile(file);
  }

  for (Entry &entry : m_assignments)
  {
    if (entry.file->symbolsUpdated)
    {
      wxVector<wxString> prefixes;
      ParsePrefix(entry.symbol->rhs, entry.file, entry.symbol->line, prefixes, true);
      if (prefixes.size() >= 1)
        entry.symbol->rhs = prefixes[1];
    }
  }

  for (Project::File *file : updatedFiles)
  {
    file->symbolsUpdated = false;
  }

  // Sort the autocompletions (necessary for binary search).
  std::sort(m_entries.begin(), m_entries.end());

}

Symbol *GetSymbol(wxString name, std::vector<Symbol*>& symbols, SymbolType search = Symbol::Type_Standard, bool onlyRoot = false);

Symbol *AddSymbolRecursive(Symbol *symbol, std::vector<Symbol *> &symbols)
{
  if (symbol == nullptr)
    return nullptr;

  SymbolType search = Symbol::Type_Standard;
  if ((symbol->type & search) == false)
  {
    search = symbol->type;
  }

  Symbol *parent = symbol->parent;
  if (parent)
  {
    parent = AddSymbolRecursive(parent, symbols);
    Symbol *new_symbol = new Symbol(parent, symbol->name, symbol->line, symbol->type);
    symbols.push_back(new_symbol);

    new_symbol->requiredModule = symbol->requiredModule;
    new_symbol->rhs = symbol->rhs;
    new_symbol->typeSymbol = AddSymbolRecursive(symbol->typeSymbol, symbols);

    symbol = new_symbol;
  }
  else
  {
    Symbol *foundSymbol = GetSymbol(symbol->name, symbols, search, true);
    if (foundSymbol == nullptr)
    {
      foundSymbol = new Symbol(parent, symbol->name, symbol->line, symbol->type);
      symbols.push_back(foundSymbol);

      foundSymbol->requiredModule = symbol->requiredModule;
      foundSymbol->rhs = symbol->rhs;
      foundSymbol->typeSymbol = AddSymbolRecursive(symbol->typeSymbol, symbols);
    }

    symbol = foundSymbol;
  }

  return symbol;
}

void AutoCompleteManager::BuildFromFile(const Project::File* file)
{

    for (unsigned int symbolIndex = 0; symbolIndex < file->symbols.size(); ++symbolIndex)
    {
      if (file->symbols[symbolIndex]->type == SymbolType::Assignment)
      {
        m_assignments.push_back(Entry(file->symbols[symbolIndex]->name, Type_Variable, file, file->symbols[symbolIndex]));
        continue;
      }

        Symbol* symbol = AddSymbolRecursive(file->symbols[symbolIndex], m_symbols);
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

wxString GetNextToken(wxString& string, unsigned int &str_pos)
{
  wxString token;

  auto AddToToken = [&string, &token, &str_pos]()
  {
    if (str_pos < string.length())
    {
      token.Append(string[str_pos]);
      ++str_pos;
    }
  };

  for (; str_pos < string.length();)
  {
    if (string[str_pos] == '.' || string[str_pos] == ':')
    {
      str_pos++;
      break;
    }
    else if (string[str_pos] == '(' || string[str_pos] == '[')
    {
      WCHAR open;
      WCHAR close;

      if (string[str_pos] == '(')
      {
        open = '(';
        close = ')';
      }
      else if (string[str_pos] == '[')
      {
        open = '[';
        close = ']';
      }

      AddToToken();

      int parenStack = 0;
      for (; str_pos < string.length();)
      {
        if (string[str_pos] == close)
        {
          if (parenStack == 0)
            break;
          parenStack--;
        }

        if (string[str_pos] == open)
          parenStack++;

        AddToToken();
      }
    }

    AddToToken();
  }

  return token;
}

void AutoCompleteManager::ParsePrefix(wxString& prefix, const Project::File *file, int current_line, wxVector<wxString> &prefixes, bool parsing_assignment) const
{
  unsigned int str_pos = 0;
  //wxStringTokenizer tokenizer(prefix, wxT(".:"));

  wxVector<wxString> tokens;
  for (;;)
  {
    wxString token = GetNextToken(prefix, str_pos);

    if (token.empty())
      break;
    

    tokens.push_back(token);
  }

  if (!parsing_assignment)
  {

    //Try to decode entire symbol if possible
    wxString tempString;
    for (size_t i = 0; i < tokens.size(); ++i)
    {
      if (i == tokens.size() - 1)
      {
        tempString += tokens[i];
      }
      else
        tempString += tokens[i] + ".";
    }

    const Entry *closest_entry = nullptr;
    int closest_length = INT_MAX;
    for (Entry const &entry : m_assignments)
    {
      if (entry.file == file && entry.name == tempString)
      {
        int line_difference = current_line - (int)entry.symbol->line;
        if (line_difference >= 1 && line_difference < closest_length)
        {
          closest_length = line_difference;
          closest_entry = &entry;
        }
      }
    }

    if (closest_entry != nullptr)
    {
      wxString newToken = closest_entry->symbol->rhs;

      int end1 = newToken.Find('.', true);
      if (end1 == wxNOT_FOUND)
        end1 = 0;
      else
        // Skip the '.' character.
        ++end1;

      int end2 = newToken.Find(':', true);
      if (end2 == wxNOT_FOUND)
        end2 = 0;
      else
        // Skip the ':' character.
        ++end2;

      if (!(newToken == prefix && closest_entry->symbol->line == current_line))
      {
        wxVector<wxString> new_prefixes;
        ParsePrefix(newToken, file, closest_entry->symbol->line, new_prefixes, parsing_assignment);

        newToken = new_prefixes[0];
      }

      prefixes.push_back(newToken);
      prefixes.push_back(newToken);
      return;
    }

    //No exact match, try to help with a substring
    int end1 = tempString.Find('.', false);
    if (end1 == wxNOT_FOUND)
      end1 = 0;

    int end2 = tempString.Find(':', false);
    if (end2 == wxNOT_FOUND)
      end2 = 0;

    closest_entry = nullptr;
    closest_length = INT_MAX;
    unsigned int closest_name_length = std::max(end1, end2);

    for (Entry const &entry : m_assignments)
    {
      if (entry.file == file)
      {
        if (tempString.StartsWith(entry.name) && entry.name.length() >= closest_name_length)
        {
          int line_difference = current_line - (int)entry.symbol->line;
          if (line_difference >= 1 && line_difference < closest_length)
          {
            closest_length = line_difference;
            closest_entry = &entry;
            closest_name_length = entry.name.length();
          }
        }
      }
    }

    //We found a substring, replace it and re-tokenize
    if (closest_entry)
    {
      wxString right = tempString.Right(tempString.length() - closest_name_length);
      wxString newToken = closest_entry->symbol->rhs;

      int end1 = newToken.Find('.', true);
      if (end1 == wxNOT_FOUND)
        end1 = 0;
      else
        // Skip the '.' character.
        ++end1;

      int end2 = newToken.Find(':', true);
      if (end2 == wxNOT_FOUND)
        end2 = 0;
      else
        // Skip the ':' character.
        ++end2;

      int end = std::max(end1, end2);

      tempString = newToken.Right(newToken.Length() - end) + right;
      str_pos = 0;
      tokens.clear();
      for (;;)
      {
        wxString token = GetNextToken(tempString, str_pos);
        if (token.empty())
          break;

        tokens.push_back(token);
      }
    }
  }

  for (wxString &token : tokens)
  {
    const Entry *closest_entry = nullptr;
    int closest_length = INT_MAX;

    for (unsigned int i = 0; i < m_entries.size(); ++i)
    {
      Entry const &entry = m_entries[i];
      if (entry.file == file && entry.type == Type_Function)
      {
        int line_difference = current_line - (int)entry.symbol->line;
        if (line_difference > 1 && line_difference < closest_length)
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
      if (token == entry.name)
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

          token = replacement;
        }
      }
    }
  }

  //Try to decode token using types
  Symbol *currentToken = nullptr;
  if (tokens.size() >= 1)
  {
    for (wxString &token : tokens)
    {
      //Strip function call
      if (token[token.size() - 1] == ')')
      {
        int paren_loc = -1;
        int paren_stack = 0;
        for (int i = token.size() - 2; i >= 0; --i)
        {
          if (token[i] == '(' && paren_stack == 0)
          {
            paren_loc = i;
            break;
          }
          if (token[i] == ')')
            paren_stack--;
          if (token[i] == '(')
            paren_stack++;
        }

        if (paren_loc != -1)
          token.RemoveLast(token.size() - paren_loc);
      }
    }

    for (const Entry &entry : m_entries)
    {
      if (entry.symbol->parent == nullptr && entry.name == tokens[0])
      {
        currentToken = entry.symbol;
        break;
      }
    }

    if (tokens.size() == 1 && currentToken && currentToken->typeSymbol)
    {
      tokens[0] = currentToken->typeSymbol->name;
    }

    //Go through the symbols to find the true type
    Symbol *foundSymbol = nullptr;
    for (size_t i = 1; i < tokens.size(); ++i)
    {
      if (currentToken != nullptr)
      {
        wxString token = tokens[i];

        for (Symbol *symbol : currentToken->children)
        {
          if (symbol->name == token)
          {
            foundSymbol = symbol;
            break;
          }
        }

        //If we found a symbol replace it with its type
        if (foundSymbol && foundSymbol->typeSymbol)
        {
          tokens[i] = foundSymbol->typeSymbol->name;
          foundSymbol = foundSymbol->typeSymbol;
        }
        else
          foundSymbol = nullptr;

        currentToken = foundSymbol;
      }
    }
  }


  wxString totalString;
  for (size_t i = 0; i < tokens.size(); ++i)
  {
    if (i == tokens.size() - 1)
    {
      prefixes.push_back(tokens[i]);
      totalString += tokens[i];
      prefixes.push_back(totalString);
    }
    else
      totalString += tokens[i] + ".";
  }
}

void AutoCompleteManager::GetMatchingItems(const wxString& token, const wxVector<wxString> &prefixes, bool member, bool function, wxString& items, const wxString& fullToken, wxVector<wxString> &tooltips) const
{
    // Autocompletion selection is case insensitive so transform everything
    // to lowercase.
    wxString test = token.Lower();

    if (!member)
    {
      for (unsigned int i = 0; i < m_languageEntries.size(); ++i)
      {
        if (m_languageEntries[i].name == test)
        {
          items.Empty();
          return;
        }

        if (m_languageEntries[i].lowerCaseName.StartsWith(test))
        {
          items += m_languageEntries[i].name;
          items += ' ';

          tooltips.push_back("");
        }
      }
    }

    // Add the items to the list that begin with the specified prefix. This
    // could be done much fater with a binary search since our items are in
    // alphabetical order.

    wxVector<const Entry *> matches;
    wxVector<wxString> assign_matches;

    for (unsigned int i = 0; i < m_entries.size(); ++i)
    {
      // Check that the scope is correct.
      if (m_entries[i].symbol->type == SymbolType::Module)
        continue;
      
      //if (m_entries[i].symbol->type == SymbolType::Function && !function)
      //  continue;
      //
      //if (m_entries[i].symbol->type == SymbolType::Variable && function)
      //  continue;

      bool inScope = false;

      if (/**/true)
      {
          // We've got no way of knowing the type of the variable in Lua (since
          // variables don't have types, only values have types), so we display
          // all members if the prefix contains a member selection operator (. or :)
          if (!m_entries[i].scope.IsEmpty() && member)
          {
            for (wxString const &prefix : prefixes)
            {
              inScope = inScope || m_entries[i].scope == prefix;
            }
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
        {
          matches.push_back(&m_entries[i]);
          tooltips.push_back(m_entries[i].symbol->GetTooltip());
        }
      }
    }

    for (unsigned int i = 0; i < m_assignments.size(); ++i)
    {
      if (m_assignments[i].name.StartsWith(fullToken))
      {
        wxString const &str = m_assignments[i].name;
        size_t end = str.Length() - 1;

        for (size_t k = test.Length() - 1; k < str.Length(); ++k)
        {
          if (IsSymbol(str[k]))
          {
            end = k - 1;
            break;
          }
        }

        size_t start = 0;
        for (size_t k = end; k > 0; --k)
        {
          if (IsSymbol(str[k]))
          {
            start = k + 1;
            break;
          }
        }

        wxString newToken = str.SubString(start, end);

        bool canPush = true;
        for (const Entry *entry : matches)
        {
          if (entry->name == newToken)
          {
            canPush = false;
            break;
          }
        }

        for (const wxString &str : assign_matches)
        {
          if (str == newToken)
          {
            canPush = false;
            break;
          }
        }

        if (canPush)
        {
          assign_matches.push_back(newToken);
          tooltips.push_back(m_assignments[i].symbol->GetTooltip());
        }
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

    for (const wxString &str : assign_matches)
    {
      items += str;
      items += ' ';
    }
}

void AutoCompleteManager::GetAllItems(wxVector<AutoCompleteManager::Entry>& items) const
{
    items.empty();

    //for (unsigned int i = 0; i < m_languageEntries.size(); ++i)
    //{
    //    items.push_back(m_languageEntries[i]);
    //}

    for (unsigned int i = 0; i < m_entries.size(); ++i)
    {
        items.push_back(m_entries[i]);
    }

    for (unsigned int i = 0; i < m_prefixModules.size(); ++i)
    {
        items.push_back(m_prefixModules[i]);
    }

    for (unsigned int i = 0; i < m_prefixNames.size(); ++i)
    {
        items.push_back(m_prefixNames[i]);
    }

    for (unsigned int i = 0; i < m_assignments.size(); ++i)
    {
        items.push_back(m_assignments[i]);
    }
}