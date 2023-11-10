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

#include "SymbolParserThread.h"
#include "SymbolParserEvent.h"
#include "Symbol.h"
#include "StlUtility.h"
#include "Tokenizer.h"
#include "OutputWindow.h"

#include <wx/wfstream.h>
#include <wx/sstream.h>
#include <wx/stack.h>

SymbolParserThread::SymbolParserThread() : wxThread(wxTHREAD_JOINABLE), m_itemsAvailable(0, 1)
{
    m_exit          = false;
    m_headItem      = NULL;
    m_eventHandler  = NULL;
}

void SymbolParserThread::SetEventHandler(wxEvtHandler* eventHandler)
{
    m_eventHandler = eventHandler;
}

void SymbolParserThread::Stop()
{

    // Clear the event handler so that we don't post a new message to it. If we did,
    // that message could be processed in the next event loop after the stop.
    m_headLock.Enter();
    m_eventHandler = NULL;
    m_headLock.Leave();

    // Tell the thread to exit.
    m_exit = true;
    m_itemsAvailable.Post();

    Wait();

}

wxThread::ExitCode SymbolParserThread::Entry()
{
    
    while (!TestDestroy() && !m_exit)
    {

        // Wait for something to show up in the queue.
        m_itemsAvailable.Wait();

        while (!TestDestroy())
        {

            m_itemsLock.Enter();

            if (m_items.empty())
            {
                m_itemsLock.Leave();
                break;
            }

            wxCriticalSectionLocker locker(m_headLock);

            m_headItem = m_items.back();
            m_items.pop_back();

            m_itemsLock.Leave();

            if (m_eventHandler != NULL)
            {

                std::vector<Symbol*> symbols;
                wxStringInputStream input(m_headItem->code);

                ParseFileSymbols(input, symbols);

                m_itemsLock.Enter();
                bool isLastItem=m_items.empty();
                m_itemsLock.Leave();

                // Dispatch the message to event handler.
                SymbolParserEvent event(m_headItem->fileId, symbols, isLastItem);
                m_eventHandler->AddPendingEvent(event);
            }

            delete m_headItem;
            m_headItem = NULL;

        }

    }

    wxCriticalSectionLocker locker1(m_itemsLock);
    ClearVector(m_items);

    wxCriticalSectionLocker locker2(m_headLock);
    delete m_headItem;
    m_headItem = NULL;
    
    return 0;

}

void SymbolParserThread::QueueForParsing(const wxString& code, unsigned int fileId)
{

    QueueItem* item = new QueueItem;
    
    item->code = code;
    item->fileId = fileId;

    {
        wxCriticalSectionLocker locker(m_itemsLock);
        m_items.push_back(item);
    }
    
    // Signal that we have data available.
    m_itemsAvailable.Post();

}

Symbol *GetSymbol(wxString name, std::vector<Symbol*>& symbols, SymbolType search = Symbol::Type_Standard, bool onlyRoot = false)
{
  for (Symbol *symbol : symbols)
  {
    if (symbol->name == name)
    {
      Symbol *found = nullptr;

      if (symbol->type & search)
        found = symbol;

      if (found)
      {
        if (onlyRoot == true && found->parent == nullptr)
          return found;
        else if (onlyRoot == false)
          return found;
      }
    }
  }

  return nullptr;
}

struct Token
{
  Token(wxString const &t, int ln)
    :token(t), lineNumber(ln)
  {}

  wxString token;
  unsigned int lineNumber;
};

wxString PeekPrevToken(std::vector<Token> const &tokens, unsigned int &iter, unsigned int &lineNumber)
{
  if (iter > 0)
  {
    lineNumber = tokens[iter - 1].lineNumber;
    return tokens[iter - 1].token;
  }

  return "";
}

wxString PeekNextToken(std::vector<Token> const &tokens, unsigned int &iter, unsigned int &lineNumber)
{
  if (iter + 1 < tokens.size())
  {
    lineNumber = tokens[iter + 1].lineNumber;
    return tokens[iter + 1].token;
  }

  return "";
}

bool GetPrevToken(std::vector<Token> const &tokens, wxString &str, unsigned int &lineNumber, unsigned int &iter)
{
  if (iter > 0)
  {
    iter -= 1;
    str = tokens[iter].token;
    lineNumber = tokens[iter].lineNumber;
    return true;
  }
  iter -= 1;
  return false;
}

bool GetNextToken(std::vector<Token> const &tokens, wxString &str, unsigned int &lineNumber, unsigned int &iter)
{
  iter += 1;
  if (iter < tokens.size())
  {
    str = tokens[iter].token;
    lineNumber = tokens[iter].lineNumber;
    return true;
  }

  return false;
}

