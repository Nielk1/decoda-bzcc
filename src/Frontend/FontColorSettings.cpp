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

#include "FontColorSettings.h"
#include "XmlUtility.h"

#include <wx/xml/xml.h>

const char* FontColorSettings::s_displayItemName[] = { DisplayColorsDef(DisplayColorString) };

FontColorSettings::FontColorSettings()
{
    m_font = wxFont(8, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

    DisplayColorsDef(DisplayColorInit);

    int a = 1;
}

unsigned int FontColorSettings::GetNumDisplayItems() const
{
    return DisplayItem_Last;
}

const char* FontColorSettings::GetDisplayItemName(unsigned int i) const
{
    return s_displayItemName[i];
}

void FontColorSettings::SetFont(const wxFont& font)
{
    m_font = font;
}

const wxFont& FontColorSettings::GetFont() const
{
    return m_font;
}

wxFont FontColorSettings::GetFont(DisplayItem displayItem) const
{

    wxFont font = GetFont();

    if (m_colors[displayItem].bold)
    {
        font.SetWeight(wxFONTWEIGHT_BOLD);
    }

    if (m_colors[displayItem].italic)
    {
        font.SetStyle(wxFONTSTYLE_ITALIC);
    }

    return font;

}

const FontColorSettings::Colors& FontColorSettings::GetColors(DisplayItem displayItem) const
{
    return m_colors[displayItem];
}

void FontColorSettings::SetColors(DisplayItem displayItem, const Colors& colors)
{
    m_colors[displayItem] = colors;
}

wxXmlNode* FontColorSettings::Save(const wxString& tag) const
{

    wxXmlNode* root = new wxXmlNode(wxXML_ELEMENT_NODE, tag);    

    wxString fontDesc = m_font.GetNativeFontInfoDesc();
    root->AddChild( WriteXmlNode("font", fontDesc) );

    for (unsigned int i = 0; i < GetNumDisplayItems(); ++i)
    {
        wxXmlNode* node = new wxXmlNode(wxXML_ELEMENT_NODE, "displayitem");    
        node->AddChild( WriteXmlNode("id", i) );
        node->AddChild( WriteXmlNode("foreColor", m_colors[i].foreColor) );
        node->AddChild( WriteXmlNode("backColor", m_colors[i].backColor) );
        node->AddChild( WriteXmlNodeBool("bold", m_colors[i].bold) );
        node->AddChild( WriteXmlNodeBool("italic", m_colors[i].italic) );
        root->AddChild(node);
    }

    return root;

}

void FontColorSettings::Load(wxXmlNode* root)
{

    wxXmlNode* node = root->GetChildren();
    while (node != NULL)
    {

        wxString data;

        if (ReadXmlNode(node, "font", data))
        {
            m_font.SetNativeFontInfo(data);
        }
        else if (node->GetName() == "displayitem")
        {

            Colors colors;
            unsigned int id = -1;

            wxXmlNode* child = node->GetChildren();    
            while (child != NULL)
            {

                ReadXmlNode(child, "id", id) ||
                ReadXmlNode(child, "foreColor", colors.foreColor) ||
                ReadXmlNode(child, "backColor", colors.backColor) ||
                ReadXmlNode(child, "bold",      colors.bold)      ||
                ReadXmlNode(child, "italic",    colors.italic);

                child = child->GetNext();

            }

            if (id != -1 && id < GetNumDisplayItems())
            {
                m_colors[id] = colors;
            }
        
        }

        node = node->GetNext();

    }

}