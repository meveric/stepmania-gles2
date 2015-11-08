#ifndef PLAYER_H
#define PLAYER_H

#include "ActorFrame.h"
#include "HoldJudgment.h"
#include "NoteDataWithScoring.h"
#include "RageSound.h"
#include "AttackDisplay.h"
#include "NoteData.h"
#include "ScreenMessage.h"
#include "ThemeMetric.h"
#include "InputEventPlus.h"
#include "TimingData.h"

class ScoreDisplay;
class LifeMeter;
class CombinedLifeMeter;
class ScoreKeeper;
class Inventory;
class RageTimer;
class NoteField;
class PlayerStageStats;
class JudgedRows;

// todo: replace these with a Message and MESSAGEMAN? -aj
AutoScreenMessage( SM_100Combo );
AutoScreenMessage( SM_200Combo );
AutoScreenMessage( SM_300Combo );
AutoScreenMessage( SM_400Combo );
AutoScreenMessage( SM_500Combo );
AutoScreenMessage( SM_600Combo );
AutoScreenMessage( SM_700Combo );
AutoScreenMessage( SM_800Combo );
AutoScreenMessage( SM_900Combo );
AutoScreenMessage( SM_1000Combo );
AutoScreenMessage( SM_ComboStopped );
AutoScreenMessage( SM_ComboContinuing );

/** @brief Accepts input, knocks down TapNotes that were stepped on, and keeps score for the player. */
class Player: public ActorFrame
{
public:
	// The passed in NoteData isn't touched until Load() is called.
	Player( NoteData &nd, bool bVisibleParts = true );
	~Player();

	virtual void Update( float fDeltaTime );
	virtual void DrawPrimitives();

	struct TrackRowTapNote
	{
		int iTrack;
		int iRow;
		TapNote *pTN;
	};
	void UpdateHoldNotes( int iSongRow, float fDeltaTime, vector<TrackRowTapNote> &vTN );

	void Init( 
		const RString &sType,
		PlayerState* pPlayerState, 
		PlayerStageStats* pPlayerStageStats,
		LifeMeter* pLM, 
		CombinedLifeMeter* pCombinedLM, 
		ScoreDisplay* pScoreDisplay, 
		ScoreDisplay* pSecondaryScoreDisplay, 
		Inventory* pInventory, 
		ScoreKeeper* pPrimaryScoreKeeper, 
		ScoreKeeper* pSecondaryScoreKeeper );
	void Load();
	void CrossedRows( int iLastRowCrossed, const RageTimer &now );
	bool IsOniDead() const;
	
	/**
	 * @brief Retrieve the Player's TimingData.
	 *
	 * This is primarily for a lua hook.
	 * @return the TimingData in question. */
	TimingData GetPlayerTimingData() const
	{
		return *(this->m_Timing);
	}

	// Called when a fret, step, or strum type button changes
	void Fret( int col, int row, const RageTimer &tm, bool bHeld, bool bRelease );

	// Called when the strum bar is pressed down
	void Strum( int col, int row, const RageTimer &tm, bool bHeld, bool bRelease );

	// Called when the strum window passes without a row being hit
	void DoStrumMiss();
	void ScoreAllActiveHoldsLetGo();
	void DoTapScoreNone();

	enum ButtonType { ButtonType_Step, ButtonType_StrumFretsChanged, ButtonType_Hopo };
	void StepStrumHopo( int col, int row, const RageTimer &tm, bool bHeld, bool bRelease, ButtonType gbt );
	void Step( int col, int row, const RageTimer &tm, bool bHeld, bool bRelease )	{ StepStrumHopo(col, row, tm, bHeld, bRelease, ButtonType_Step); }

	// called by Fret for Hammer-ons and Pull-offs
	void Hopo( int col, int row, const RageTimer &tm, bool bHeld, bool bRelease )	{ StepStrumHopo(col, row, tm, bHeld, bRelease, ButtonType_Hopo); }

	void FadeToFail();
	void CacheAllUsedNoteSkins();
	TapNoteScore GetLastTapNoteScore() const { return m_LastTapNoteScore; }
	void ApplyWaitingTransforms();
	void SetPaused( bool bPaused ) { m_bPaused = bPaused; }

	static float GetMaxStepDistanceSeconds();
	static float GetWindowSeconds( TimingWindow tw );
	const NoteData &GetNoteData() const { return m_NoteData; }
	bool HasVisibleParts() const { return m_pNoteField != NULL; }

	void SetActorWithJudgmentPosition( Actor *pActor ) { m_pActorWithJudgmentPosition = pActor; }
	void SetActorWithComboPosition( Actor *pActor ) { m_pActorWithComboPosition = pActor; }

	void SetSendJudgmentAndComboMessages( bool b ) { m_bSendJudgmentAndComboMessages = b; }

	// Lua
	virtual void PushSelf( lua_State *L );
	
	PlayerState * GetPlayerState() { return this->m_pPlayerState; }

protected:
	void UpdateTapNotesMissedOlderThan( float fMissIfOlderThanThisBeat );
	void UpdateJudgedRows();
	void FlashGhostRow( int iRow );
	void HandleTapRowScore( unsigned row );
	void HandleHoldScore( const TapNote &tn );
	void HandleHoldCheckpoint( int iRow, int iNumHoldsHeldThisRow, int iNumHoldsMissedThisRow, const vector<int> &viColsWithHold );
	void DrawTapJudgments();
	void DrawHoldJudgments();
	void SendComboMessages( int iOldCombo, int iOldMissCombo );
	void PlayKeysound( const TapNote &tn, TapNoteScore score );