Symbol *DecodaDefFunction(std::vector<Symbol*>& symbols, unsigned int &lineNumber, Symbol *module, std::vector<Token> const &tokens, unsigned int &current_token, wxString token)
{
  SymbolType type = SymbolType::Function;
  if (token == "__variable")
  {
    type = SymbolType::Variable;
    if (!GetNextToken(tokens, token, lineNumber, current_token)) return nullptr;
  }

  if (type == SymbolType::Function && token == "operator")
  {
    wxString oper;
    if (!GetNextToken(tokens, oper, lineNumber, current_token)) return nullptr;

    while (oper == "+" || oper == "-" || oper == "*" || oper == "/" ||
      oper == "=" || oper == "<" || oper == ">" || oper == "!" ||
      oper == "(" || oper == ")")
    {
      token += oper;
      if (!GetNextToken(tokens, oper, lineNumber, current_token)) return nullptr;
    }

    current_token--;
  }

  wxString typeName;
  if (!GetNextToken(tokens, typeName, lineNumber, current_token)) return nullptr;

  Symbol *lastModule = new Symbol(module, token, lineNumber, type);

  Symbol *typeSymbol = GetSymbol(typeName, symbols);
  if (typeSymbol == nullptr)
  {
    typeSymbol = new Symbol(nullptr, typeName, lineNumber, SymbolType::Type);
    symbols.push_back(typeSymbol);
  }

  lastModule->typeSymbol = typeSymbol;

  symbols.push_back(lastModule);

  return lastModule;
}

void DecodaDefRecursive(std::vector<Symbol*>& symbols, unsigned int &lineNumber, Symbol *module, std::vector<Token> const &tokens, unsigned int &current_token)
{
  wxString token;
  if (!GetNextToken(tokens, token, lineNumber, current_token)) return;

  Symbol *lastModule = nullptr;
  while (token != "}")
  {
    if (token == "{")
    {
      DecodaDefRecursive(symbols, lineNumber, lastModule, tokens, current_token);
    }
    else
    {
      lastModule = DecodaDefFunction(symbols, lineNumber, module, tokens, current_token, token);
      if (lastModule == nullptr)
        return;
    }

    if (!GetNextToken(tokens, token, lineNumber, current_token)) return;
  }
}

void DecodaPrefixRecursive(std::vector<Symbol*>& symbols, unsigned int &lineNumber, Symbol *module, std::vector<Token> const &tokens, unsigned int &current_token)
{
  wxString t1;
  if (!GetNextToken(tokens, t1, lineNumber, current_token)) return;

  wxString t2;
  if (!GetNextToken(tokens, t2, lineNumber, current_token)) return;

  while (t1 != "}" && t2 != "}")
  {
    Symbol *sym = new Symbol(module, t2, lineNumber, SymbolType::Prefix);
    sym->requiredModule = t1;
    symbols.push_back(sym);

    if (!GetNextToken(tokens, t1, lineNumber, current_token)) return;
    if (!GetNextToken(tokens, t2, lineNumber, current_token)) return;
  }
}

OutputWindow *outputWin = nullptr;

Symbol* GetStackTop(wxStack<Symbol*> symStack)
{
    Symbol* possible = symStack.top();
    while (possible != nullptr && possible->type == SymbolType::ScopeDummy)
        possible = possible->parent;
    return possible;
}

