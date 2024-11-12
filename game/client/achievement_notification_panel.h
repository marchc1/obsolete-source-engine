//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ACHIEVEMENT_NOTIFICATION_PANEL_H
#define ACHIEVEMENT_NOTIFICATION_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/EditablePanel.h>
#include "hudelement.h"

using namespace vgui;

class CAchievementNotificationPanel : public CHudElement, public EditablePanel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CAchievementNotificationPanel, EditablePanel );

public:
	CAchievementNotificationPanel( const char *pElementName );

	void	Init() override;
	void	ApplySchemeSettings( IScheme *scheme ) override;
	bool	ShouldDraw( void ) override;
	void	PerformLayout( void ) override;
	void	LevelInit( void ) override { m_flHideTime = 0; }
	void	FireGameEvent( IGameEvent * event ) override;
	void	OnTick( void ) override;

	void AddNotification( const char *szIconBaseName, const wchar_t *pHeading, const wchar_t *pTitle );

private:
	void ShowNextNotification();
	void SetXAndWide( Panel *pPanel, int x, int wide );

	double m_flHideTime;

	Label *m_pLabelHeading;
	Label *m_pLabelTitle;
	EditablePanel *m_pPanelBackground;
	ImagePanel *m_pIcon;

	struct Notification_t
	{
		char szIconBaseName[255];
		wchar_t szHeading[255];
		wchar_t szTitle[255];
	};

	CUtlLinkedList<Notification_t> m_queueNotification;
};

#endif	// ACHIEVEMENT_NOTIFICATION_PANEL_H