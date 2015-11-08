#include "global.h"
#include "Transition.h"
#include "ScreenManager.h"

Transition::Transition()
{
	m_State = waiting;
}

void Transition::Load( RString sBGAniDir )
{
	this->RemoveAllChildren();

	m_sprTransition.Load( sBGAniDir );
	this->AddChild( m_sprTransition );

	m_State = waiting;
}


void Transition::UpdateInternal( float fDeltaTime )
{
	if( m_State != transitioning )
		return;

	// Check this before running Update, so we draw the last frame of the finished
	// transition before sending m_MessageToSendWhenDone.
	if( m_sprTransition->GetTweenTimeLeft() == 0 )	// over
	{
		m_State = finished;
		SCREENMAN->SendMessageToTopScreen( m_MessageToSendWhenDone );
	}

	ActorFrame::UpdateInternal( fDeltaTime );
}

void Transition::Reset()
{
	m_State = waiting;
	m_bFirstUpdate = true;

	if( m_sprTransition.IsLoaded() )
		m_sprTransition->FinishTweening();
}

bool Transition::EarlyAbortDraw() const
{
	return m_State == waiting;
}

void Transition::StartTransitioning( ScreenMessage send_when_done )
{
	if( m_State != waiting )
		return;	// ignore
	
	// If transition isn't loaded don't set state to transitioning.
	// We assume elsewhere that m_sprTransition is loaded.
	if( !m_sprTransition.IsLoaded() )
		return;
	
	m_sprTransition->PlayCommand( "StartTransitioning" );

	m_MessageToSendWhenDone = send_when_done;
	m_State = transitioning;
}

float Transition::GetTweenTimeLeft() const
{
	if( m_State != transitioning )
		return 0;

	return m_sprTransition->GetTweenTimeLeft();
}

/*
 * (c) 2001-2004 Chris Danford
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
