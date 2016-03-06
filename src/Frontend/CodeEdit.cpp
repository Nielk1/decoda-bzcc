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

#include "CodeEdit.h"
#include "FontColorSettings.h"
#include "EditorSettings.h"
#include "ToolTipWindow.h"
#include "AutoCompleteManager.h"
#include "Tokenizer.h"
#include "Bitmaps.h"

#include "res/functionicon.xpm"
#include "res/classicon.xpm"

#include <algorithm>

BEGIN_EVENT_TABLE( CodeEdit, wxStyledTextCtrl )

    EVT_LEAVE_WINDOW(                    CodeEdit::OnMouseLeave)
    EVT_KILL_FOCUS(                      CodeEdit::OnKillFocus)
    EVT_STC_CHARADDED(         wxID_ANY, CodeEdit::OnCharAdded)
    EVT_STC_CHANGE(            wxID_ANY, CodeEdit::OnChange)
    EVT_STC_MODIFIED(          wxID_ANY, CodeEdit::OnModified)
    EVT_STC_AUTOCOMP_DWELLSTART(wxID_ANY, CodeEdit::OnAutocompletionDwellStart)
    EVT_STC_AUTOCOMP_DWELLEND(wxID_ANY, CodeEdit::OnAutocompletionDwellEnd)

END_EVENT_TABLE()

CodeEdit::CodeEdit()
{

    // The minimum number of characters that must be typed before autocomplete
    // is displayed for global symbols. We impose a minimum so that autocomplete
    // doesn't popup too much.
    m_minAutoCompleteLength = 3;
    m_autoCompleteManager   = NULL;

    m_tipWindow             = NULL;
    m_autoIndented          = false;

    m_enableAutoComplete    = true;
    m_lineMappingDirty      = true;

}