void SymbolParserThread::ParseFileSymbols(wxInputStream& input, std::vector<Symbol*>& symbols)
{

    if (!input.IsOk())
    {
        return;
    }

    wxString token;

    unsigned int lineNumber = 1;

    Symbol *return_symbol = nullptr;

    wxStack<Symbol *> symStack;
    symStack.push(nullptr);

    std::vector<Token> tokens;
    while (GetToken(input, token, lineNumber))
    {
      tokens.emplace_back(token, lineNumber);
    }

    for (unsigned current_token = 0; current_token < tokens.size(); ++current_token)
    {
      token = tokens[current_token].token;
      lineNumber = tokens[current_token].lineNumber;

      if (token == "function")
      {
        unsigned int defLineNumber = lineNumber;
        Symbol *function = nullptr;

        // Lua functions can have these forms:
        //    function (...)
        //    function Name (...)
        //    function Module.Function (...)
        //    function Class:Method (...)

        wxString t1;
        if (!GetNextToken(tokens, t1, lineNumber, current_token)) break;

        if (t1 == "(")
        {
          // The form function (...) which doesn't have a name. If we
          // were being really clever we could check to see what is being
          // done with this value, but we're not.
          //continue;
          
          // The form function Name (...).
          function = new Symbol(GetStackTop(symStack), "__unnamed__", defLineNumber, SymbolType::ScopeDummy);
          //symbols.push_back(function);
          symStack.push(function);
          continue;
        }

        wxString t2;

        if (!GetNextToken(tokens, t2, lineNumber, current_token)) break;

        if (t2 == "(")
        {
          function = new Symbol(GetStackTop(symStack), t1, defLineNumber);

          if (return_symbol)
          {
            function->typeSymbol = return_symbol;
            return_symbol = nullptr;
          }

          // The form function Name (...).
          symbols.push_back(function);
        }
        else
        {

          wxString t3;
          if (!GetNextToken(tokens, t3, lineNumber, current_token)) break;

          if (t2 == "." || t2 == ":")
          {
            Symbol *module = GetSymbol(t1, symbols);
            if (module == nullptr)
            {
              module = new Symbol(GetStackTop(symStack), t1, defLineNumber, SymbolType::Module);
              symbols.push_back(module);
            }

            function = new Symbol(module, t3, defLineNumber);
            if (return_symbol)
            {
              function->typeSymbol = return_symbol;
              return_symbol = nullptr;
            }

            symbols.push_back(function);
          }

        }


        if (function)
          symStack.push(function);

      }
      else if (token == "if")
      {
        unsigned int defLineNumber = lineNumber;
        Symbol *function = nullptr;
      
        // Lua ifs can have these forms:
        //    if ... then
        //    (TODO consider resetting scope if encountering an else or elseif, treating it like a combo end+if)
       
        function = new Symbol(GetStackTop(symStack), "__if__", defLineNumber, SymbolType::ScopeDummy);
        //symbols.push_back(function);
        symStack.push(function);
        continue;
      }
      else if (token == "while")
      {
        unsigned int defLineNumber = lineNumber;
        Symbol *function = nullptr;
      
        // Lua while loops can have these forms:
        //    while ... do
       
        function = new Symbol(GetStackTop(symStack), "__while__", defLineNumber, SymbolType::ScopeDummy);
        //symbols.push_back(function);
        symStack.push(function);
        continue;
      }
      else if (token == "for")
      {
        unsigned int defLineNumber = lineNumber;
        Symbol *function = nullptr;
      
        // Lua for loops can have these forms:
        //    for ... do
       
        function = new Symbol(GetStackTop(symStack), "__for__", defLineNumber, SymbolType::ScopeDummy);
        //symbols.push_back(function);
        symStack.push(function);
        continue;
      }
      else if (token == "decodadef")
      {
        //A decodadef will be in the form:
        //decodadef name { Definitions }
        //decodadef name ret

        unsigned int defLineNumber = lineNumber;

        wxString moduleName;
        if (!GetNextToken(tokens, moduleName, lineNumber, current_token)) break;

        wxString t1 = PeekNextToken(tokens, current_token, lineNumber);
        if (t1 == "{")
        {
          if (!GetNextToken(tokens, t1, lineNumber, current_token)) break;
          //outputWin->OutputMessage("Processing " + moduleName);

          Symbol *module = GetSymbol(moduleName, symbols);
          if (module == nullptr)
          {
            module = new Symbol(GetStackTop(symStack), moduleName, lineNumber, SymbolType::Type);
            symbols.push_back(module);
          }

          DecodaDefRecursive(symbols, lineNumber, module, tokens, current_token);
        }
        else
        {
          DecodaDefFunction(symbols, lineNumber, nullptr, tokens, current_token, moduleName);
        }
      }
      else if (token == "end")
      {
          if (symStack.size() > 1)
          {
              Symbol* pop = symStack.top();
              symStack.pop();
              if (pop->type == SymbolType::ScopeDummy)
                  delete pop;
          }
      }
      else if (token == "decodaprefix")
      {
        //A decodaprefix will be in the form:
        //decodaprefix Module name

        /*
        decodaprefix this __FILENAME__
        decodaprefix this { Weapon nil }

        */

        unsigned int defLineNumber = lineNumber;

        wxString moduleName;
        if (!GetNextToken(tokens, moduleName, lineNumber, current_token)) break;


        Symbol *module = GetSymbol(moduleName, symbols, SymbolType::Prefix);
        if (module == nullptr)
        {
          module = new Symbol(nullptr, moduleName, defLineNumber, SymbolType::Prefix);
          symbols.push_back(module);
        }

        wxString t1;
        if (!GetNextToken(tokens, t1, lineNumber, current_token)) break;

        //List of 
        if (t1 == "{")
        {
          DecodaPrefixRecursive(symbols, lineNumber, module, tokens, current_token);
        }
        else
        {
          Symbol *sym_prefix = new Symbol(module, t1, defLineNumber, SymbolType::Prefix);
          sym_prefix->requiredModule = moduleName;
          symbols.push_back(sym_prefix);
        }
      }
      else if (token == "decodareturn")
      {
        //A decodaprefix will be in the form:
        //decodareturn Module

        unsigned int defLineNumber = lineNumber;

        wxString moduleName;
        if (!GetNextToken(tokens, moduleName, lineNumber, current_token)) break;

        Symbol *module = GetSymbol(moduleName, symbols);
        if (module == nullptr)
        {
          module = new Symbol(GetStackTop(symStack), moduleName, lineNumber, SymbolType::Type);
          symbols.push_back(module);
        }

        return_symbol = module;
      }
      else if (token == "=")
      {
        unsigned int defLineNumber = lineNumber;

        //If we find an equal sign, we need to find the left and right hand side
        unsigned start = current_token;

        //First handle +=, -=, *=, /=
        wxString prev = PeekPrevToken(tokens, current_token, lineNumber);
        if (prev == "+" || prev == "-" || prev == "*" || prev == "/" || prev == "~" || prev == "=")
          GetPrevToken(tokens, prev, lineNumber, current_token);

        wxStack<wxString> lhs_stack;
        wxString lhs;
        if (!GetPrevToken(tokens, lhs, lineNumber, current_token)) break;
        
        lhs_stack.push(lhs);

        int currentLine = lineNumber;

        prev = PeekPrevToken(tokens, current_token, lineNumber);
        while ((prev == "." || prev == ":" || prev == ")" || prev == "]") && lineNumber == currentLine)
        {
          if (prev == "." || prev == ":")
          {
            GetPrevToken(tokens, prev, lineNumber, current_token);
            lhs_stack.push(prev);

            wxString part;
            if (!GetPrevToken(tokens, part, lineNumber, current_token)) return;
            if (part == ")" || part == "]")
            {
              current_token++;
              prev = part;
              continue;
            }

            lhs_stack.push(part);
          }
          else if (prev == ")" || prev == "]")
          {
            GetPrevToken(tokens, prev, lineNumber, current_token);
            lhs_stack.push(prev);

            wxString open;
            wxString close;

            if (prev == ")")
            {
              open = "(";
              close = ")";
            }
            else if (prev == "]")
            {
              open = "[";
              close = "]";
            }


            int parenStack = 0;
            wxString part;
            if (!GetPrevToken(tokens, part, lineNumber, current_token)) return;
            for (;;)
            {
              if (part == close)
                parenStack++;
              if (part == open)
              {
                if (parenStack == 0)
                  break;
                parenStack--;
              }

              lhs_stack.push(part);
              if (!GetPrevToken(tokens, part, lineNumber, current_token)) return;
            }
            lhs_stack.push(part);

            if (!GetPrevToken(tokens, part, lineNumber, current_token)) return;
            lhs_stack.push(part);
          }

          prev = PeekPrevToken(tokens, current_token, lineNumber);
        }

        //Parse rhs
        current_token = start;

        //First handle +=, -=, *=, /=
        wxString next = PeekNextToken(tokens, current_token, lineNumber);
        int next_size = next.size();
        bool valid = true;
        for (int i = 0; i < next_size; ++i)
        {
          if (IsSymbol(next[i]) || IsSpace(next[i]))
          {
            valid = false;
            break;
          }
        }

        wxString rhs;

        if (valid)
        {
          GetNextToken(tokens, next, lineNumber, current_token);
          rhs.Append(next);

          next = PeekNextToken(tokens, current_token, lineNumber);
          while ((next == "." || next == ":" || next == "(" || next == "[") && lineNumber == currentLine)
          {
            if (next == "." || next == ":")
            {
              GetNextToken(tokens, next, lineNumber, current_token);
              rhs.Append(next);

              wxString part;
              if (!GetNextToken(tokens, part, lineNumber, current_token)) return;
              rhs.Append(part);
            }
            else if (next == "(" || next == "[")
            {
              GetNextToken(tokens, next, lineNumber, current_token);
              rhs.Append(next);

              wxString open;
              wxString close;

              if (next == "(")
              {
                open = "(";
                close = ")";
              }
              else if (next == "[")
              {
                open = "[";
                close = "]";
              }


              int parenStack = 0;
              wxString part;
              if (!GetNextToken(tokens, part, lineNumber, current_token)) return;
              for (;;)
              {
                if (part == open)
                  parenStack++;
                if (part == close)
                {
                  if (parenStack == 0)
                    break;
                  parenStack--;
                }

                rhs.Append(part);
                if (!GetNextToken(tokens, part, lineNumber, current_token)) return;
              }
              rhs.Append(part);
            }

            next = PeekNextToken(tokens, current_token, lineNumber);
          }
        }

        //Build up the strings with the stacks
        if (lhs_stack.size() > 0 && rhs.size() > 0)
        {
          lhs.Empty();

          while (!lhs_stack.empty())
          {
            lhs.Append(lhs_stack.top());
            lhs_stack.pop();
          }

          Symbol *assignment = new Symbol(GetStackTop(symStack), lhs, defLineNumber, SymbolType::Assignment);
          assignment->rhs = rhs;
          symbols.push_back(assignment);
        }

        //Reset token
        current_token = start;
      }
    }
}