/** @brief NoteTypes - Types for holding tap notes and scores. */

#ifndef NOTE_TYPES_H
#define NOTE_TYPES_H

#include "GameConstantsAndTypes.h"
#include "PlayerNumber.h"
#include "RageLog.h"

class XNode;

/** @brief The result of hitting (or missing) a tap note. */
struct TapNoteResult
{
	/** @brief Set up the TapNoteResult with default values. */
	TapNoteResult() : tns(TNS_None), fTapNoteOffset(0.f), bHidden(false) { }
	/** @brief The TapNoteScore that was achieved by the player. */
	TapNoteScore	tns;

	/**
	 * @brief Offset, in seconds, for a tap grade.
	 *
	 * Negative numbers mean the note was hit early; positive numbers mean 
	 * it was hit late. These values are only meaningful for graded taps
	 * (tns >= TNS_W5). */
	float		fTapNoteOffset;

	/** @brief If the whole row has been judged, all taps on the row will be set to hidden. */
	bool		bHidden;

	// XML
	XNode* CreateNode() const;
	void LoadFromNode( const XNode* pNode );
};
/** @brief The result of holding (or letting go of) a hold note. */
struct HoldNoteResult
{
	HoldNoteResult() : hns(HNS_None), fLife(1.f), fOverlappedTime(0), iLastHeldRow(0), iCheckpointsHit(0), iCheckpointsMissed(0), bHeld(false), bActive(false) { }
	float GetLastHeldBeat() const;

	HoldNoteScore	hns;

	/**
	 * @brief the current life of the hold.
	 * 
	 * 1.0 means this HoldNote has full life.
	 * 
	 * 0.0 means this HoldNote is dead.
	 * 
	 * When this value hits 0.0 for the first time, m_HoldScore becomes HNS_LetGo.
	 * If the life is > 0.0 when the HoldNote ends, then m_HoldScore becomes HNS_Held. */
	float	fLife;

	/** @brief The number of seconds the hold note has overlapped the current beat.
	 *
	 * This value is 0 if it doesn't overlap. */
	float	fOverlappedTime;

	/** @brief Last index where fLife was greater than 0. If the tap was missed, this
	 * will be the first index of the hold. */
	int		iLastHeldRow;

	/** @brief If checkpoint holds are enabled, the number of checkpoints hit. */
	int		iCheckpointsHit;
	/** @brief If checkpoint holds are enabled, the number of checkpoints missed. */
	int		iCheckpointsMissed;

	/** @brief Was the button held during the last update? */
	bool		bHeld;
	/** @brief Is there life in the hold and does it overlap the current beat? */
	bool		bActive;

	// XML
	XNode* CreateNode() const;
	void LoadFromNode( const XNode* pNode );
};

/** @brief The various properties of a tap note. */
struct TapNote
{
	/** @brief What is the TapNote's core type? */
	enum Type
	{ 
		empty, 		/**< There is no note here. */
		tap,		/**< The player simply steps on this. */
		hold_head,	/**< This is graded like the Tap type, but should be held. */
		hold_tail,	/**< In 2sand3s mode, holds are deleted and hold_tail is added. */
		mine,		/**< In most modes, it is suggested to not step on these mines. */
		lift,		/**< Lift your foot up when it crosses the target area. */
		attack,		/**< Hitting this note causes an attack to take place. */
		autoKeysound,	/**< A special sound is played when this note crosses the target area. */
		fake,		/**< This arrow can't be scored for or against the player. */
 	};
	/** @brief The list of a TapNote's sub types. */
	enum SubType
	{
		hold_head_hold, /**< The start of a traditional hold note. */
		hold_head_roll, /**< The start of a roll note that must be hit repeatedly. */
		//hold_head_mine,
		NUM_SubType,
		SubType_Invalid
	};
	/** @brief The different places a TapNote could come from. */
	enum Source
	{
		original,	/**< This note is part of the original NoteData. */
		addition,	/**< This note is additional note added by a transform. */
	};
	/** @brief The core note type that is about to cross the target area. */
	Type		type;
	/** @brief The sub type of the note. This is only used if the type is hold_head. */
	SubType		subType;
	/** @brief The originating source of the TapNote. */
	Source		source;
	/** @brief The result of hitting or missing the TapNote. */
	TapNoteResult	result;
	/** @brief The Player that is supposed to hit this note. This is mainly for Routine Mode. */
	PlayerNumber	pn;
	/** @brief Can this note be hammered on or pulled off? This is set before gameplay begins. */
	bool		bHopoPossible;