void CodeEdit::SetFontColorSettings(const FontColorSettings& settings)
{
    // For some reason StyleSetFont takes a (non-const) reference, so we need to make
    // a copy before passing it in.
    wxFont font = settings.GetFont();

    SetSelForeground(true, settings.GetColors(FontColorSettings::DisplayItem_Selection).foreColor);
    SetSelBackground(true, settings.GetColors(FontColorSettings::DisplayItem_Selection).backColor);

    StyleSetFont(wxSTC_STYLE_DEFAULT, font);
    StyleClearAll();

    font = settings.GetFont(FontColorSettings::DisplayItem_Default);
    StyleSetFont(wxSTC_LUA_DEFAULT,                 font);
    StyleSetFont(wxSTC_LUA_IDENTIFIER,              font);
    StyleSetForeground(wxSTC_LUA_DEFAULT,           settings.GetColors(FontColorSettings::DisplayItem_Default).foreColor);
    StyleSetBackground(wxSTC_LUA_DEFAULT,           settings.GetColors(FontColorSettings::DisplayItem_Default).backColor);
    StyleSetForeground(wxSTC_STYLE_DEFAULT,         settings.GetColors(FontColorSettings::DisplayItem_Default).foreColor);
    StyleSetBackground(wxSTC_STYLE_DEFAULT,         settings.GetColors(FontColorSettings::DisplayItem_Default).backColor);
    StyleSetForeground(wxSTC_LUA_IDENTIFIER,        settings.GetColors(FontColorSettings::DisplayItem_Default).foreColor);
    StyleSetBackground(wxSTC_LUA_IDENTIFIER,        settings.GetColors(FontColorSettings::DisplayItem_Default).backColor);

    font = settings.GetFont(FontColorSettings::DisplayItem_Comment);
    StyleSetFont(wxSTC_LUA_COMMENT,                 font);
    StyleSetFont(wxSTC_LUA_COMMENTLINE,             font);
    StyleSetFont(wxSTC_LUA_COMMENTDOC,              font);
    StyleSetForeground(wxSTC_LUA_COMMENT,           settings.GetColors(FontColorSettings::DisplayItem_Comment).foreColor);
    StyleSetBackground(wxSTC_LUA_COMMENT,           settings.GetColors(FontColorSettings::DisplayItem_Comment).backColor);
    StyleSetForeground(wxSTC_LUA_COMMENTLINE,       settings.GetColors(FontColorSettings::DisplayItem_Comment).foreColor);
    StyleSetBackground(wxSTC_LUA_COMMENTLINE,       settings.GetColors(FontColorSettings::DisplayItem_Comment).backColor);
    StyleSetForeground(wxSTC_LUA_COMMENTDOC,        settings.GetColors(FontColorSettings::DisplayItem_Comment).foreColor);
    StyleSetBackground(wxSTC_LUA_COMMENTDOC,        settings.GetColors(FontColorSettings::DisplayItem_Comment).backColor);

    font = settings.GetFont(FontColorSettings::DisplayItem_Keyword);
    StyleSetFont(wxSTC_LUA_WORD2,                   font);
    StyleSetForeground(wxSTC_LUA_WORD2,             settings.GetColors(FontColorSettings::DisplayItem_Keyword).foreColor);
    StyleSetBackground(wxSTC_LUA_WORD2,             settings.GetColors(FontColorSettings::DisplayItem_Keyword).backColor);

    font = settings.GetFont(FontColorSettings::DisplayItem_Operator);
    StyleSetFont(wxSTC_LUA_OPERATOR,                font);
    StyleSetForeground(wxSTC_LUA_OPERATOR,          settings.GetColors(FontColorSettings::DisplayItem_Operator).foreColor);
    StyleSetBackground(wxSTC_LUA_OPERATOR,          settings.GetColors(FontColorSettings::DisplayItem_Operator).backColor);

    font = settings.GetFont(FontColorSettings::DisplayItem_String);
    StyleSetFont(wxSTC_LUA_STRING,                  font);
    StyleSetForeground(wxSTC_LUA_STRING,            settings.GetColors(FontColorSettings::DisplayItem_String).foreColor);
    StyleSetBackground(wxSTC_LUA_STRING,            settings.GetColors(FontColorSettings::DisplayItem_String).backColor);
    StyleSetFont(wxSTC_LUA_STRINGEOL,               font);
    StyleSetForeground(wxSTC_LUA_STRINGEOL,         settings.GetColors(FontColorSettings::DisplayItem_String).foreColor);
    StyleSetBackground(wxSTC_LUA_STRINGEOL,         settings.GetColors(FontColorSettings::DisplayItem_String).backColor);
    StyleSetFont(wxSTC_LUA_LITERALSTRING,           font);
    StyleSetForeground(wxSTC_LUA_LITERALSTRING,     settings.GetColors(FontColorSettings::DisplayItem_String).foreColor);
    StyleSetBackground(wxSTC_LUA_LITERALSTRING,     settings.GetColors(FontColorSettings::DisplayItem_String).backColor);
    StyleSetFont(wxSTC_LUA_CHARACTER,               font);
    StyleSetForeground(wxSTC_LUA_CHARACTER,         settings.GetColors(FontColorSettings::DisplayItem_String).foreColor);
    StyleSetBackground(wxSTC_LUA_CHARACTER,         settings.GetColors(FontColorSettings::DisplayItem_String).backColor);

    font = settings.GetFont(FontColorSettings::DisplayItem_Number);
    StyleSetFont(wxSTC_LUA_NUMBER,                  font);
    StyleSetForeground(wxSTC_LUA_NUMBER,            settings.GetColors(FontColorSettings::DisplayItem_Number).foreColor);
    StyleSetBackground(wxSTC_LUA_NUMBER,            settings.GetColors(FontColorSettings::DisplayItem_Number).backColor);

    StyleSetSize(wxSTC_STYLE_LINENUMBER, font.GetPointSize());

    StyleSetBackground(wxSTC_STYLE_LINENUMBER, settings.GetColors(FontColorSettings::DisplayItem_WindowMargin).backColor);
    StyleSetForeground(wxSTC_STYLE_LINENUMBER, settings.GetColors(FontColorSettings::DisplayItem_WindowMargin).foreColor);

    SetCaretForeground(settings.GetColors(FontColorSettings::DisplayItem_CaretLine).foreColor);
    SetCaretLineBackground(settings.GetColors(FontColorSettings::DisplayItem_CaretLine).backColor);
    SetCaretLineVisible(true);
}

bool CodeEdit::LoadFile(const wxString& filename)
{
    return wxStyledTextCtrl::LoadFile(filename);
}

bool CodeEdit::SaveFile(const wxString& filename)
{
    unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
    wxFile f;
    if (!f.Open(filename, wxFile::write ))
        return false;
    f.Write(bom, 3);
    wxString text(GetText());
    wxScopedCharBuffer buffer(text.ToUTF8());
    f.Write(buffer.data(), buffer.length());
    f.Close();
    return true;
}

