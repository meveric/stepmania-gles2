#include "global.h"
#include "Screen.h"
#include "PrefsManager.h"
#include "RageSound.h"
#include "RageLog.h"
#include "ThemeManager.h"
#include "ScreenManager.h"
#include "ActorUtil.h"
#include "InputEventPlus.h"

#define NEXT_SCREEN		THEME->GetMetric (m_sName,"NextScreen")
#define PREV_SCREEN		THEME->GetMetric (m_sName,"PrevScreen")
#define PREPARE_SCREENS		THEME->GetMetric (m_sName,"PrepareScreens")
#define PERSIST_SCREENS		THEME->GetMetric (m_sName,"PersistScreens")
#define GROUPED_SCREENS		THEME->GetMetric (m_sName,"GroupedScreens")

static const char *ScreenTypeNames[] = {
	"Attract",
	"GameMenu",
	"Gameplay",
	"SystemMenu",
};
XToString( ScreenType );
LuaXType( ScreenType );

void Screen::InitScreen( Screen *pScreen )
{
	pScreen->Init();
}

Screen::~Screen()
{

}

bool Screen::SortMessagesByDelayRemaining( const Screen::QueuedScreenMessage &m1,
					   const Screen::QueuedScreenMessage &m2 )
{
	return m1.fDelayRemaining < m2.fDelayRemaining;
}

void Screen::Init()
{
	ALLOW_OPERATOR_MENU_BUTTON.Load( m_sName, "AllowOperatorMenuButton" );
	HANDLE_BACK_BUTTON.Load( m_sName, "HandleBackButton" );
	REPEAT_RATE.Load( m_sName, "RepeatRate" );
	REPEAT_DELAY.Load( m_sName, "RepeatDelay" );
	LIGHTS_MODE.Load( m_sName, "LightsMode" );

	m_Codes.Load( m_sName );

	SetFOV( 0 );

	m_smSendOnPop = SM_None;
	m_bRunning = false;

	ActorUtil::LoadAllCommandsFromName( *this, m_sName, "Screen" );

	PlayCommandNoRecurse( Message("Init") );

	vector<RString> asList;
	split( PREPARE_SCREENS, ",", asList );
	for( unsigned i = 0; i < asList.size(); ++i )
	{
		LOG->Trace( "Screen \"%s\" preparing \"%s\"", m_sName.c_str(), asList[i].c_str() );
		SCREENMAN->PrepareScreen( asList[i] );
	}

	asList.clear();
	split( GROUPED_SCREENS, ",", asList );
	for( unsigned i = 0; i < asList.size(); ++i )
		SCREENMAN->GroupScreen( asList[i] );

	asList.clear();
	split( PERSIST_SCREENS, ",", asList );
	for( unsigned i = 0; i < asList.size(); ++i )
		SCREENMAN->PersistantScreen( asList[i] );
}

void Screen::BeginScreen()
{
	m_bRunning = true;
	m_bFirstUpdate = true;

	/* Screens set these when they determine their next screen dynamically. Reset them
	 * here, so a reused screen doesn't inherit these from the last time it was used. */
	m_sNextScreen = RString();

	m_fLockInputSecs = 0;

	this->RunCommands( THEME->GetMetricA(m_sName, "ScreenOnCommand") );

	if( m_fLockInputSecs == 0 )
		m_fLockInputSecs = 0.0001f; // always lock for a tiny amount of time so that we throw away any queued inputs during the load.

	this->PlayCommand( "Begin" );
}

void Screen::EndScreen()
{
	this->PlayCommand( "End" );
	m_bRunning = false;
}

