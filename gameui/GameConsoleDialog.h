//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef GAMECONSOLEDIALOG_H
#define GAMECONSOLEDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/consoledialog.h"
#include <Color.h>
#include "tier1/utlvector.h"
#include "EngineInterface.h"
#include "vgui_controls/Frame.h"


//-----------------------------------------------------------------------------
// Purpose: Game/dev console dialog
//-----------------------------------------------------------------------------
class CGameConsoleDialog : public vgui::CConsoleDialog
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CGameConsoleDialog, vgui::CConsoleDialog );

public:
	CGameConsoleDialog();

private:
	MESSAGE_FUNC( OnClosedByHittingTilde, "ClosedByHittingTilde" );
	MESSAGE_FUNC_CHARPTR_OVERRIDE( OnCommandSubmitted, "CommandSubmitted", command );

	void OnKeyCodeTyped( vgui::KeyCode code ) override;
	void OnCommand( const char *command ) override;
};


#endif // GAMECONSOLEDIALOG_H