void CodeEdit::SetText(const wxString& text)
{
    wxStyledTextCtrl::SetText(wxString::FromUTF8(text));
}

void CodeEdit::SetAutoCompleteManager(const AutoCompleteManager* autoCompleteManager)
{
    m_autoCompleteManager = autoCompleteManager;
}

void CodeEdit::SetEditorSettings(const EditorSettings& settings)
{
    m_indentationSize = settings.GetIndentSize();

    SetIndent(m_indentationSize);
    SetTabWidth(m_indentationSize);
    
    bool useTabs = settings.GetUseTabs();
    bool showWhiteSpace = settings.GetShowWhiteSpace();

    SetCodePage(wxSTC_CP_UTF8);
    SetUseTabs(useTabs);
    SetTabIndents(useTabs);
    SetBackSpaceUnIndents(useTabs);
    SetViewWhiteSpace(showWhiteSpace);
    SetMultipleSelection(true);
    SetAdditionalSelectionTyping(true);
    SetMultiPaste(1);
    SetVirtualSpaceOptions(1);

    if (settings.GetShowLineNumbers())
    {

        // Figure out how wide the margin needs to be do display
        // the most number of linqes we'd reasonbly have.
        int marginSize = TextWidth(wxSTC_STYLE_LINENUMBER, "_99999");
        SetMarginWidth(0, marginSize);

        SetMarginType(0,wxSTC_MARGIN_NUMBER);

    }
    else
    {
        SetMarginWidth(0, 0);
    }    

    m_enableAutoComplete = settings.GetEnableAutoComplete();

}    

void CodeEdit::SetDefaultLexer()
{
    SetLexer(wxSTC_LEX_NULL);

    SetKeyWords(1, "");

    // Set the caret width to match MSVC.
    SetCaretWidth(2);

    // Set the marker bitmaps.
    MarkerDefineBitmap(Marker_Breakpoint,  wxMEMORY_BITMAP(Breakpoint_png) );
    MarkerDefineBitmap(Marker_CurrentLine, wxMEMORY_BITMAP(Currentline_png) );
    MarkerDefineBitmap(Marker_BreakLine,   wxMEMORY_BITMAP(Breakline_png) );

    // Setup the dwell time before a tooltip is displayed.
    SetMouseDwellTime(300);

    SetMarginSensitive(1, true);
    SetMarginType(1, wxSTC_MARGIN_SYMBOL);

    // Set the autocomplete icons.

    wxColour maskColor(0xFF, 0x9B, 0x77);

    wxBitmap functionIcon(functionicon, wxBITMAP_TYPE_XPM);
    functionIcon.SetMask(new wxMask(functionicon, maskColor));
    
    RegisterImage(AutoCompleteManager::Type_Function, functionIcon);

    wxBitmap classIcon(classicon, wxBITMAP_TYPE_XPM);
    classIcon.SetMask(new wxMask(classicon, maskColor));
    
    RegisterImage(AutoCompleteManager::Type_Class, classIcon);

}

void CodeEdit::SetLuaLexer()
{
    SetDefaultLexer();

    SetLexer(wxSTC_LEX_LUA);

    const char* keywords =
        "and       break     do        else      elseif "
        "end       false     for       function  if "
        "in        local     nil       not       or "
        "repeat    return    then      true      until     while ";

    SetKeyWords(1, keywords);

}

bool CodeEdit::Untabify()
{

    wxString text = GetText();

    if (Untabify(text))
    {
        SetText(text);
        return true;
    }

    return false;

}

bool CodeEdit::UntabifySelection()
{

    wxString text = GetSelectedText();

    if (Untabify(text))
    {
        ReplaceSelection(text);
        return true;
    }

    return false;

}

bool CodeEdit::Untabify(wxString& text) const
{

    // wxString::Replace is very slow with a big string because the operation
    // is performed in place (which requires a lot of copying). Instead we use
    // a different method with a second string.

    assert(m_indentationSize < 32);

    char indentation[32];
    memset(indentation, ' ', 32);
    indentation[m_indentationSize] = 0;

    wxString result;
    result.reserve(text.Length());

    unsigned int numTabs = 0;

    for (unsigned int i = 0; i < text.Length(); ++i)
    {

        if (text[i] == '\t')
        {
            result += indentation;
            ++numTabs;
        }
        else
        {
            result += text[i];
        }

    }

    if (numTabs > 0)
    {
        text = result;
        return true;
    }
    return false;

}

