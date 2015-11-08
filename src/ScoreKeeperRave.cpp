#include "global.h"
#include "ScoreKeeperRave.h"
#include "ThemeManager.h"
#include "RageUtil.h"
#include "GameState.h"
#include "Character.h"
#include "ScreenManager.h"
#include "PrefsManager.h"
#include "ThemeMetric.h"
#include "PlayerState.h"
#include "NoteTypes.h"

ThemeMetric<float> ATTACK_DURATION_SECONDS	("ScoreKeeperRave","AttackDurationSeconds");

static const float g_fSuperMeterPercentChangeInit[] =
{
	+0.02f, // SE_CheckpointHit
	+0.05f, // SE_W1
	+0.04f, // SE_W2
	+0.02f, // SE_W3
	+0.00f, // SE_W4
	+0.00f, // SE_W5
	-0.20f, // SE_Miss
	-0.40f, // SE_HitMine
	-0.02f, // SE_CheckpointMiss
	+0.04f, // SE_Held
	-0.20f, // SE_LetGo
};
COMPILE_ASSERT( ARRAYLEN(g_fSuperMeterPercentChangeInit) == NUM_ScoreEvent );

static void SuperMeterPercentChangeInit( size_t /*ScoreEvent*/ i, RString &sNameOut, float &defaultValueOut )
{
	sNameOut = "SuperMeterPercentChange" + ScoreEventToString( (ScoreEvent)i );
	defaultValueOut = g_fSuperMeterPercentChangeInit[i];
}

static Preference1D<float> g_fSuperMeterPercentChange( SuperMeterPercentChangeInit, NUM_ScoreEvent );

ScoreKeeperRave::ScoreKeeperRave( PlayerState *pPlayerState, PlayerStageStats *pPlayerStageStats ) : 
	ScoreKeeper(pPlayerState, pPlayerStageStats)
{
}

void ScoreKeeperRave::HandleTapScore( const TapNote &tn )
{
	TapNoteScore score = tn.result.tns;
	float fPercentToMove = 0;

	if( score == TNS_HitMine )
		fPercentToMove = g_fSuperMeterPercentChange[SE_HitMine];

	AddSuperMeterDelta( fPercentToMove );
}

#define CROSSED( val ) (fOld < val && fNew >= val)
#define CROSSED_ATTACK_LEVEL( level ) CROSSED(1.f/NUM_ATTACK_LEVELS*(level+1))
void ScoreKeeperRave::HandleTapRowScore( const NoteData &nd, int iRow )
{
	TapNoteScore scoreOfLastTap;
	int iNumTapsInRow;
	float fPercentToMove = 0.0f;

	GetScoreOfLastTapInRow( nd, iRow, scoreOfLastTap, iNumTapsInRow );
	if( iNumTapsInRow <= 0 )
		return;
	switch( scoreOfLastTap )
	{
		DEFAULT_FAIL( scoreOfLastTap );
		case TNS_W1:	fPercentToMove = g_fSuperMeterPercentChange[SE_W1];		break;
		case TNS_W2:	fPercentToMove = g_fSuperMeterPercentChange[SE_W2];		break;
		case TNS_W3:	fPercentToMove = g_fSuperMeterPercentChange[SE_W3];		break;
		case TNS_W4:	fPercentToMove = g_fSuperMeterPercentChange[SE_W4];		break;
		case TNS_W5:	fPercentToMove = g_fSuperMeterPercentChange[SE_W5];		break;
		case TNS_Miss:	fPercentToMove = g_fSuperMeterPercentChange[SE_Miss];	break;
	}
	AddSuperMeterDelta( fPercentToMove );
}

void ScoreKeeperRave::HandleHoldScore( const TapNote &tn )
{
	// todo: should hit mine be handled in HandleTapRow score instead? -aj
	TapNoteScore tapScore = tn.result.tns;
	float fPercentToMove = 0.0f;
	switch( tapScore )
	{
		case TNS_HitMine:
			fPercentToMove = g_fSuperMeterPercentChange[SE_HitMine];
			break;
		default: break;
	}

	// Playing with this code enabled seems to feel "wrong", but I'm leaving it
	// in for player feedback. -aj
	HoldNoteScore holdScore = tn.HoldResult.hns;
	switch( holdScore )
	{
		case HNS_Held: fPercentToMove = g_fSuperMeterPercentChange[SE_Held]; break;
		case HNS_LetGo: fPercentToMove = g_fSuperMeterPercentChange[SE_LetGo]; break;
		default: break;
	}
	AddSuperMeterDelta( fPercentToMove );
}

extern ThemeMetric<bool> PENALIZE_TAP_SCORE_NONE;
void ScoreKeeperRave::HandleTapScoreNone()
{
	if( PENALIZE_TAP_SCORE_NONE )
	{
		float fPercentToMove = g_fSuperMeterPercentChange[SE_Miss];
		AddSuperMeterDelta( fPercentToMove );
	}
}

