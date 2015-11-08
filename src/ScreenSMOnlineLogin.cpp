#include "global.h"
#if !defined(WITHOUT_NETWORKING)
#include "ScreenSMOnlineLogin.h"
#include "RageLog.h"
#include "ProfileManager.h"
#include "GameSoundManager.h"
#include "ThemeManager.h"
#include "PrefsManager.h"
#include "ScreenManager.h"
#include "ScreenTextEntry.h"
#include "ScreenPrompt.h"
#include "GameState.h"
#include "NetworkSyncManager.h"
#include "Profile.h"
#include "LocalizedString.h"
#include "OptionRowHandler.h"

REGISTER_SCREEN_CLASS(ScreenSMOnlineLogin);

AutoScreenMessage( SM_SMOnlinePack );
AutoScreenMessage( SM_PasswordDone );
AutoScreenMessage( SM_NoProfilesDefined );

static LocalizedString DEFINE_A_PROFILE( "ScreenSMOnlineLogin", "You must define a Profile." );
void ScreenSMOnlineLogin::Init()
{
	ScreenOptions::Init();
	m_iPlayer = 0;

	vector<OptionRowHandler*> vHands;

	OptionRowHandler *pHand = OptionRowHandlerUtil::MakeNull();

	pHand->m_Def.m_sName = "Profile";
	pHand->m_Def.m_bOneChoiceForAllPlayers = false;
	pHand->m_Def.m_bAllowThemeItems = false;
	pHand->m_Def.m_vEnabledForPlayers.clear();

	FOREACH_PlayerNumber( pn )
		pHand->m_Def.m_vEnabledForPlayers.insert( pn );

	PROFILEMAN->GetLocalProfileDisplayNames( pHand->m_Def.m_vsChoices );

	if( pHand->m_Def.m_vsChoices.empty() )
	{
		// Give myself a message so that I can bail out later
		PostScreenMessage(SM_NoProfilesDefined, 0);
		SAFE_DELETE(pHand);
	}
	else
		vHands.push_back( pHand );

	InitMenu( vHands );
	SOUND->PlayMusic( THEME->GetPathS("ScreenOptionsServiceChild", "music"));
	OptionRow &row = *m_pRows.back();
	row.SetExitText("Login");
}

void ScreenSMOnlineLogin::ImportOptions( int iRow, const vector<PlayerNumber> &vpns )
{
	switch( iRow )
	{
	case 0:
		{
			vector<RString> vsProfiles;
			PROFILEMAN->GetLocalProfileIDs( vsProfiles );

			FOREACH_PlayerNumber( pn )
			{
				vector<RString>::iterator iter = find(vsProfiles.begin(), vsProfiles.end(), ProfileManager::m_sDefaultLocalProfileID[pn].Get() );
				if( iter != vsProfiles.end() )
					m_pRows[0]->SetOneSelection((PlayerNumber) pn, iter - vsProfiles.begin());
			}
		}
		break;
	}
}

void ScreenSMOnlineLogin::ExportOptions( int iRow, const vector<PlayerNumber> &vpns )
{
	switch( iRow )
	{
	case 0:
		{
			vector<RString> vsProfiles;
			PROFILEMAN->GetLocalProfileIDs( vsProfiles );

			FOREACH_EnabledPlayer( pn )
				ProfileManager::m_sDefaultLocalProfileID[pn].Set( vsProfiles[m_pRows[0]->GetOneSelection(pn)] );
		}
		break;
	}
}

static LocalizedString UNIQUE_PROFILE		( "ScreenSMOnlineLogin", "Each player needs a unique Profile." );
static LocalizedString YOU_ARE_LOGGING_ON_AS	( "ScreenSMOnlineLogin", "You are logging on as:" );
static LocalizedString ENTER_YOUR_PASSWORD	( "ScreenSMOnlineLogin", "Enter your password." );