void CodeEdit::CommentSelection()
{

    if (GetLexer() == wxSTC_LEX_LUA)
    {
        CommentSelection("--");
    }

}

void CodeEdit::CommentSelection(const wxString& comment)
{


    if (GetSelectionStart() < GetSelectionEnd())
    {
  
        int startLine = LineFromPosition(GetSelectionStart());
        int endLine   = LineFromPosition(GetSelectionEnd());

        // Find the minimum indentation level for all of the selected lines.

        int minIndentation = INT_MAX;

        for (int i = startLine; i <= endLine; ++i)
        {
            
            wxString lineText = GetLine(i);
            int firstNonWhitespace = GetFirstNonWhitespace(lineText);

            if (firstNonWhitespace != -1)
            {
                minIndentation = wxMin(firstNonWhitespace, minIndentation);
            }

        }

        // Insert the comment string on each non-blank line.

        wxString result;

        for (int i = startLine; i <= endLine; ++i)
        {

            wxString lineText = GetLine(i);

            if (GetFirstNonWhitespace(lineText) != -1)
            {
                lineText.insert(minIndentation, comment);
            }

            result += lineText;

        }

        SetTargetStart(PositionFromLine(startLine));
        SetTargetEnd(PositionFromLine(endLine + 1));

        ReplaceTarget(result);

        // Select the replaced text.
        SetSelectionStart(GetTargetStart());
        SetSelectionEnd(GetTargetEnd() - 1);
    
    }

}

void CodeEdit::UncommentSelection()
{

    if (GetLexer() == wxSTC_LEX_LUA)
    {
        UncommentSelection("--");
    }

}

void CodeEdit::UncommentSelection(const wxString& commentString)
{

    if (GetSelectionStart() < GetSelectionEnd())
    {

        int startLine = LineFromPosition(GetSelectionStart());
        int endLine   = LineFromPosition(GetSelectionEnd());

        wxString result;
        
        for (int i = startLine; i <= endLine; ++i)
        {

            wxString lineText = GetLine(i);

            unsigned int c = GetFirstNonWhitespace(lineText);

            if (c != -1 && lineText.compare(c, commentString.Length(), commentString) == 0)
            {
                lineText.erase(c, commentString.Length());
            }

            result += lineText;

        }

        SetTargetStart(PositionFromLine(startLine));
        SetTargetEnd(PositionFromLine(endLine + 1));

        ReplaceTarget(result);

        // Select the replaced text.
        SetSelectionStart(GetTargetStart());
        SetSelectionEnd(GetTargetEnd() - 1);

    }

}

unsigned int CodeEdit::GetFirstNonWhitespace(const wxString& text) const
{

    for (unsigned int c = 0; c < text.Length(); ++c)
    {
        if (!IsSpace(text[c]))
        {
            return c;
        }
    }

    return -1;

}

void CodeEdit::Recolor()
{
    ClearDocumentStyle();
    int length = GetLength();
    Colourise(0, length);
}

bool CodeEdit::GetHoverText(int position, wxString& result)
{

    int selectionStart = GetSelectionStart();
    int selectionEnd   = GetSelectionEnd();

    if (position >= selectionStart && position < selectionEnd)
    {
        // We're mousing over the selected text.
        result = GetSelectedText();
        return true;
    }

    // We don't use the : character as a joiner since we don't
    // want to evaulate with that.
    return GetTokenFromPosition(position, ".", result);
    
}

bool CodeEdit::GetIsIdentifierChar(char c) const
{
    return isalnum(c) || c == '_';
}

void CodeEdit::ShowToolTip(int position, const wxString& text)
{

    bool showToolTip = true;

    // There doesn't seem to be a way to determine the window that the mouse is inside
    // in wxWidgets. If we port to another platform, we'll need to handle this.

#ifdef __WXMSW__

    POINT cursorPos;
    GetCursorPos(&cursorPos);

    HWND hActiveWindow = WindowFromPoint(cursorPos);
    showToolTip = hActiveWindow == GetHandle();

#else
    
    wxCOMPILE_TIME_ASSERT(0, "Unportable code");

#endif
    
    HideToolTip();

    if (showToolTip)
    {
        m_tipWindow = new ToolTipWindow(this, text);
    }

}

