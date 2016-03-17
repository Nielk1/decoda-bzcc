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

#ifndef SYMBOL_H
#define SYMBOL_H

#include <wx/wx.h>
#include <wx/string.h>

enum class SymbolSearch
{
  Standard,
  PrefixOnly
};

enum class SymbolType : int
{
  Unknown    = 0x00,
  Function   = 0x01,
  Type       = 0x02,
  Module     = 0x04,
  Variable   = 0x08,
  Prefix     = 0x10,
  Assignment = 0x20,
};

using T = std::underlying_type<SymbolType>::type;

inline SymbolType operator | (SymbolType lhs, SymbolType rhs)
{
  return (SymbolType)(static_cast<T>(lhs) | static_cast<T>(rhs));
}

inline bool operator & (SymbolType lhs, SymbolType rhs)
{
  return (static_cast<T>(lhs) & static_cast<T>(rhs)) != 0;
}

class Symbol
{
public:
    Symbol();
    Symbol(Symbol *parent, const wxString& name, unsigned int line, SymbolType type = SymbolType::Function);
    bool operator<(const Symbol& entry) const;
    static const SymbolType Type_Standard = (SymbolType)((int)SymbolType::Function | (int)SymbolType::Type | (int)SymbolType::Module | (int)SymbolType::Variable);
public:

  wxString GetModuleName();
  wxString GetParentsName();
  Symbol *GetCurrentModule();
  wxString GetScope(int level = 0);
  wxString GetTooltip();

  Symbol*             parent = nullptr;
  Symbol*             typeSymbol = nullptr;
  wxVector<Symbol *>  children;
  wxString            name;
  unsigned int        line;
  SymbolType          type;
  wxString            requiredModule;
  wxString            rhs;
};

#endif