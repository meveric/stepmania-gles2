#include "global.h"
#include "WorkoutGraph.h"
#include "RageUtil.h"
#include "ActorUtil.h"
#include "Sprite.h"
#include "Trail.h"
#include "Steps.h"
#include "GameState.h"
#include "ThemeManager.h"
#include "StatsManager.h"
#include "Foreach.h"
#include "Course.h"
#include "Style.h"

const int MAX_METERS_TO_SHOW = 50;

REGISTER_ACTOR_CLASS( WorkoutGraph );

WorkoutGraph::WorkoutGraph()
{
	m_iSongsChoppedOffAtBeginning = 0;
}

WorkoutGraph::~WorkoutGraph()
{
	FOREACH( Sprite*, m_vpBars, a )
		delete *a;
	m_vpBars.clear();
}

void WorkoutGraph::Load()
{
	m_sprEmpty.Load( THEME->GetPathG("WorkoutGraph","empty") );
	this->AddChild( &m_sprEmpty );
}

void WorkoutGraph::LoadFromNode( const XNode* pNode )
{
	ActorFrame::LoadFromNode( pNode );

	Load();
}

void WorkoutGraph::SetFromCurrentWorkout()
{
	SetInternal( 0 );
}

void WorkoutGraph::SetInternal( int iMinSongsPlayed )
{
	FOREACH( Sprite*, m_vpBars, p )
	{
		this->RemoveChild( *p );
		delete *p;
	}
	m_vpBars.clear();

	Trail *pTrail = GAMESTATE->m_pCurTrail[PLAYER_1];
	if( pTrail == NULL )
		return;

	vector<int> viMeters;
	FOREACH_CONST( TrailEntry, pTrail->m_vEntries, e )
	{
		ASSERT( e->pSteps != NULL );
		viMeters.push_back( e->pSteps->GetMeter() );
	}

	int iBlocksWide = viMeters.size();
	int iBlocksHigh = MAX_METER;

	const float fMaxWidth = 300;
	float fTotalWidth = SCALE( iBlocksWide, 1.0f, 10.0f, 50.0f, fMaxWidth );
	CLAMP( fTotalWidth, 50, fMaxWidth );

	const float fMaxHeight = 130;
	float fTotalHeight = SCALE( iBlocksHigh, 1.0f, 10.0f, 50.0f, fMaxHeight );
	CLAMP( fTotalHeight, 50, fMaxHeight );

	float fBlockSize = min( fTotalWidth / iBlocksWide, fTotalHeight / iBlocksHigh );

	m_sprEmpty.SetVertAlign( align_bottom );
	m_sprEmpty.SetCustomImageRect( RectF(0,0,(float)iBlocksWide,(float)iBlocksHigh) );
	m_sprEmpty.ZoomToWidth( iBlocksWide * fBlockSize );
	m_sprEmpty.ZoomToHeight( iBlocksHigh * fBlockSize );

	FOREACH_CONST( int, viMeters, iter )
	{
		int iIndex = iter - viMeters.begin();
		float fOffsetFromCenter = iIndex - (iBlocksWide-1)/2.0f;
		Sprite *p = new Sprite;
		p->Load( THEME->GetPathG("WorkoutGraph","bar") );
		p->SetVertAlign( align_bottom );
		p->ZoomToWidth( fBlockSize );
		int iMetersToCover = (MAX_METER - *iter);
		p->SetCustomImageRect( RectF(0,(float)iMetersToCover/(float)iBlocksHigh,1,1) );
		p->ZoomToHeight( *iter * fBlockSize );
		p->SetX( fOffsetFromCenter * fBlockSize );
		m_vpBars.push_back( p );
		this->AddChild( p );
	}
}

void WorkoutGraph::SetFromGameStateAndHighlightSong( int iSongIndex )
{
	SetInternal( iSongIndex+1 );

	FOREACH( Sprite*, m_vpBars, spr )
		(*spr)->StopEffect();

	int iBarIndex = iSongIndex - m_iSongsChoppedOffAtBeginning;

	if( iBarIndex < (int)m_vpBars.size() )
		m_vpBars[iBarIndex]->SetEffectGlowBlink(0.3f);
}


// lua start
#include "LuaBinding.h"

/** @brief Allow Lua to have access to the WorkoutGraph. */ 
class LunaWorkoutGraph: public Luna<WorkoutGraph>
{
public:
	static int SetFromCurrentWorkout( T* p, lua_State *L )			{ p->SetFromCurrentWorkout(); return 0; }
	static int SetFromGameStateAndHighlightSong( T* p, lua_State *L )	{ p->SetFromGameStateAndHighlightSong(IArg(1)); return 0; }

	LunaWorkoutGraph()
	{
		ADD_METHOD( SetFromCurrentWorkout );
		ADD_METHOD( SetFromGameStateAndHighlightSong );
	}
};

LUA_REGISTER_DERIVED_CLASS( WorkoutGraph, ActorFrame )
// lua end

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