void CodeEdit::HideToolTip()
{
    if (m_tipWindow != NULL)
    {
        m_tipWindow->Destroy();
        m_tipWindow = NULL;
    }

    if (m_acTipWindow)
    {
      m_acTipWindow->Destroy();
      m_acTipWindow = nullptr;
    }
}

void CodeEdit::OnMouseLeave(wxMouseEvent& event)
{
    HideToolTip();
    event.Skip();
}

void CodeEdit::OnKillFocus(wxFocusEvent& event)
{
    AutoCompCancel();
    HideToolTip();
    event.Skip();
}

void CodeEdit::OnCharAdded(wxStyledTextEvent& event)
{

    // Indent the line to the same indentation as the previous line.
    // Adapted from http://STCntilla.sourceforge.net/STCntillaUsage.html

    char ch = event.GetKey();

    if  (ch == '\r' || ch == '\n')
    {

        int line        = GetCurrentLine();
        int lineLength  = LineLength(line);

        if (line > 0 && lineLength <= 2)
        {

            wxString buffer = GetLine(line - 1);
                
            for (unsigned int i =  0; i < buffer.Length();  ++i)
            {
                if (buffer[i] != ' ' && buffer[i] != '\t')
                {
                    buffer.Truncate(i);
                    break;
                }
            }

            ReplaceSelection(buffer);
            
            // Remember that we just auto-indented so that the backspace
            // key will un-autoindent us.
            m_autoIndented = true;
            
        }
        
    }
    else if (m_enableAutoComplete && m_autoCompleteManager != NULL)
    {

        // Handle auto completion.
      int style = GetStyleAt(GetCurrentPos());
      if (style != wxSTC_LUA_COMMENT && style != wxSTC_LUA_STRING && style != wxSTC_LUA_COMMENTLINE && style != wxSTC_LUA_COMMENTDOC)
      {
        wxString token;

        if (GetTokenFromPosition(GetCurrentPos() - 1, ".:", token))
        {
          StartAutoCompletion(token);
        }
      }
    }

    event.Skip();

}

void CodeEdit::OnChange(wxStyledTextEvent& event)
{
    m_lineMappingDirty = true;
    event.Skip();
}

void CodeEdit::OnModified(wxStyledTextEvent& event)
{
    
    event.Skip();    

    int linesAdded = event.GetLinesAdded();
        
    // If we're inserting new lines before a line, so we need to move the
    // markers down. STCntilla doesn't do this automatically for the current line.

    if (linesAdded > 0)
    {
    
        unsigned int position = event.GetPosition();
    
        unsigned int line = LineFromPosition(position);
        unsigned int lineStartPosition = PositionFromLine(line);

        if (position == lineStartPosition)
        {
            
            int markers = MarkerGet(line);

            // Delete all of the markers from the line.
            for (int i = 0; i < 32; ++i)
            {
                MarkerDelete(line, i);
            }

            // Add the markers back on the new line.
            MarkerAddSet(line + linesAdded, markers);

        }

    }    

}

void CodeEdit::CreateContextMenu(wxStyledTextEvent& event)
{
  AddToPopUp("Go To Definition", idcmdGotoDefinition);
}

