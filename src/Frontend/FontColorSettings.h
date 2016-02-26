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

#ifndef FONT_COLOR_SETTINGS_H
#define FONT_COLOR_SETTINGS_H

#include <wx/wx.h>
#include <vector>

//
// Forward declarations.
//

class wxXmlNode;

/**
 * This class stores font and color settings for code display and syntax
 * highlighting.
 */

#define DisplayColorsDef(_) \
  _(Default,        wxColor(0x00, 0x00, 0x00), wxColor(0xFF, 0xFF, 0xFF), false, false) \
  _(Comment,        wxColor(0x00, 0x80, 0x00), wxColor(0xFF, 0xFF, 0xFF), false, false) \
  _(Keyword,        wxColor(0x00, 0x00, 0xFF), wxColor(0xFF, 0xFF, 0xFF), false, false) \
  _(Operator,       wxColor(0x00, 0x00, 0x00), wxColor(0xFF, 0xFF, 0xFF), false, false) \
  _(String,         wxColor(0xFF, 0x00, 0x00), wxColor(0xFF, 0xFF, 0xFF), false, false) \
  _(Number,         wxColor(0xFF, 0x00, 0x00), wxColor(0xFF, 0xFF, 0xFF), false, false) \
  _(Error,          wxColor(0xFF, 0xFF, 0xFF), wxColor(0xFF, 0x00, 0x00), false, false) \
  _(Warning,        wxColor(0x00, 0x00, 0x00), wxColor(0xFF, 0xFF, 0x00), false, false) \
  _(Selection,      wxColor(0x00, 0x00, 0x00), wxColor(0x9D, 0xA7, 0xC3), false, false) \
  _(Window,         wxColor(0x00, 0x00, 0x00), wxColor(0xE0, 0xE0, 0xE0), false, false) \
  _(WindowMargin,   wxColor(0x00, 0x00, 0xC0), wxColor(0xE0, 0xE0, 0xE0), false, false) \
  _(CaretLine,      wxColor(0x00, 0x00, 0x00), wxColor(0xE0, 0xE0, 0xE0), false, false) \

#define DisplayColorEnum(name, fore, back, bold, ital) DisplayItem_ ## name,
#define DisplayColorString(name, fore, back, bold, ital) #name,
#define DisplayColorInit(name, fore, back, bold_, ital_) \
  m_colors[DisplayItem_ ## name ## ].foreColor = fore; \
  m_colors[DisplayItem_ ## name ## ].backColor = back; \
  m_colors[DisplayItem_ ## name ## ].bold = bold_;      \
  m_colors[DisplayItem_ ## name ## ].italic = ital_;    \

class FontColorSettings
{

public:

    enum DisplayItem
    {
        DisplayColorsDef(DisplayColorEnum)
        DisplayItem_Last,
    };

    struct Colors
    {
        wxColour    foreColor;
        wxColour    backColor;
        bool        bold;
        bool        italic;
    };

    /**
     * Constructor. Sets the font and colors to the defaults.
     */
    FontColorSettings();

    /**
     * Returns the number of display items the settings store information for.
     */
    unsigned int GetNumDisplayItems() const;

    /**
     * Returns the human-readable name for the ith display item.
     */
    const char* GetDisplayItemName(unsigned int i) const;

    /**
     * Sets the font used to display all of the styles.
     */
    void SetFont(const wxFont& font);

    /**
     * Gets the font used to display all of the styles.
     */
    const wxFont& GetFont() const;

    /**
     * Gets the font used to display a specific item. This includes the bold
     * and italic modifiers.
     */
    wxFont GetFont(DisplayItem displayItem) const;

    /**
     * Returns the colors for the specified display item.
     */
    const Colors& GetColors(DisplayItem displayItem) const;

    /**
     * Sets the colors for the specified display item.
     */
    void SetColors(DisplayItem displayItem, const Colors& colors);

    /**
     * Saves the font and color settings in XML format. The tag is the name that is given
     * to the root node.
     */
    wxXmlNode* Save(const wxString& tag) const;

    /**
     * Loads the font and color settings from XML format.
     */
    void Load(wxXmlNode* root);

private:

    static const char*          s_displayItemName[DisplayItem_Last];

    wxFont                      m_font;
    Colors                      m_colors[DisplayItem_Last];

};

#endif