	// used only if Type == attack:
	RString		sAttackModifiers;
	float		fAttackDurationSeconds;

	// Index into Song's vector of keysound files if nonnegative:
	int		iKeysoundIndex;

	// also used for hold_head only:
	int		iDuration;
	HoldNoteResult	HoldResult;

	// XML
	XNode* CreateNode() const;
	void LoadFromNode( const XNode* pNode );

	TapNote(): type(empty), subType(SubType_Invalid), source(original),
		result(), pn(PLAYER_INVALID), bHopoPossible(false), 
		sAttackModifiers(""), fAttackDurationSeconds(0), 
		iKeysoundIndex(-1), iDuration(0), HoldResult() {}
	void Init()
	{
		type = empty;
		subType = SubType_Invalid; 
		source = original; 
		pn = PLAYER_INVALID, 
		bHopoPossible = false;
		fAttackDurationSeconds = 0.f; 
		iKeysoundIndex = -1;
		iDuration = 0;
	}
	TapNote( 
		Type type_,
		SubType subType_,
		Source source_, 
		RString sAttackModifiers_,
		float fAttackDurationSeconds_,
		int iKeysoundIndex_ ):
		type(type_), subType(subType_), source(source_), result(),
		pn(PLAYER_INVALID), bHopoPossible(false),
		sAttackModifiers(sAttackModifiers_),
		fAttackDurationSeconds(fAttackDurationSeconds_),
		iKeysoundIndex(iKeysoundIndex_), iDuration(0), HoldResult()
	{
		if (type_ > TapNote::fake )
		{
			LOG->Trace("Invalid tap note type %d (most likely) due to random vanish issues. Assume it doesn't need judging.", (int)type_ );
			type = TapNote::empty;
		}
	}

	/**
	 * @brief Determine if the two TapNotes are equal to each other.
	 * @param other the other TapNote we're checking.
	 * @return true if the two TapNotes are equal, or false otherwise. */
	bool operator==( const TapNote &other ) const
	{
#define COMPARE(x)	if(x!=other.x) return false
		COMPARE(type);
		COMPARE(subType);
		COMPARE(source);
		COMPARE(sAttackModifiers);
		COMPARE(fAttackDurationSeconds);
		COMPARE(iKeysoundIndex);
		COMPARE(iDuration);
		COMPARE(pn);
#undef COMPARE
		return true;
	}
	/**
	 * @brief Determine if the two TapNotes are not equal to each other.
	 * @param other the other TapNote we're checking.
	 * @return true if the two TapNotes are not equal, or false otherwise. */
	bool operator!=( const TapNote &other ) const { return !operator==( other ); }
};

extern TapNote TAP_EMPTY;			// '0'
extern TapNote TAP_ORIGINAL_TAP;		// '1'
extern TapNote TAP_ORIGINAL_HOLD_HEAD;		// '2'
extern TapNote TAP_ORIGINAL_ROLL_HEAD;		// '4'
extern TapNote TAP_ORIGINAL_MINE;		// 'M'
extern TapNote TAP_ORIGINAL_LIFT;		// 'L'
extern TapNote TAP_ORIGINAL_ATTACK;		// 'A'
extern TapNote TAP_ORIGINAL_AUTO_KEYSOUND;	// 'K'
extern TapNote TAP_ORIGINAL_FAKE;		// 'F'
//extern TapNote TAP_ORIGINAL_MINE_HEAD;	// 'N' (tentative, we'll see when iDance gets ripped.)
extern TapNote TAP_ADDITION_TAP;
extern TapNote TAP_ADDITION_MINE;

/**
 * @brief Retrieve the string representing the TapNote Type.
 *
 * TODO: Find a way to standardize this with the other enum string calls.
 * @param tn the TapNote's type.
 * @return the intended string. */
inline const RString TapNoteTypeToString( TapNote::Type tn )
{
	switch( tn )
	{
		case TapNote::empty:
			return RString("empty");
		case TapNote::tap:
			return RString("tap");
		case TapNote::hold_head:
			return RString("hold_head");
		case TapNote::hold_tail:
			return RString("hold_tail");
		case TapNote::mine:	
			return RString("mine");
		case TapNote::lift:	
			return RString("lift");
		case TapNote::attack:
			return RString("attack");
		case TapNote::autoKeysound:
			return RString("autoKeysound");
		case TapNote::fake:
			return RString("fake");
		default:
			return RString("");
	}
}

