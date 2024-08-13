//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ATTRIBUTESTRINGCHOICEPANEL_h
#define ATTRIBUTESTRINGCHOICEPANEL_h

#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/BaseAttributeChoicePanel.h"
#include "movieobjects/dmeeditortypedictionary.h"
#include "vgui_controls/MessageMap.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct AttributeWidgetInfo_t;

namespace vgui
{
	class Panel;
	class ComboBox;
}


//-----------------------------------------------------------------------------
// Configuration for string choices
//-----------------------------------------------------------------------------
class CDmeEditorStringChoicesInfo : public CDmeEditorChoicesInfo
{
	DEFINE_ELEMENT( CDmeEditorStringChoicesInfo, CDmeEditorChoicesInfo );

public:
	// Add a choice
	CDmElement *AddChoice( const char *pValueString, const char *pChoiceString );

	// Gets the choices
	const char *GetChoiceValue( int nIndex ) const;
};


//-----------------------------------------------------------------------------
// CAttributeStringChoicePanel
//-----------------------------------------------------------------------------
class CAttributeStringChoicePanel : public CBaseAttributeChoicePanel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CAttributeStringChoicePanel, CBaseAttributeChoicePanel );

public:
	CAttributeStringChoicePanel( vgui::Panel *parent,	const AttributeWidgetInfo_t &info );

private:
	// Derived classes can re-implement this to fill the combo box however they like
	void PopulateComboBox( vgui::ComboBox *pComboBox ) override;
	void SetAttributeFromComboBox( vgui::ComboBox *pComboBox, KeyValues *pKeyValues ) override;
	void SetComboBoxFromAttribute( vgui::ComboBox *pComboBox ) override;
};


// ----------------------------------------------------------------------------
#endif // ATTRIBUTESTRINGCHOICEPANEL_h