	void SetMineJudgment( TapNoteScore tns );
	void SetJudgment( TapNoteScore tns, int iFirstTrack, float fTapNoteOffset );	// -1 if no track as in TNS_Miss
	void SetHoldJudgment( TapNoteScore tns, HoldNoteScore hns, int iTrack );
	void SetCombo( int iCombo, int iMisses );

	void ChangeLife( TapNoteScore tns );
	void ChangeLife( HoldNoteScore hns, TapNoteScore tns );
	void ChangeLifeRecord();

	int GetClosestNoteDirectional( int col, int iStartRow, int iMaxRowsAhead, bool bAllowGraded, bool bForward ) const;
	int GetClosestNote( int col, int iNoteRow, int iMaxRowsAhead, int iMaxRowsBehind, bool bAllowGraded ) const;
	int GetClosestNonEmptyRowDirectional( int iStartRow, int iMaxRowsAhead, bool bAllowGraded, bool bForward ) const;
	int GetClosestNonEmptyRow( int iNoteRow, int iMaxRowsAhead, int iMaxRowsBehind, bool bAllowGraded ) const;

	RString ApplyRandomAttack();

	inline void HideNote( int col, int row )
	{
		NoteData::iterator iter = m_NoteData.FindTapNote( col, row );
		if( iter != m_NoteData.end(col) )
			iter->second.result.bHidden = true;
	}

	bool			m_bLoaded;

	/** @brief The player's present state. */
	PlayerState		*m_pPlayerState;
	/** @brief The player's present stage stats. */
	PlayerStageStats	*m_pPlayerStageStats;
	TimingData      *m_Timing;
	float			m_fNoteFieldHeight;

	bool			m_bPaused;
	bool			m_bDelay;

	NoteData		&m_NoteData;
	NoteField		*m_pNoteField;

	vector<HoldJudgment*>	m_vpHoldJudgment;

	AutoActor		m_sprJudgment;
	AutoActor		m_sprCombo;
	Actor			*m_pActorWithJudgmentPosition;
	Actor			*m_pActorWithComboPosition;

	AttackDisplay		*m_pAttackDisplay;

	TapNoteScore		m_LastTapNoteScore;
	LifeMeter		*m_pLifeMeter;
	CombinedLifeMeter	*m_pCombinedLifeMeter;
	ScoreDisplay		*m_pScoreDisplay;
	ScoreDisplay		*m_pSecondaryScoreDisplay;
	ScoreKeeper		*m_pPrimaryScoreKeeper;
	ScoreKeeper		*m_pSecondaryScoreKeeper;
	Inventory		*m_pInventory;

	int			m_iFirstUncrossedRow;	// used by hold checkpoints logic
	NoteData::all_tracks_iterator *m_pIterNeedsTapJudging;
	NoteData::all_tracks_iterator *m_pIterNeedsHoldJudging;
	NoteData::all_tracks_iterator *m_pIterUncrossedRows;
	NoteData::all_tracks_iterator *m_pIterUnjudgedRows;
	NoteData::all_tracks_iterator *m_pIterUnjudgedMineRows;
	int			m_iLastSeenCombo;
	JudgedRows		*m_pJudgedRows;

	RageSound		m_soundMine;
	RageSound		m_soundAttackLaunch;
	RageSound		m_soundAttackEnding;

	float			m_fActiveRandomAttackStart;

	vector<bool>	m_vbFretIsDown;

	vector<RageSound>	m_vKeysounds;

	ThemeMetric<float>	GRAY_ARROWS_Y_STANDARD;
	ThemeMetric<float>	GRAY_ARROWS_Y_REVERSE;
	ThemeMetric2D<float>	ATTACK_DISPLAY_X;
	ThemeMetric<float>	ATTACK_DISPLAY_Y;
	ThemeMetric<float>	ATTACK_DISPLAY_Y_REVERSE;
	ThemeMetric<float>	HOLD_JUDGMENT_Y_STANDARD;
	ThemeMetric<float>	HOLD_JUDGMENT_Y_REVERSE;
	ThemeMetric<int>	BRIGHT_GHOST_COMBO_THRESHOLD;
	ThemeMetric<bool>	TAP_JUDGMENTS_UNDER_FIELD;
	ThemeMetric<bool>	HOLD_JUDGMENTS_UNDER_FIELD;
	ThemeMetric<bool>	COMBO_UNDER_FIELD;
	ThemeMetric<int>	DRAW_DISTANCE_AFTER_TARGET_PIXELS;
	ThemeMetric<int>	DRAW_DISTANCE_BEFORE_TARGET_PIXELS;

#define NUM_REVERSE 2
#define NUM_CENTERED 2
	TweenState		m_tsJudgment[NUM_REVERSE][NUM_CENTERED];
	TweenState		m_tsCombo[NUM_REVERSE][NUM_CENTERED];

	bool m_bSendJudgmentAndComboMessages;
};

class PlayerPlus
{
	Player *m_pPlayer;
	NoteData m_NoteData;
public:
	PlayerPlus() { m_pPlayer = new Player(m_NoteData); }
	~PlayerPlus() { delete m_pPlayer; }
	void Load( const NoteData &nd ) { m_NoteData = nd; m_pPlayer->Load(); }
	Player *operator->() { return m_pPlayer; }
	const Player *operator->() const { return m_pPlayer; }
	operator Player*() { return m_pPlayer; }
	operator const Player*() const { return m_pPlayer; }
};

#endif

/*
 * (c) 2001-2006 Chris Danford, Steve Checkoway
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