void Screen::Update( float fDeltaTime )
{
	ActorFrame::Update( fDeltaTime );
	
	m_fLockInputSecs = max( 0, m_fLockInputSecs-fDeltaTime );

	/* We need to ensure two things:
	 * 1. Messages must be sent in the order of delay. If two messages are sent
	 *    simultaneously, one with a .001 delay and another with a .002 delay,
	 *    the .001 delay message must be sent first.
	 * 2. Messages to be delivered simultaneously must be sent in the order queued.
	 * 
	 * Sort by time to ensure #1; use a stable sort to ensure #2. */
	stable_sort(m_QueuedMessages.begin(), m_QueuedMessages.end(), SortMessagesByDelayRemaining);

	// Update the times of queued ScreenMessages.
	for( unsigned i=0; i<m_QueuedMessages.size(); i++ )
	{
		/* Hack:
		 * If we simply subtract time and then send messages, we have a problem.
		 * Messages are queued to arrive at specific times, and those times line
		 * up with things like tweens finishing. If we send the message at the
		 * exact time given, then it'll be on the same cycle that would be rendering
		 * the last frame of a tween (such as an object going off the screen).
		 * However, when we send the message, we're likely to set up a new screen,
		 * which causes everything to stop in place; this results in actors
		 * occasionally not quite finishing their tweens.
		 * Let's delay all messages that have a non-zero time an extra frame. */
		if( m_QueuedMessages[i].fDelayRemaining > 0.0001f )
		{
			m_QueuedMessages[i].fDelayRemaining -= fDeltaTime;
			m_QueuedMessages[i].fDelayRemaining = max( m_QueuedMessages[i].fDelayRemaining, 0.0001f );
		}
		else
		{
			m_QueuedMessages[i].fDelayRemaining -= fDeltaTime;
		}
	}

	/* Now dispatch messages. If the number of messages on the queue changes
	 * within HandleScreenMessage, someone cleared messages on the queue. This
	 * means we have no idea where 'i' is, so start over. Since we applied time
	 * already, this won't cause messages to be mistimed. */
	for( unsigned i=0; i<m_QueuedMessages.size(); i++ )
	{
		if( m_QueuedMessages[i].fDelayRemaining > 0.0f )
			continue; /* not yet */

		// Remove the message from the list.
		const ScreenMessage SM = m_QueuedMessages[i].SM;
		m_QueuedMessages.erase( m_QueuedMessages.begin()+i );
		i--;

		unsigned iSize = m_QueuedMessages.size();

		// send this sucker!
		CHECKPOINT_M( ssprintf("ScreenMessage(%s)", ScreenMessageHelpers::ScreenMessageToString(SM).c_str()) );
		this->HandleScreenMessage( SM );

		// If the size changed, start over.
		if( iSize != m_QueuedMessages.size() )
			i = 0;
	}
}

/* Returns true if the input was handled, or false if not handled.  For
 * overlays, this determines whether the event will be propagated to lower
 * screens (i.e. it propagates from an overlay only when this returns false). */
bool Screen::Input( const InputEventPlus &input )
{
	Message msg("");
	if( m_Codes.InputMessage(input, msg) )
	{
		this->HandleMessage( msg );
		return true;
	}

	// Don't send release messages with the default handler.
	switch( input.type )
	{
	case IET_FIRST_PRESS:
	case IET_REPEAT:
		break; // OK
	default:
		return false; // don't care
	}

	// Always broadcast mouse input so themers can grab it. -aj
	if( input.DeviceI == DeviceInput( DEVICE_MOUSE, MOUSE_LEFT ) )
		MESSAGEMAN->Broadcast( (MessageID)(Message_LeftClick) );
	if( input.DeviceI == DeviceInput( DEVICE_MOUSE, MOUSE_RIGHT ) )
		MESSAGEMAN->Broadcast( (MessageID)(Message_RightClick) );
	if( input.DeviceI == DeviceInput( DEVICE_MOUSE, MOUSE_MIDDLE ) )
		MESSAGEMAN->Broadcast( (MessageID)(Message_MiddleClick) );
	// Can't do MouseWheelUp and MouseWheelDown at the same time. -aj
	if( input.DeviceI == DeviceInput( DEVICE_MOUSE, MOUSE_WHEELUP ) )
		MESSAGEMAN->Broadcast( (MessageID)(Message_MouseWheelUp) );
	else if( input.DeviceI == DeviceInput( DEVICE_MOUSE, MOUSE_WHEELDOWN ) )
		MESSAGEMAN->Broadcast( (MessageID)(Message_MouseWheelDown) );

	// default input handler used by most menus
	switch( input.MenuI )
	{
		case GAME_BUTTON_MENUUP:    return this->MenuUp   ( input );
		case GAME_BUTTON_MENUDOWN:  return this->MenuDown ( input );
		case GAME_BUTTON_MENULEFT:  return this->MenuLeft ( input );
		case GAME_BUTTON_MENURIGHT: return this->MenuRight( input );
		case GAME_BUTTON_BACK:
			// Don't make the user hold the back button if they're pressing escape and escape is the back button.
			if( !PREFSMAN->m_bDelayedBack || input.type==IET_REPEAT || (input.DeviceI.device == DEVICE_KEYBOARD && input.DeviceI.button == KEY_ESC) )
			{
				if( HANDLE_BACK_BUTTON )
					return this->MenuBack( input );
			}
			return false;
		case GAME_BUTTON_START:  return this->MenuStart ( input );
		case GAME_BUTTON_SELECT: return this->MenuSelect( input );
		case GAME_BUTTON_COIN:   return this->MenuCoin  ( input );
		default: return false;
	}
}