bool CodeEdit::GetTokenFromPosition(int position, const wxString& joiners, wxString& token)
{

    if (position != -1)
    {
       
        // Search the text.

        int line = LineFromPosition(position);
        int seek = position - PositionFromLine(line);

        wxString text = GetLine(LineFromPosition(position));

        if (!isalnum(text[seek]) && joiners.Find(text[seek]) == wxNOT_FOUND)
        {
            return false;
        }
        
        // Search from the seek point to the left until we hit a non-alphanumeric which isn't a "."
        // "." must be handled specially so that expressions like player.health are easy to evaluate. 

        int start = seek;

        while (start > 0 && (GetIsIdentifierChar(text[start - 1]) || joiners.Find(text[start - 1]) != wxNOT_FOUND || text[start - 1] == ')' || text[start - 1] == ']'))
        {
          if (text[start - 1] == ')' || text[start - 1] == ']')
          {
            char open = ')';
            if (text[start - 1] == ']')
              open = ']';

            char close = '(';
            if (text[start - 1] == ']')
              close = '[';

            start--;
            int paren_stack = 0;
            while (start > 0)
            {
              if (text[start - 1] == close)
              {
                if (paren_stack == 0)
                  break;
                else
                  paren_stack--;
              }

              if (text[start - 1] == open)
                paren_stack++;

              start--;
            }

            //Eat the last character
            start--;
          }
          else
            --start;
        }

        // Search from the seek point to the right until we hit a non-alphanumeric

        unsigned int end = seek;

        while (end + 1 < text.Length() && (GetIsIdentifierChar(text[end + 1]) || text[end + 1] == '(' || text[end + 1] == '['))
        {
          if (text[end + 1] == '(' || text[end + 1] == '[')
          {
            char open = '(';
            if (text[end + 1] == '[')
              open = '[';

            char close = ')';
            if (text[end + 1] == '[')
              close = ']';

            ++end;
            int paren_stack = 0;
            while (end + 1 < text.Length())
            {
              if (text[end + 1] == close)
              {
                if (paren_stack == 0)
                  break;
                else
                  paren_stack--;
              }

              if (text[end + 1] == open)
                paren_stack++;

              ++end;
            }

            //Eat the last character
            ++end;
          }
          else
            ++end;
        }

        token = text.SubString(start, end);
        return true;

    }

    return false;

}

void CodeEdit::StartAutoCompletion(const wxString& token)
{
    wxASSERT(m_autoCompleteManager != NULL);

    wxString items;

    // Get the actual prefix of the thing we're trying to match for autocompletion.
    // If the token refers to a member, the prefix is the member name.

    wxString prefix;
    wxString newToken;
    bool member = false;
    bool function = false;

    if (GetLexer() == wxSTC_LEX_LUA)
    {

        int end1 = token.Find('.', true);

        if (end1 == wxNOT_FOUND)
        {
            end1 = 0;
        }
        else
        {
            // Skip the '.' character.
            ++end1;
            member = true;
        }

        int end2 = token.Find(':', true);

        if (end2 == wxNOT_FOUND)
        {
            end2 = 0;
        }
        else
        {
            // Skip the ':' character.
            ++end2;
            member = true;
        }

        int end = std::max(end1, end2);
        newToken = token.Right(token.Length() - end);
        prefix = token.Left(end - 1);

        if (end != 0 && token[end - 1] == ':')
          function = true;
    }
    else
    {
        // No autocompletion when using the default lexer.
        return;
    }

    if (!member && newToken.Length() < m_minAutoCompleteLength)
    {
        // Don't pop up the auto completion if the user hasn't typed in very
        // much yet.
        return;
    }

    wxVector<wxString> prefixes;
    m_acTooltips.clear();

    m_autoCompleteManager->ParsePrefix(prefix, file, GetCurrentLine(), prefixes);
    m_autoCompleteManager->GetMatchingItems(newToken, prefixes, member, function, items, token, m_acTooltips);

    if (!AutoCompActive() || m_autoCompleteItems != items)
    {

        // Remember the items in the list so that we don't redisplay the list
        // with the same set of items (reduces flickering).
        m_autoCompleteItems = items;

        if (!items.IsEmpty())
        {
            // Show the autocomplete selection list.
          AutoCompShow(newToken.Length(), items);
        }
        else
        {
            // We have no matching items, so hide the autocompletion selection.
            AutoCompCancel();
        }

    }

}

void CodeEdit::OnAutocompletionDwellStart(wxStyledTextEvent& event)
{
  wxPoint p(event.GetX(), event.GetY());

  if (p.x != 0 && p.y != 0)
    m_acTipWindow = new ToolTipWindow((wxWindow *)event.GetEventObject(), m_acTooltips[event.GetInt()], p);
}

void CodeEdit::OnAutocompletionDwellEnd(wxStyledTextEvent& event)
{
  if (m_acTipWindow)
    m_acTipWindow->Destroy();

  m_acTipWindow = nullptr;
}

bool CodeEdit::GetIsLineMappingDirty() const
{
    return m_lineMappingDirty;
}

void CodeEdit::SetIsLineMappingDirty(bool lineMappingDirty)
{
    m_lineMappingDirty = lineMappingDirty;
}

wxColor CodeEdit::GetInverse(const wxColor& color)
{

    unsigned char r = color.Red();
    unsigned char g = color.Green();
    unsigned char b = color.Blue();

    return wxColor( r ^ 0xFF, g ^ 0xFF, b ^ 0xFF );

}