/**
 * @brief The number of tracks allowed.
 *
 * TODO: Don't have a hard-coded track limit.
 */
const int MAX_NOTE_TRACKS = 16;

/**
 * @brief The number of rows per beat.
 *
 * This is a divisor for our "fixed-point" time/beat representation. It must be
 * evenly divisible by 2, 3, and 4, to exactly represent 8th, 12th and 16th notes.
 *
 * XXX: Some other forks try to keep this flexible by putting this in the simfile.
 * Is this a recommended course of action? -Wolfman2000 */
const int ROWS_PER_BEAT	= 48;

/** @brief The max number of rows allowed for a Steps pattern. */
const int MAX_NOTE_ROW = (1<<30);

/** @brief The list of quantized note types allowed at present. */
enum NoteType 
{ 
	NOTE_TYPE_4TH,	/**< quarter note */
	NOTE_TYPE_8TH,	/**< eighth note */
	NOTE_TYPE_12TH,	/**< quarter note triplet */
	NOTE_TYPE_16TH,	/**< sixteenth note */
	NOTE_TYPE_24TH,	/**< eighth note triplet */
	NOTE_TYPE_32ND,	/**< thirty-second note */
	NOTE_TYPE_48TH, /**< sixteenth note triplet */
	NOTE_TYPE_64TH,	/**< sixty-fourth note */
	NOTE_TYPE_192ND,/**< sixty-fourth note triplet */
	NUM_NoteType,
	NoteType_Invalid
};
const RString& NoteTypeToString( NoteType nt );
const RString& NoteTypeToLocalizedString( NoteType nt );
LuaDeclareType( NoteType );
float NoteTypeToBeat( NoteType nt );
int NoteTypeToRow( NoteType nt );
NoteType GetNoteType( int row );
NoteType BeatToNoteType( float fBeat );
bool IsNoteOfType( int row, NoteType t );

/* This is more accurate: by computing the integer and fractional parts separately, we
 * can avoid storing very large numbers in a float and possibly losing precision.  It's
 * slower; use this once less stuff uses BeatToNoteRow. */
/*
inline int   BeatToNoteRow( float fBeatNum )
{
	float fraction = fBeatNum - truncf(fBeatNum);
	int integer = int(fBeatNum) * ROWS_PER_BEAT;
	return integer + lrintf(fraction * ROWS_PER_BEAT);
}
*/
/**
 * @brief Convert the beat into a note row.
 * @param fBeatNum the beat to convert.
 * @return the note row. */
inline int   BeatToNoteRow( float fBeatNum )		{ return lrintf( fBeatNum * ROWS_PER_BEAT ); }	// round
/**
 * @brief Convert the beat into a note row without rounding.
 * @param fBeatNum the beat to convert.
 * @return the note row. */
inline int   BeatToNoteRowNotRounded( float fBeatNum )	{ return (int)( fBeatNum * ROWS_PER_BEAT ); }
/**
 * @brief Convert the note row to a beat.
 * @param iRow the row to convert.
 * @return the beat. */
inline float NoteRowToBeat( int iRow )			{ return iRow / (float)ROWS_PER_BEAT; }

// These functions can be useful for function templates,
// where both rows and beats can be specified.

/**
 * @brief Convert the note row to note row (returns itself).
 * @param row the row to convert.
 */
static inline int ToNoteRow(int row)    { return row; }

/**
 * @brief Convert the beat to note row.
 * @param beat the beat to convert.
 */
static inline int ToNoteRow(float beat) { return BeatToNoteRow(beat); }

/**
 * @brief Convert the note row to beat.
 * @param row the row to convert.
 */
static inline float ToBeat(int row)    { return NoteRowToBeat(row); }

/**
 * @brief Convert the beat row to beat (return itself).
 * @param beat the beat to convert.
 */
static inline float ToBeat(float beat) { return beat; }

/**
 * @brief Scales the position.
 * @param T start - the starting row of the scaling region
 * @param T length - the length of the scaling region
 * @param T newLength - the new length of the scaling region
 * @param T position - the position to scale
 * @return T the scaled position
 */
template<typename T>
inline T ScalePosition( T start, T length, T newLength, T position )
{
	if( position < start )
		return position;
	if( position >= start + length )
		return position - length + newLength;
	return start + (position - start) * newLength / length;
}

#endif

/**
 * @file
 * @author Chris Danford, Glenn Maynard (c) 2001-2004
 * @section LICENSE
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