void ScreenSMOnlineLogin::HandleScreenMessage(const ScreenMessage SM)
{
	RString sLoginQuestion;
//	if( GAMESTATE->IsPlayerEnabled((PlayerNumber) m_iPlayer) )

	if( SM == SM_PasswordDone )
	{
		if(!ScreenTextEntry::s_bCancelledLast)
			SendLogin(ScreenTextEntry::s_sLastAnswer);
		else
			SCREENMAN->PostMessageToTopScreen( SM_GoToPrevScreen, 0 );
	}
	else if( SM == SM_NoProfilesDefined )
	{
		SCREENMAN->SystemMessage(DEFINE_A_PROFILE);
		SCREENMAN->SetNewScreen("ScreenOptionsManageProfiles");
	}
	else if( SM == SM_SMOnlinePack )
	{
		LOG->Trace("[ScreenSMOnlineLogin::HandleScreenMessage] SMOnlinePack");
		if(!GAMESTATE->IsPlayerEnabled((PlayerNumber) m_iPlayer))
		{
			LOG->Warn("Invalid player number: %i", m_iPlayer);
			return;
		}

		// This can cause problems in certain situations -aj
		sLoginQuestion = YOU_ARE_LOGGING_ON_AS.GetValue() + "\n"
			+ GAMESTATE->GetPlayerDisplayName((PlayerNumber) m_iPlayer) + "\n" +
			ENTER_YOUR_PASSWORD.GetValue();
		int ResponseCode = NSMAN->m_SMOnlinePacket.Read1();
		if (ResponseCode == 0)
		{
			int Status = NSMAN->m_SMOnlinePacket.Read1();
			if(Status == 0)
			{
				NSMAN->isSMOLoggedIn[m_iPlayer] = true;
				m_iPlayer++;
				if( GAMESTATE->IsPlayerEnabled((PlayerNumber) m_iPlayer) && m_iPlayer < NUM_PLAYERS )
				{
					ScreenTextEntry::Password(SM_PasswordDone, sLoginQuestion, NULL );
				}
				else
				{
					SCREENMAN->SetNewScreen( THEME->GetMetric (m_sName,"NextScreen") );
					m_iPlayer = 0;
				}
			}
			else
			{
				RString Response = NSMAN->m_SMOnlinePacket.ReadNT();
				ScreenTextEntry::Password( SM_PasswordDone, Response + "\n\n" + sLoginQuestion, NULL );
			}
		}
	}
	else if( SM == SM_GoToNextScreen )
	{
		LOG->Trace("[ScreenSMOnlineLogin::HandleScreenMessage] GoToNextScreen");
		vector<PlayerNumber> v;
		v.push_back( GAMESTATE->GetMasterPlayerNumber() );
		for( unsigned r=0; r<m_pRows.size(); r++ )
			ExportOptions( r, v );

		PREFSMAN->SavePrefsToDisk();
		FOREACH_EnabledPlayer(pn)
		{
			PROFILEMAN->LoadLocalProfileFromMachine((PlayerNumber) pn);
		}

		if(GAMESTATE->IsPlayerEnabled((PlayerNumber) 0) && GAMESTATE->IsPlayerEnabled((PlayerNumber) 1) &&
			(GAMESTATE->GetPlayerDisplayName((PlayerNumber) 0) == GAMESTATE->GetPlayerDisplayName((PlayerNumber) 1)))
		{
			SCREENMAN->SystemMessage( UNIQUE_PROFILE );
			SCREENMAN->SetNewScreen("ScreenSMOnlineLogin");
		}
		else
		{
			m_iPlayer=0;
			while(!GAMESTATE->IsPlayerEnabled((PlayerNumber) m_iPlayer))
				++m_iPlayer;
			sLoginQuestion = YOU_ARE_LOGGING_ON_AS.GetValue() + "\n" + GAMESTATE->GetPlayerDisplayName((PlayerNumber) m_iPlayer) + "\n" + ENTER_YOUR_PASSWORD.GetValue();
			ScreenTextEntry::Password(SM_PasswordDone, sLoginQuestion, NULL );
		}
		return;
	}

	ScreenOptions::HandleScreenMessage(SM);
}

bool ScreenSMOnlineLogin::MenuStart( const InputEventPlus &input )
{
	return ScreenOptions::MenuStart( input );
}

RString ScreenSMOnlineLogin::GetSelectedProfileID()
{
	vector<RString> vsProfiles;
	PROFILEMAN->GetLocalProfileIDs( vsProfiles );

	const OptionRow &row = *m_pRows[GetCurrentRow()];
	const int Selection = row.GetOneSharedSelection();
	if( !Selection )
		return RString();
	return vsProfiles[ Selection-1 ];
}

void ScreenSMOnlineLogin::SendLogin( RString sPassword )
{
	RString PlayerName = GAMESTATE->GetPlayerDisplayName( (PlayerNumber) m_iPlayer );

	RString HashedName = NSMAN->MD5Hex( sPassword );

	int authMethod = 0;
	if ( NSMAN->GetSMOnlineSalt() != 0 )
	{
		authMethod = 1;
		HashedName = NSMAN->MD5Hex( HashedName + ssprintf("%d", NSMAN->GetSMOnlineSalt()) );
	}

	NSMAN->m_SMOnlinePacket.ClearPacket();
	NSMAN->m_SMOnlinePacket.Write1( (uint8_t)0 );		//Login command
	NSMAN->m_SMOnlinePacket.Write1( (uint8_t)m_iPlayer );	//Player
	NSMAN->m_SMOnlinePacket.Write1( (uint8_t)authMethod );	//MD5 hash style
	NSMAN->m_SMOnlinePacket.WriteNT( PlayerName );
	NSMAN->m_SMOnlinePacket.WriteNT( HashedName );
	NSMAN->SendSMOnline();
}

#endif

/*
 * (c) 2004-2005 Charles Lohr, Adam Lowman
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