void ScoreKeeperRave::AddSuperMeterDelta( float fUnscaledPercentChange )
{
	if( PREFSMAN->m_bMercifulDrain  &&  fUnscaledPercentChange<0 )
	{
		float fSuperPercentage = m_pPlayerState->m_fSuperMeter / NUM_ATTACK_LEVELS;
		fUnscaledPercentChange *= SCALE( fSuperPercentage, 0.f, 1.f, 0.5f, 1.f);
	}

	// more mercy: Grow super meter slower or faster depending on life.
	if( PREFSMAN->m_bMercifulSuperMeter )
	{
		float fLifePercentage = 0;
		switch( m_pPlayerState->m_PlayerNumber )
		{
		case PLAYER_1:	fLifePercentage = GAMESTATE->m_fTugLifePercentP1;		break;
		case PLAYER_2:	fLifePercentage = 1 - GAMESTATE->m_fTugLifePercentP1;	break;
		default:
			FAIL_M(ssprintf("Invalid player number: %i", m_pPlayerState->m_PlayerNumber));
		}
		CLAMP( fLifePercentage, 0.f, 1.f );
		if( fUnscaledPercentChange > 0 )
			fUnscaledPercentChange *= SCALE( fLifePercentage, 0.f, 1.f, 1.7f, 0.3f);
		else	// fUnscaledPercentChange <= 0
			fUnscaledPercentChange /= SCALE( fLifePercentage, 0.f, 1.f, 1.7f, 0.3f);
	}

	// mercy: drop super meter faster if at a higher level
	if( fUnscaledPercentChange < 0 )
		fUnscaledPercentChange *= SCALE( m_pPlayerState->m_fSuperMeter, 0.f, 1.f, 0.01f, 1.f );

	AttackLevel oldAL = (AttackLevel)(int)m_pPlayerState->m_fSuperMeter;

	float fPercentToMove = fUnscaledPercentChange;
	m_pPlayerState->m_fSuperMeter += fPercentToMove * m_pPlayerState->m_fSuperMeterGrowthScale;
	CLAMP( m_pPlayerState->m_fSuperMeter, 0.f, NUM_ATTACK_LEVELS );

	AttackLevel newAL = (AttackLevel)(int)m_pPlayerState->m_fSuperMeter;

	if( newAL > oldAL )
	{
		LaunchAttack( oldAL );
		if( newAL == NUM_ATTACK_LEVELS )	// hit upper bounds of meter
			m_pPlayerState->m_fSuperMeter -= 1.f;
	}

	// mercy: if losing remove attacks on life drain
	if( fUnscaledPercentChange < 0 )
	{
		bool bWinning;
		switch( m_pPlayerState->m_PlayerNumber )
		{
		case PLAYER_1:	bWinning = GAMESTATE->m_fTugLifePercentP1 > 0.5f;	break;
		case PLAYER_2:	bWinning = GAMESTATE->m_fTugLifePercentP1 < 0.5f;	break;
		default:
			bWinning = false;
			FAIL_M(ssprintf("Invalid player number: %i", m_pPlayerState->m_PlayerNumber));
		}
		if( !bWinning )
			m_pPlayerState->EndActiveAttacks();
	}
}

void ScoreKeeperRave::LaunchAttack( AttackLevel al )
{
	PlayerNumber pn = m_pPlayerState->m_PlayerNumber;

	RString* asAttacks = GAMESTATE->m_pCurCharacters[pn]->m_sAttacks[al];	// [NUM_ATTACKS_PER_LEVEL]
	RString sAttackToGive;

	if (GAMESTATE->m_pCurCharacters[pn] != NULL)		
		sAttackToGive = asAttacks[ RandomInt(NUM_ATTACKS_PER_LEVEL) ];
	else
	{
		// "If you add any noteskins here, you need to make sure they're cached, too." -?
		// Noteskins probably won't work here anymore. -aj
		RString DefaultAttacks[8] = { "1.5x", "2.0x", "0.5x", "reverse", "sudden", "boost", "brake", "wave" };
		sAttackToGive = DefaultAttacks[ RandomInt(8) ];
	}

	PlayerNumber pnToAttack = OPPOSITE_PLAYER[pn];
	PlayerState *pPlayerStateToAttack = GAMESTATE->m_pPlayerState[pnToAttack];

	Attack a;
	a.level = al;
	a.fSecsRemaining = ATTACK_DURATION_SECONDS;
	a.sModifiers = sAttackToGive;

	// remove current attack (if any)
	pPlayerStateToAttack->RemoveActiveAttacks();

	// apply new attack
	pPlayerStateToAttack->LaunchAttack( a );

//	SCREENMAN->SystemMessage( ssprintf( "attacking %d with %s", pnToAttack, sAttackToGive.c_str() ) );
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
