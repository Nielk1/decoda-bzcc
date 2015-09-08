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

#include "Symbol.h"

Symbol::Symbol()
{
    line = 0;
}

Symbol::Symbol(Symbol *_parent, const wxString& _name, unsigned int _line, SymbolType _type)
  :requiredModule(wxString())
{
  parent = _parent;
  name   = _name;
  line   = _line;
  type   = _type;

  if (parent)
  {
    parent->children.push_back(this);
  }
}

bool Symbol::operator<(const Symbol& symbol) const
{
  return name.Cmp(symbol.name) < 0;
}

wxString Symbol::GetScope(int level)
{
  if (parent)
  {
    wxString str = parent->GetScope(level + 1);
    if (level == 0)
      return str;
    else if (level == 1)
      return str + name;
    else
      return str + name + ".";
  }
  else
  {
    if (level == 0)
      return "";
    else if (level > 1)
      return name + ".";
    else
      return name;
  }

  return "";
}


wxString Symbol::GetModuleName()
{
  if (parent)
    return parent->GetModuleName() + "." + name;
  else
    return name;
}

wxString Symbol::GetParentsName()
{
  if (parent)
    return parent->name;
  else
    return wxString();
}

Symbol *Symbol::GetCurrentModule()
{
  if (type == SymbolType::Module && parent == nullptr)
    return this;
  else if (parent)
  {
    return parent->GetCurrentModule();
  }

  return nullptr;
}