void Screen::HandleScreenMessage( const ScreenMessage SM )
{
	if( SM == SM_GoToNextScreen || SM == SM_GoToPrevScreen )
	{
		if( SCREENMAN->IsStackedScreen(this) )
			SCREENMAN->PopTopScreen( m_smSendOnPop );
		else
			SCREENMAN->SetNewScreen( SM == SM_GoToNextScreen? GetNextScreenName():GetPrevScreen() );
	}
	else if( SM == SM_GainFocus )
	{
		if( REPEAT_RATE != -1.0f )
			INPUTFILTER->SetRepeatRate( REPEAT_RATE );
		if( REPEAT_DELAY != -1.0f )
			INPUTFILTER->SetRepeatDelay( REPEAT_DELAY );

		LIGHTSMAN->SetLightsMode( LIGHTS_MODE );
	}
	else if( SM == SM_LoseFocus )
	{
		INPUTFILTER->ResetRepeatRate();
	}
}

RString Screen::GetNextScreenName() const
{
	if( !m_sNextScreen.empty() )
		return m_sNextScreen;
	return NEXT_SCREEN;
}

RString Screen::GetPrevScreen() const
{
	if( !m_sPrevScreen.empty() )
		return m_sPrevScreen;
	return PREV_SCREEN;
}

void Screen::PostScreenMessage( const ScreenMessage SM, float fDelay )
{
	ASSERT( fDelay >= 0.0 );

	QueuedScreenMessage QSM;
	QSM.SM = SM;
	QSM.fDelayRemaining = fDelay;
	m_QueuedMessages.push_back( QSM );
}

void Screen::ClearMessageQueue()
{
	m_QueuedMessages.clear(); 
}

void Screen::ClearMessageQueue( const ScreenMessage SM )
{
	for( int i=m_QueuedMessages.size()-1; i>=0; i-- )
		if( m_QueuedMessages[i].SM == SM )
			m_QueuedMessages.erase( m_QueuedMessages.begin()+i ); 
}

// lua start
#include "LuaBinding.h"

/** @brief Allow Lua to have access to the Screen. */ 
class LunaScreen: public Luna<Screen>
{
public:
	static int GetNextScreenName( T* p, lua_State *L ) { lua_pushstring(L, p->GetNextScreenName() ); return 1; }
	static int GetPrevScreenName( T* p, lua_State *L ) { lua_pushstring(L, p->GetPrevScreen() ); return 1; }
	static int lockinput( T* p, lua_State *L ) { p->SetLockInputSecs(FArg(1)); return 0; }
	DEFINE_METHOD( GetScreenType,	GetScreenType() )

	static int PostScreenMessage( T* p, lua_State *L )
	{
		RString sMessage = SArg(1);
		ScreenMessage SM = ScreenMessageHelpers::ToScreenMessage( sMessage );
		p->PostScreenMessage( SM, IArg(2) );
		return 0;
	}

	LunaScreen()
	{
		ADD_METHOD( GetNextScreenName );
		ADD_METHOD( GetPrevScreenName );
		ADD_METHOD( PostScreenMessage );
		ADD_METHOD( lockinput );
		ADD_METHOD( GetScreenType );
	}
};

LUA_REGISTER_DERIVED_CLASS( Screen, ActorFrame )
// lua end

/*
 * (c) 2001-2004 Chris Danford, Glenn Maynard
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
