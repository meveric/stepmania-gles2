#include "global.h"
#include "NoteDataUtil.h"
#include "NoteData.h"
#include "RageUtil.h"
#include "RageLog.h"
#include "PlayerOptions.h"
#include "Song.h"
#include "Style.h"
#include "GameState.h"
#include "RadarValues.h"
#include "Foreach.h"
#include <utility>

// TODO: Remove these constants that aren't time signature-aware
static const int BEATS_PER_MEASURE = 4;
static const int ROWS_PER_MEASURE = ROWS_PER_BEAT * BEATS_PER_MEASURE;

NoteType NoteDataUtil::GetSmallestNoteTypeForMeasure( const NoteData &nd, int iMeasureIndex )
{
	const int iMeasureStartIndex = iMeasureIndex * ROWS_PER_MEASURE;
	const int iMeasureEndIndex = (iMeasureIndex+1) * ROWS_PER_MEASURE;

	return NoteDataUtil::GetSmallestNoteTypeInRange( nd, iMeasureStartIndex, iMeasureEndIndex );
}

NoteType NoteDataUtil::GetSmallestNoteTypeInRange( const NoteData &n, int iStartIndex, int iEndIndex )
{
	// probe to find the smallest note type
	FOREACH_ENUM(NoteType, nt)
	{
		float fBeatSpacing = NoteTypeToBeat( nt );
		int iRowSpacing = lrintf( fBeatSpacing * ROWS_PER_BEAT );

		bool bFoundSmallerNote = false;
		// for each index in this measure
		FOREACH_NONEMPTY_ROW_ALL_TRACKS_RANGE( n, i, iStartIndex, iEndIndex )
		{
			if( i % iRowSpacing == 0 )
				continue;	// skip
			
			if( !n.IsRowEmpty(i) )
			{
				bFoundSmallerNote = true;
				break;
			}
		}

		if( bFoundSmallerNote )
			continue;	// searching the next NoteType
		else
			return nt;	// stop searching. We found the smallest NoteType
	}
	return NoteType_Invalid;	// well-formed notes created in the editor should never get here
}

static void LoadFromSMNoteDataStringWithPlayer( NoteData& out, const RString &sSMNoteData, int start,
						int len, PlayerNumber pn, int iNumTracks )
{
	/* Don't allocate memory for the entire string, nor per measure. Instead, use the in-place
	 * partial string split twice. By maintaining begin and end pointers to each measure line
	 * we can perform this without copying the string at all. */
	int size = -1;
	const int end = start + len;
	vector<pair<const char *, const char *> > aMeasureLines;
	for( unsigned m = 0; true; ++m )
	{
		/* XXX Ignoring empty seems wrong for measures. It means that ",,," is treated as
		 * "," where I would expect most people would want 2 empty measures. ",\n,\n,"
		 * would do as I would expect. */
		split( sSMNoteData, ",", start, size, end, true ); // Ignore empty is important.
		if( start == end )
			break;

		// Partial string split.
		int measureLineStart = start, measureLineSize = -1;
		const int measureEnd = start + size;

		aMeasureLines.clear();
		while( true )
		{
			// Ignore empty is clearly important here.
			split( sSMNoteData, "\n", measureLineStart, measureLineSize, measureEnd, true );
			if( measureLineStart == measureEnd )
				break;
			//RString &line = sSMNoteData.substr( measureLineStart, measureLineSize );
			const char *beginLine = sSMNoteData.data() + measureLineStart;
			const char *endLine = beginLine + measureLineSize;

			while( beginLine < endLine && strchr("\r\n\t ", *beginLine) )
				++beginLine;
			while( endLine > beginLine && strchr("\r\n\t ", *(endLine - 1)) )
				--endLine;
			if( beginLine < endLine ) // nonempty
				aMeasureLines.push_back( pair<const char *, const char *>(beginLine, endLine) );
		}

		for( unsigned l=0; l<aMeasureLines.size(); l++ )
		{
			const char *p = aMeasureLines[l].first;
			const char *const beginLine = p;
			const char *const endLine = aMeasureLines[l].second;

			const float fPercentIntoMeasure = l/(float)aMeasureLines.size();
			const float fBeat = (m + fPercentIntoMeasure) * BEATS_PER_MEASURE;
			const int iIndex = BeatToNoteRow( fBeat );

			int iTrack = 0;
			while( iTrack < iNumTracks && p < endLine )
			{
				TapNote tn;
				char ch = *p;

				switch( ch )
				{
				case '0': tn = TAP_EMPTY;				break;
				case '1': tn = TAP_ORIGINAL_TAP;			break;
				case '2':
				case '4':
				// case 'N': // minefield
					tn = ch == '2' ? TAP_ORIGINAL_HOLD_HEAD : TAP_ORIGINAL_ROLL_HEAD;
					/*
					// upcoming code for minefields -aj
					switch(ch)
					{
					case '2': tn = TAP_ORIGINAL_HOLD_HEAD; break;
					case '4': tn = TAP_ORIGINAL_ROLL_HEAD; break;
					case 'N': tn = TAP_ORIGINAL_MINE_HEAD; break;
					}
					*/

					/* Set the hold note to have infinite length. We'll clamp
					 * it when we hit the tail. */
					tn.iDuration = MAX_NOTE_ROW;
					break;
				case '3':
				{
					// This is the end of a hold. Search for the beginning.
					int iHeadRow;
					if( !out.IsHoldNoteAtRow( iTrack, iIndex, &iHeadRow ) )
					{
						int n = intptr_t(endLine) - intptr_t(beginLine);
						LOG->Warn( "Unmatched 3 in \"%.*s\"", n, beginLine );
					}
					else
					{
						out.FindTapNote( iTrack, iHeadRow )->second.iDuration = iIndex - iHeadRow;
					}

					// This won't write tn, but keep parsing normally anyway.
					break;
				}
				//				case 'm':
				// Don't be loose with the definition.  Use only 'M' since
				// that's what we've been writing to disk.  -Chris
				case 'M': tn = TAP_ORIGINAL_MINE;			break;
				// case 'A': tn = TAP_ORIGINAL_ATTACK;			break;
				case 'K': tn = TAP_ORIGINAL_AUTO_KEYSOUND;		break;
				case 'L': tn = TAP_ORIGINAL_LIFT;			break;
				case 'F': tn = TAP_ORIGINAL_FAKE;			break;
				// case 'I': tn = TAP_ORIGINAL_ITEM;			break;
				default: 
					/* Invalid data. We don't want to assert, since there might
					 * simply be invalid data in an .SM, and we don't want to die
					 * due to invalid data. We should probably check for this when
					 * we load SM data for the first time ... */
					// FAIL_M("Invalid data in SM");
					tn = TAP_EMPTY;
					break;
				}

				p++;
				// We won't scan past the end of the line so these are safe to do.
#if 0
				// look for optional attack info (e.g. "{tipsy,50% drunk:15.2}")
				if( *p == '{' )
				{
					p++;

					char szModifiers[256] = "";
					float fDurationSeconds = 0;
					if( sscanf( p, "%255[^:]:%f}", szModifiers, &fDurationSeconds ) == 2 )	// not fatal if this fails due to malformed data
					{
						tn.type = TapNote::attack;
						tn.sAttackModifiers = szModifiers;
		 				tn.fAttackDurationSeconds = fDurationSeconds;
					}

					// skip past the '}'
					while( p < endLine )
					{
						if( *(p++) == '}' )
							break;
					}
				}
#endif

				// look for optional keysound index (e.g. "[123]")
				if( *p == '[' )
				{
					p++;
					int iKeysoundIndex = 0;
					if( 1 == sscanf( p, "%d]", &iKeysoundIndex ) )	// not fatal if this fails due to malformed data
		 				tn.iKeysoundIndex = iKeysoundIndex;

					// skip past the ']'
					while( p < endLine )
					{
						if( *(p++) == ']' )
							break;
					}
				}

#if 0
				// look for optional item name (e.g. "<potion>"),
				// where the name in the <> is a Lua function defined elsewhere
				// (Data/ItemTypes.lua, perhaps?) -aj
				if( *p == '<' )
				{
					p++;

					// skip past the '>'
					while( p < endLine )
					{
						if( *(p++) == '>' )
							break;
					}
				}
#endif

				/* Optimization: if we pass TAP_EMPTY, NoteData will do a search
				 * to remove anything in this position.  We know that there's nothing
				 * there, so avoid the search. */
				if( tn.type != TapNote::empty && ch != '3' )
				{
					tn.pn = pn;
					out.SetTapNote( iTrack, iIndex, tn );
				}

				iTrack++;
			}
		}
	}

	// Make sure we don't have any hold notes that didn't find a tail.
	for( int t=0; t<out.GetNumTracks(); t++ )
	{
		NoteData::iterator begin = out.begin( t );
		NoteData::iterator lEnd = out.end( t );
		while( begin != lEnd )
		{
			NoteData::iterator next = Increment( begin );
			const TapNote &tn = begin->second;
			if( tn.type == TapNote::hold_head && tn.iDuration == MAX_NOTE_ROW )
			{
				int iRow = begin->first;
				LOG->UserLog( "", "", "While loading .sm/.ssc note data, there was an unmatched 2 at beat %f", NoteRowToBeat(iRow) );
				out.RemoveTapNote( t, begin );
			}

			begin = next;
		}
	}
}

void NoteDataUtil::LoadFromSMNoteDataString( NoteData &out, const RString &sSMNoteData_, bool bComposite )
{
	// Load note data
	RString sSMNoteData;
	RString::size_type iIndexCommentStart = 0;
	RString::size_type iIndexCommentEnd = 0;
	RString::size_type origSize = sSMNoteData_.size();
	const char *p = sSMNoteData_.data();

	sSMNoteData.reserve( origSize );
	while( (iIndexCommentStart = sSMNoteData_.find("//", iIndexCommentEnd)) != RString::npos )
	{
		sSMNoteData.append( p, iIndexCommentStart - iIndexCommentEnd );
		p += iIndexCommentStart - iIndexCommentEnd;
		iIndexCommentEnd = sSMNoteData_.find( "\n", iIndexCommentStart );
		iIndexCommentEnd = (iIndexCommentEnd == RString::npos ? origSize : iIndexCommentEnd+1);
		p += iIndexCommentEnd - iIndexCommentStart;
	}
	sSMNoteData.append( p, origSize - iIndexCommentEnd );

	// Clear notes, but keep the same number of tracks.
	int iNumTracks = out.GetNumTracks();
	out.Init();
	out.SetNumTracks( iNumTracks );

	if( !bComposite )
	{
		LoadFromSMNoteDataStringWithPlayer( out, sSMNoteData, 0, sSMNoteData.size(),
						    PLAYER_INVALID, iNumTracks );
		return;
	}

	int start = 0, size = -1;

	vector<NoteData> vParts;
	FOREACH_PlayerNumber( pn )
	{
		// Split in place.
		split( sSMNoteData, "&", start, size, false );
		if( unsigned(start) == sSMNoteData.size() )
			break;
		vParts.push_back( NoteData() );
		NoteData &nd = vParts.back();

		nd.SetNumTracks( iNumTracks );
		LoadFromSMNoteDataStringWithPlayer( nd, sSMNoteData, start, size, pn, iNumTracks );
	}
	CombineCompositeNoteData( out, vParts );
}

void NoteDataUtil::InsertHoldTails( NoteData &inout )
{
	for( int t=0; t < inout.GetNumTracks(); t++ )
	{
		NoteData::iterator begin = inout.begin(t), end = inout.end(t);

		for( ; begin != end; ++begin )
		{
			int iRow = begin->first;
			const TapNote &tn = begin->second;
			if( tn.type != TapNote::hold_head )
				continue;

			TapNote tail = tn;
			tail.type = TapNote::hold_tail;

			/* If iDuration is 0, we'd end up overwriting the head with the tail
			 * (and invalidating our iterator). Empty hold notes aren't valid. */
			ASSERT( tn.iDuration != 0 );

			inout.SetTapNote( t, iRow + tn.iDuration, tail );
		}
	}
}

void NoteDataUtil::GetSMNoteDataString( const NoteData &in, RString &sRet )
{
	// Get note data
	vector<NoteData> parts;
	float fLastBeat = -1.0f;

	SplitCompositeNoteData( in, parts );

	FOREACH( NoteData, parts, nd )
	{
		InsertHoldTails( *nd );
		fLastBeat = max( fLastBeat, nd->GetLastBeat() );
	}

	int iLastMeasure = int( fLastBeat/BEATS_PER_MEASURE );

	sRet = "";
	FOREACH( NoteData, parts, nd )
	{
		if( nd != parts.begin() )
			sRet.append( "&\n" );
		for( int m = 0; m <= iLastMeasure; ++m ) // foreach measure
		{
			if( m )
				sRet.append( 1, ',' );
			sRet += ssprintf("  // measure %d\n", m);

			NoteType nt = GetSmallestNoteTypeForMeasure( *nd, m );
			int iRowSpacing;
			if( nt == NoteType_Invalid )
				iRowSpacing = 1;
			else
				iRowSpacing = lrintf( NoteTypeToBeat(nt) * ROWS_PER_BEAT );
			// (verify first)
			// iRowSpacing = BeatToNoteRow( NoteTypeToBeat(nt) );

			const int iMeasureStartRow = m * ROWS_PER_MEASURE;
			const int iMeasureLastRow = (m+1) * ROWS_PER_MEASURE - 1;

			for( int r=iMeasureStartRow; r<=iMeasureLastRow; r+=iRowSpacing )
			{
				for( int t = 0; t < nd->GetNumTracks(); ++t )
				{
					const TapNote &tn = nd->GetTapNote(t, r);
					char c;
					switch( tn.type )
					{
					case TapNote::empty:			c = '0'; break;
					case TapNote::tap:				c = '1'; break;
					case TapNote::hold_head:
						switch( tn.subType )
						{
						case TapNote::hold_head_hold:	c = '2'; break;
						case TapNote::hold_head_roll:	c = '4'; break;
						//case TapNote::hold_head_mine:	c = 'N'; break;
						default:
							FAIL_M(ssprintf("Invalid tap note subtype: %i", tn.subType));
						}
						break;
					case TapNote::hold_tail:		c = '3'; break;
					case TapNote::mine:			c = 'M'; break;
					case TapNote::attack:			c = 'A'; break;
					case TapNote::autoKeysound:	c = 'K'; break;
					case TapNote::lift:			c = 'L'; break;
					case TapNote::fake:			c = 'F'; break;
					default: 
						c = '\0';
						FAIL_M(ssprintf("Invalid tap note type: %i", tn.type));
					}
					sRet.append( 1, c );

					if( tn.type == TapNote::attack )
					{
						sRet.append( ssprintf("{%s:%.2f}", tn.sAttackModifiers.c_str(),
								      tn.fAttackDurationSeconds) );
					}
					// hey maybe if we have TapNote::item we can do things here.
					if( tn.iKeysoundIndex >= 0 )
						sRet.append( ssprintf("[%d]",tn.iKeysoundIndex) );
				}

				sRet.append( 1, '\n' );
			}
		}
	}
}

void NoteDataUtil::SplitCompositeNoteData( const NoteData &in, vector<NoteData> &out )
{
	if( !in.IsComposite() )
	{
		out.push_back( in );
		return;
	}

	FOREACH_PlayerNumber( pn )
	{
		out.push_back( NoteData() );
		out.back().SetNumTracks( in.GetNumTracks() );
	}

	for( int t = 0; t < in.GetNumTracks(); ++t )
	{
		for( NoteData::const_iterator iter = in.begin(t); iter != in.end(t); ++iter )
		{
			int row = iter->first;
			TapNote tn = iter->second;
			/*
			 XXX: This code is (hopefully) a temporary hack to make sure that
			 routine charts don't have any notes without players assigned to them.
			 I suspect this is due to a related bug that these problems were
			 occuring to begin with, but at this time, I am unsure how to deal with it.
			 Hopefully this hack can be removed soon. -- Jason "Wolfman2000" Felds
			 */
			const Style *curStyle = GAMESTATE->GetCurrentStyle();
			if( (curStyle == NULL || curStyle->m_StyleType == StyleType_TwoPlayersSharedSides )
				&& int( tn.pn ) > NUM_PlayerNumber )
			{
				tn.pn = PLAYER_1;
			}
			unsigned index = int( tn.pn );

			ASSERT_M( index < NUM_PlayerNumber, ssprintf("We have a note not assigned to a player. The note in question is on beat %f, column %i.", NoteRowToBeat(row), t + 1) );
			tn.pn = PLAYER_INVALID;
			out[index].SetTapNote( t, row, tn );
		}
	}
}

void NoteDataUtil::CombineCompositeNoteData( NoteData &out, const vector<NoteData> &in )
{
	FOREACH_CONST( NoteData, in, nd )
	{
		const int iMaxTracks = min( out.GetNumTracks(), nd->GetNumTracks() );

		for( int track = 0; track < iMaxTracks; ++track )
		{
			for( NoteData::const_iterator i = nd->begin(track); i != nd->end(track); ++i )
			{
				int row = i->first;
				if( out.IsHoldNoteAtRow(track, i->first) )
					continue;
				if( i->second.type == TapNote::hold_head )
					out.AddHoldNote( track, row, row + i->second.iDuration, i->second );
				else
					out.SetTapNote( track, row, i->second );
			}
		}
	}
}


void NoteDataUtil::LoadTransformedSlidingWindow( const NoteData &in, NoteData &out, int iNewNumTracks )
{
	// reset all notes
	out.Init();
	
	if( in.GetNumTracks() > iNewNumTracks )
	{
		// Use a different algorithm for reducing tracks.
		LoadOverlapped( in, out, iNewNumTracks );
		return;
	}

	out.SetNumTracks( iNewNumTracks );

	if( in.GetNumTracks() == 0 )
		return;	// nothing to do and don't AV below

	int iCurTrackOffset = 0;
	int iTrackOffsetMin = 0;
	int iTrackOffsetMax = abs( iNewNumTracks - in.GetNumTracks() );
	int bOffsetIncreasing = true;

	int iLastMeasure = 0;
	int iMeasuresSinceChange = 0;
	FOREACH_NONEMPTY_ROW_ALL_TRACKS( in, r )
	{
		const int iMeasure = r / ROWS_PER_MEASURE;
		if( iMeasure != iLastMeasure )
			++iMeasuresSinceChange;

		if( iMeasure != iLastMeasure && iMeasuresSinceChange >= 4 ) // adjust sliding window every 4 measures at most
		{
			// See if there is a hold crossing the beginning of this measure
			bool bHoldCrossesThisMeasure = false;

			for( int t=0; t<in.GetNumTracks(); t++ )
			{
				if( in.IsHoldNoteAtRow( t, r-1 ) &&
				    in.IsHoldNoteAtRow( t, r ) )
				{
					bHoldCrossesThisMeasure = true;
					break;
				}
			}

			// adjust offset
			if( !bHoldCrossesThisMeasure )
			{
				iMeasuresSinceChange = 0;
				iCurTrackOffset += bOffsetIncreasing ? 1 : -1;
				if( iCurTrackOffset == iTrackOffsetMin  ||  iCurTrackOffset == iTrackOffsetMax )
					bOffsetIncreasing ^= true;
				CLAMP( iCurTrackOffset, iTrackOffsetMin, iTrackOffsetMax );
			}
		}

		iLastMeasure = iMeasure;

		// copy notes in this measure
		for( int t=0; t<in.GetNumTracks(); t++ )
		{
			int iOldTrack = t;
			int iNewTrack = (iOldTrack + iCurTrackOffset) % iNewNumTracks;
			const TapNote &tn = in.GetTapNote( iOldTrack, r );
			out.SetTapNote( iNewTrack, r, tn );
		}
	}
}

void PlaceAutoKeysound( NoteData &out, int row, TapNote akTap )
{
	int iEmptyTrack = -1;
	int iEmptyRow = row;
	int iNewNumTracks = out.GetNumTracks();
	bool bFoundEmptyTrack = false;
	int iRowsToLook[3] = {0, -1, 1};
	
	for( int j = 0; j < 3; j ++ )
	{
		int r = iRowsToLook[j] + row;
		if( r < 0 )
			continue;
		for( int i = 0; i < iNewNumTracks; ++i )
		{
			if ( out.GetTapNote(i, r) == TAP_EMPTY && !out.IsHoldNoteAtRow(i, r) )
			{
				iEmptyTrack = i;
				iEmptyRow = r;
				bFoundEmptyTrack = true;
				break;
			}
		}
		if( bFoundEmptyTrack )
			break;
	}
	
	if( iEmptyTrack != -1 )
	{
		akTap.type = TapNote::autoKeysound;
		out.SetTapNote( iEmptyTrack, iEmptyRow, akTap );
	}
}

void NoteDataUtil::LoadOverlapped( const NoteData &in, NoteData &out, int iNewNumTracks )
{
	out.SetNumTracks( iNewNumTracks );

	/* Keep track of the last source track that put a tap into each destination track,
	 * and the row of that tap. Then, if two rows are trying to put taps into the
	 * same row within the shift threshold, shift the newcomer source row. */
	int LastSourceTrack[MAX_NOTE_TRACKS];
	int LastSourceRow[MAX_NOTE_TRACKS];
	int DestRow[MAX_NOTE_TRACKS];

	for( int tr = 0; tr < MAX_NOTE_TRACKS; ++tr )
	{
		LastSourceTrack[tr] = -1;
		LastSourceRow[tr] = -MAX_NOTE_ROW;
		DestRow[tr] = tr;
		wrap( DestRow[tr], iNewNumTracks );
	}

	const int ShiftThreshold = BeatToNoteRow(1);

	FOREACH_NONEMPTY_ROW_ALL_TRACKS( in, row )
	{
		for( int iTrackFrom = 0; iTrackFrom < in.GetNumTracks(); ++iTrackFrom )
		{
			const TapNote &tnFrom = in.GetTapNote( iTrackFrom, row );
			if( tnFrom.type == TapNote::empty || tnFrom.type == TapNote::autoKeysound )
				continue;

			// If this is a hold note, find the end.
			int iEndIndex = row;
			if( tnFrom.type == TapNote::hold_head )
				iEndIndex = row + tnFrom.iDuration;

			int &iTrackTo = DestRow[iTrackFrom];
			if( LastSourceTrack[iTrackTo] != iTrackFrom )
			{
				if( iEndIndex - LastSourceRow[iTrackTo] < ShiftThreshold )
				{
					/* This destination track is in use by a different source
					 * track. Use the least-recently-used track. */
					for( int DestTrack = 0; DestTrack < iNewNumTracks; ++DestTrack )
						if( LastSourceRow[DestTrack] < LastSourceRow[iTrackTo] )
							iTrackTo = DestTrack;
				}

				// If it's still in use, then we just don't have an available track.
				if( iEndIndex - LastSourceRow[iTrackTo] < ShiftThreshold )
				{
					// If it has a keysound, put it in autokeysound track.
					if( tnFrom.iKeysoundIndex >= 0 )
					{
						TapNote akTap = tnFrom;
						PlaceAutoKeysound( out, row, akTap );
					}
					continue;
				}
			}

			LastSourceTrack[iTrackTo] = iTrackFrom;
			LastSourceRow[iTrackTo] = iEndIndex;
			out.SetTapNote( iTrackTo, row, tnFrom );
			if( tnFrom.type == TapNote::hold_head )
			{
				const TapNote &tnTail = in.GetTapNote( iTrackFrom, iEndIndex );
				out.SetTapNote( iTrackTo, iEndIndex, tnTail );
			}
		}
		
		// find empty track for autokeysounds in 2 next rows, so you can hear most autokeysounds
		for( int iTrackFrom = 0; iTrackFrom < in.GetNumTracks(); ++iTrackFrom )
		{
			const TapNote &tnFrom = in.GetTapNote( iTrackFrom, row );
			if( tnFrom.type != TapNote::autoKeysound )
				continue;
			
			PlaceAutoKeysound( out, row, tnFrom );
		}
	}
}

int FindLongestOverlappingHoldNoteForAnyTrack( const NoteData &in, int iRow )
{
	int iMaxTailRow = -1;
	for( int t=0; t<in.GetNumTracks(); t++ )
	{
		const TapNote &tn = in.GetTapNote( t, iRow );
		if( tn.type == TapNote::hold_head )
			iMaxTailRow = max( iMaxTailRow, iRow + tn.iDuration );
	}

	return iMaxTailRow;
}

// For every row in "in" with a tap or hold on any track, enable the specified tracks in "out".
void LightTransformHelper( const NoteData &in, NoteData &out, const vector<int> &aiTracks )
{
	for( unsigned i = 0; i < aiTracks.size(); ++i )
		ASSERT_M( aiTracks[i] < out.GetNumTracks(), ssprintf("%i, %i", aiTracks[i], out.GetNumTracks()) );

	FOREACH_NONEMPTY_ROW_ALL_TRACKS( in, r )
	{
		/* If any row starts a hold note, find the end of the hold note, and keep searching
		 * until we've extended to the end of the latest overlapping hold note. */
		int iHoldStart = r;
		int iHoldEnd = -1;
		while(1)
		{
			int iMaxTailRow = FindLongestOverlappingHoldNoteForAnyTrack( in, r );
			if( iMaxTailRow == -1 )
				break;
			iHoldEnd = iMaxTailRow;
			r = iMaxTailRow;
		}

		if( iHoldEnd != -1 )
		{
			// If we found a hold note, add it to all tracks.
			for( unsigned i = 0; i < aiTracks.size(); ++i )
			{
				int t = aiTracks[i];
				out.AddHoldNote( t, iHoldStart, iHoldEnd, TAP_ORIGINAL_HOLD_HEAD );
			}
			continue;
		}

		if( in.IsRowEmpty(r) )
			continue;

		// Enable every track in the output.
		for( unsigned i = 0; i < aiTracks.size(); ++i )
		{
			int t = aiTracks[i];
			out.SetTapNote( t, r, TAP_ORIGINAL_TAP );
		}
	}
}

// For every track enabled in "in", enable all tracks in "out".
void NoteDataUtil::LoadTransformedLights( const NoteData &in, NoteData &out, int iNewNumTracks )
{
	// reset all notes
	out.Init();

	out.SetNumTracks( iNewNumTracks );

	vector<int> aiTracks;
	for( int i = 0; i < out.GetNumTracks(); ++i )
		aiTracks.push_back( i );

	LightTransformHelper( in, out, aiTracks );
}

// This transform is specific to StepsType_lights_cabinet.
#include "LightsManager.h" // for LIGHT_*
void NoteDataUtil::LoadTransformedLightsFromTwo( const NoteData &marquee, const NoteData &bass, NoteData &out )
{
	ASSERT( marquee.GetNumTracks() >= 4 );
	ASSERT( bass.GetNumTracks() >= 1 );

	/* For each track in "marquee", enable a track in the marquee lights.
	 * This will reinit out. */
	{
		NoteData transformed_marquee;
		transformed_marquee.CopyAll( marquee );
		Wide( transformed_marquee );

		const int iOriginalTrackToTakeFrom[NUM_CabinetLight] = { 0, 1, 2, 3, -1, -1 };
		out.LoadTransformed( transformed_marquee, NUM_CabinetLight, iOriginalTrackToTakeFrom );
	}

	// For each track in "bass", enable the bass lights.
	{
		vector<int> aiTracks;
		aiTracks.push_back( LIGHT_BASS_LEFT );
		aiTracks.push_back( LIGHT_BASS_RIGHT );
		LightTransformHelper( bass, out, aiTracks );
	}

	// Delete all mines.
	NoteDataUtil::RemoveMines( out );
}

RadarStats CalculateRadarStatsFast( const NoteData &in, RadarStats &out )
{
	out.taps = 0;
	out.jumps = 0;
	out.hands = 0;
	out.quads = 0;
	map<int, int> simultaneousMap;
	map<int, int> simultaneousMapNoHold;
	map<int, int> simultaneousMapTapHoldHead;
	map<int, int>::iterator itr;
	for( int t=0; t<in.GetNumTracks(); t++ )
	{
		FOREACH_NONEMPTY_ROW_IN_TRACK_RANGE( in, t, r, 0, MAX_NOTE_ROW )
		{
			/* This function deals strictly with taps, jumps, hands, and quads.
			 * As such, all rows in here have to be judgable. */
			if (!GAMESTATE->GetProcessedTimingData()->IsJudgableAtRow(r))
				continue;
			
			const TapNote &tn = in.GetTapNote(t, r);
			switch( tn.type )
			{
				case TapNote::mine:
				case TapNote::empty:
				case TapNote::fake:
				case TapNote::autoKeysound:
					continue;	// skip these types - they don't count
				default: break;
			}

			if( (itr = simultaneousMap.find(r)) == simultaneousMap.end() )
				simultaneousMap[r] = 1;
			else
				itr->second++;

			if( (itr = simultaneousMapNoHold.find(r)) == simultaneousMapNoHold.end() )
				simultaneousMapNoHold[r] = 1;
			else
				itr->second++;
			
			if( tn.type == TapNote::tap || tn.type == TapNote::lift || tn.type == TapNote::hold_head )
			{
				simultaneousMapTapHoldHead[r] = 1;
			}

			if( tn.type == TapNote::hold_head )
			{
				int searchStartRow = r + 1;
				int searchEndRow   = r + tn.iDuration;
				FOREACH_NONEMPTY_ROW_ALL_TRACKS_RANGE( in, rr, searchStartRow, searchEndRow )
				{
					switch( in.GetTapNote(t, rr).type )
					{
						case TapNote::mine:
						case TapNote::empty:
						case TapNote::fake:
							continue;	// skip these types - they don't count
						default: break;
					}
					if( (itr = simultaneousMap.find(rr)) == simultaneousMap.end() )
						simultaneousMap[rr] = 1;
					else
						itr->second++;
				}
			}
		}
	}
	for( itr = simultaneousMap.begin(); itr != simultaneousMap.end(); itr ++ )
	{
		if( itr->second >= 3 )
		{
			out.hands ++;
			if( itr->second >= 4 )
			{
				out.quads ++;
			}
		}
	}
	for( itr = simultaneousMapNoHold.begin(); itr != simultaneousMapNoHold.end(); itr ++ )
	{
		if( itr->second >= 2 )
		{
			out.jumps ++;
		}
	}
	out.taps = simultaneousMapTapHoldHead.size();
	return out;
}

void NoteDataUtil::CalculateRadarValues( const NoteData &in, float fSongSeconds, RadarValues& out )
{
	RadarStats stats;
	CalculateRadarStatsFast( in, stats );
	
	// The for loop and the assert are used to ensure that all fields of 
	// RadarValue get set in here.
	FOREACH_ENUM( RadarCategory, rc )
	{
		switch( rc )
		{
		case RadarCategory_Stream:			out[rc] = GetStreamRadarValue( in, fSongSeconds );	break;	
		case RadarCategory_Voltage:			out[rc] = GetVoltageRadarValue( in, fSongSeconds );	break;
		case RadarCategory_Air:				out[rc] = GetAirRadarValue( in, fSongSeconds );		break;
		case RadarCategory_Freeze:			out[rc] = GetFreezeRadarValue( in, fSongSeconds );	break;
		case RadarCategory_Chaos:			out[rc] = GetChaosRadarValue( in, fSongSeconds );	break;
		case RadarCategory_TapsAndHolds:	out[rc] = (float) stats.taps;				break;
		case RadarCategory_Jumps:			out[rc] = (float) stats.jumps;				break;
		case RadarCategory_Holds:			out[rc] = (float) in.GetNumHoldNotes();		break;
		case RadarCategory_Mines:			out[rc] = (float) in.GetNumMines();			break;
		case RadarCategory_Hands:			out[rc] = (float) in.GetNumHands();			break;
		case RadarCategory_Rolls:			out[rc] = (float) in.GetNumRolls();			break;
		case RadarCategory_Lifts:			out[rc] = (float) in.GetNumLifts();			break;
		case RadarCategory_Fakes:			out[rc] = (float) in.GetNumFakes();			break;
		default:	FAIL_M("Non-existant radar category attempted to be set!");
		}
	}
}

float NoteDataUtil::GetStreamRadarValue( const NoteData &in, float fSongSeconds )
{
	if( !fSongSeconds )
		return 0.0f;
	// density of steps
	int iNumNotes = in.GetNumTapNotes() + in.GetNumHoldNotes();
	float fNotesPerSecond = iNumNotes/fSongSeconds;
	float fReturn = fNotesPerSecond / 7;
	return min( fReturn, 1.0f );
}

float NoteDataUtil::GetVoltageRadarValue( const NoteData &in, float fSongSeconds )
{
	if( !fSongSeconds )
		return 0.0f;

	const float fLastBeat = in.GetLastBeat();
	const float fAvgBPS = fLastBeat / fSongSeconds;

	// peak density of steps
	float fMaxDensitySoFar = 0;

	const float BEAT_WINDOW = 8;
	const int BEAT_WINDOW_ROWS = BeatToNoteRow( BEAT_WINDOW );

	for( int i=0; i<=BeatToNoteRow(fLastBeat); i+=BEAT_WINDOW_ROWS )
	{
		int iNumNotesThisWindow = in.GetNumTapNotes( i, i+BEAT_WINDOW_ROWS ) + in.GetNumHoldNotes( i, i+BEAT_WINDOW_ROWS );
		float fDensityThisWindow = iNumNotesThisWindow / BEAT_WINDOW;
		fMaxDensitySoFar = max( fMaxDensitySoFar, fDensityThisWindow );
	}

	float fReturn = fMaxDensitySoFar*fAvgBPS/10;
	return min( fReturn, 1.0f );
}

float NoteDataUtil::GetAirRadarValue( const NoteData &in, float fSongSeconds )
{
	if( !fSongSeconds )
		return 0.0f;
	// number of doubles
	int iNumDoubles = in.GetNumJumps();
	float fReturn = iNumDoubles / fSongSeconds;
	return min( fReturn, 1.0f );
}

float NoteDataUtil::GetFreezeRadarValue( const NoteData &in, float fSongSeconds )
{
	if( !fSongSeconds )
		return 0.0f;
	// number of hold steps
	float fReturn = in.GetNumHoldNotes() / fSongSeconds;
	return min( fReturn, 1.0f );
}

float NoteDataUtil::GetChaosRadarValue( const NoteData &in, float fSongSeconds )
{
	if( !fSongSeconds )
		return 0.0f;
	// count number of notes smaller than 8ths
	int iNumChaosNotes = 0;

	FOREACH_NONEMPTY_ROW_ALL_TRACKS( in, r )
	{
		if( GetNoteType(r) >= NOTE_TYPE_12TH )
			iNumChaosNotes++;
	}

	float fReturn = iNumChaosNotes / fSongSeconds * 0.5f;
	return min( fReturn, 1.0f );
}

void NoteDataUtil::RemoveHoldNotes( NoteData &in, int iStartIndex, int iEndIndex )
{
	// turn all the HoldNotes into TapNotes
	for( int t=0; t<in.GetNumTracks(); ++t )
	{
		NoteData::TrackMap::iterator begin, end;
		in.GetTapNoteRangeInclusive( t, iStartIndex, iEndIndex, begin, end );
		for( ; begin != end; ++begin )
		{
			if( begin->second.type != TapNote::hold_head ||
				begin->second.subType != TapNote::hold_head_hold )
				continue;
			begin->second.type = TapNote::tap;
		}
	}
}

void NoteDataUtil::ChangeRollsToHolds( NoteData &in, int iStartIndex, int iEndIndex )
{
	for( int t=0; t<in.GetNumTracks(); ++t )
	{
		NoteData::TrackMap::iterator begin, end;
		in.GetTapNoteRangeInclusive( t, iStartIndex, iEndIndex, begin, end );
		for( ; begin != end; ++begin )
		{
			if( begin->second.type != TapNote::hold_head ||
				begin->second.subType != TapNote::hold_head_roll )
				continue;
			begin->second.subType = TapNote::hold_head_hold;
		}
	}
}

void NoteDataUtil::ChangeHoldsToRolls( NoteData &in, int iStartIndex, int iEndIndex )
{
	for( int t=0; t<in.GetNumTracks(); ++t )
	{
		NoteData::TrackMap::iterator begin, end;
		in.GetTapNoteRangeInclusive( t, iStartIndex, iEndIndex, begin, end );
		for( ; begin != end; ++begin )
		{
			if( begin->second.type != TapNote::hold_head ||
				begin->second.subType != TapNote::hold_head_hold )
				continue;
			begin->second.subType = TapNote::hold_head_roll;
		}
	}
}

void NoteDataUtil::RemoveSimultaneousNotes( NoteData &in, int iMaxSimultaneous, int iStartIndex, int iEndIndex )
{
	// Remove tap and hold notes so no more than iMaxSimultaneous buttons are being held at any
	// given time.  Never touch data outside of the range given; if many hold notes are overlapping
	// iStartIndex, and we'd have to change those holds to obey iMaxSimultaneous, just do the best
	// we can without doing so.
	if( in.IsComposite() )
	{
		// Do this per part.
		vector<NoteData> vParts;
		
		SplitCompositeNoteData( in, vParts );
		FOREACH( NoteData, vParts, nd )
			RemoveSimultaneousNotes( *nd, iMaxSimultaneous, iStartIndex, iEndIndex );
		in.Init();
		in.SetNumTracks( vParts.front().GetNumTracks() );
		CombineCompositeNoteData( in, vParts );
	}
	FOREACH_NONEMPTY_ROW_ALL_TRACKS_RANGE( in, r, iStartIndex, iEndIndex )
	{
		set<int> viTracksHeld;
		in.GetTracksHeldAtRow( r, viTracksHeld );

		// remove the first tap note or the first hold note that starts on this row
		int iTotalTracksPressed = in.GetNumTracksWithTapOrHoldHead(r) + viTracksHeld.size();
		int iTracksToRemove = max( 0, iTotalTracksPressed - iMaxSimultaneous );
		for( int t=0; iTracksToRemove>0 && t<in.GetNumTracks(); t++ )
		{
			const TapNote &tn = in.GetTapNote(t,r);
			if( tn.type == TapNote::tap || tn.type == TapNote::hold_head )
			{
				in.SetTapNote( t, r, TAP_EMPTY );
				iTracksToRemove--;
			}
		}
	}
}

void NoteDataUtil::RemoveJumps( NoteData &inout, int iStartIndex, int iEndIndex )
{
	RemoveSimultaneousNotes( inout, 1, iStartIndex, iEndIndex );
}

void NoteDataUtil::RemoveHands( NoteData &inout, int iStartIndex, int iEndIndex )
{
	RemoveSimultaneousNotes( inout, 2, iStartIndex, iEndIndex );
}

void NoteDataUtil::RemoveQuads( NoteData &inout, int iStartIndex, int iEndIndex )
{
	RemoveSimultaneousNotes( inout, 3, iStartIndex, iEndIndex );
}

void NoteDataUtil::RemoveSpecificTapNotes( NoteData &inout, TapNote::Type tn, int iStartIndex, int iEndIndex )
{
	for( int t=0; t<inout.GetNumTracks(); t++ )
		FOREACH_NONEMPTY_ROW_IN_TRACK_RANGE( inout, t, r, iStartIndex, iEndIndex ) 
			if( inout.GetTapNote(t,r).type == tn )
				inout.SetTapNote( t, r, TAP_EMPTY );
}

void NoteDataUtil::RemoveMines( NoteData &inout, int iStartIndex, int iEndIndex )
{
	RemoveSpecificTapNotes( inout, TapNote::mine, iStartIndex, iEndIndex );
}

void NoteDataUtil::RemoveLifts( NoteData &inout, int iStartIndex, int iEndIndex )
{
	RemoveSpecificTapNotes( inout, TapNote::lift, iStartIndex, iEndIndex );
}

void NoteDataUtil::RemoveFakes( NoteData &inout, int iStartIndex, int iEndIndex )
{
	RemoveSpecificTapNotes( inout, TapNote::fake, iStartIndex, iEndIndex );
}

void NoteDataUtil::RemoveAllButOneTap( NoteData &inout, int row )
{
	if(row < 0) return;

	int track;
	for( track = 0; track < inout.GetNumTracks(); ++track )
	{
		if( inout.GetTapNote(track, row).type == TapNote::tap )
			break;
	}

	track++;

	for( ; track < inout.GetNumTracks(); ++track )
	{
		NoteData::iterator iter = inout.FindTapNote( track, row );
		if( iter != inout.end(track) && iter->second.type == TapNote::tap )
			inout.RemoveTapNote( track, iter );
	}
}

void NoteDataUtil::RemoveAllButPlayer( NoteData &inout, PlayerNumber pn )
{
	for( int track = 0; track < inout.GetNumTracks(); ++track )
	{
		NoteData::iterator i = inout.begin( track );
		
		while( i != inout.end(track) )
		{
			if( i->second.pn != pn && i->second.pn != PLAYER_INVALID )
				inout.RemoveTapNote( track, i++ );
			else
				++i;
		}
	}
}

// TODO: Perform appropriate matrix calculations for everything instead.
static void GetTrackMapping( StepsType st, NoteDataUtil::TrackMapping tt, int NumTracks, int *iTakeFromTrack )
{
	// Identity transform for cases not handled below.
	for( int t = 0; t < MAX_NOTE_TRACKS; ++t )
		iTakeFromTrack[t] = t;

	switch( tt )
	{
	case NoteDataUtil::left:
	case NoteDataUtil::right:
		// Is there a way to do this without handling each StepsType? -Chris
		switch( st )
		{
		case StepsType_dance_single:
		case StepsType_dance_double:
		case StepsType_dance_couple:
		case StepsType_dance_routine:
			iTakeFromTrack[0] = 2;
			iTakeFromTrack[1] = 0;
			iTakeFromTrack[2] = 3;
			iTakeFromTrack[3] = 1;
			iTakeFromTrack[4] = 6;
			iTakeFromTrack[5] = 4;
			iTakeFromTrack[6] = 7;
			iTakeFromTrack[7] = 5;
			break;
		case StepsType_dance_solo:
			iTakeFromTrack[0] = 5;
			iTakeFromTrack[1] = 4;
			iTakeFromTrack[2] = 0;
			iTakeFromTrack[3] = 3;
			iTakeFromTrack[4] = 1;
			iTakeFromTrack[5] = 2;
			break;
		case StepsType_pump_single:
		case StepsType_pump_couple:
			iTakeFromTrack[0] = 1;
			iTakeFromTrack[1] = 3;
			iTakeFromTrack[2] = 2;
			iTakeFromTrack[3] = 4;
			iTakeFromTrack[4] = 0;
			iTakeFromTrack[5] = 6;
			iTakeFromTrack[6] = 8;
			iTakeFromTrack[7] = 7;
			iTakeFromTrack[8] = 9;
			iTakeFromTrack[9] = 5;
			break;
		case StepsType_pump_halfdouble:
			iTakeFromTrack[0] = 2;
			iTakeFromTrack[1] = 0;
			iTakeFromTrack[2] = 1;
			iTakeFromTrack[3] = 3;
			iTakeFromTrack[4] = 4;
			iTakeFromTrack[5] = 5;
			break;
		case StepsType_pump_double:
			iTakeFromTrack[0] = 8;
			iTakeFromTrack[1] = 9;
			iTakeFromTrack[2] = 7;
			iTakeFromTrack[3] = 5;
			iTakeFromTrack[4] = 6;
			iTakeFromTrack[5] = 3;
			iTakeFromTrack[6] = 4;
			iTakeFromTrack[7] = 2;
			iTakeFromTrack[8] = 0;
			iTakeFromTrack[9] = 1;
			break;
		default: break;
		}

		if( tt == NoteDataUtil::right )
		{
			// Invert.
			int iTrack[MAX_NOTE_TRACKS];
			memcpy( iTrack, iTakeFromTrack, sizeof(iTrack) );
			for( int t = 0; t < MAX_NOTE_TRACKS; ++t )
			{
				const int to = iTrack[t];
				iTakeFromTrack[to] = t;
			}
		}

		break;
	case NoteDataUtil::backwards:
	{
		// If a Pump game type, treat differently. Otherwise, send to mirror.
		bool needsBackwards = true;
		switch (st)
		{
			case StepsType_pump_single:
			case StepsType_pump_couple:
			{
				iTakeFromTrack[0] = 3;
				iTakeFromTrack[1] = 4;
				iTakeFromTrack[2] = 2;
				iTakeFromTrack[3] = 0;
				iTakeFromTrack[4] = 1;
				iTakeFromTrack[5] = 8;
				iTakeFromTrack[6] = 9;
				iTakeFromTrack[7] = 2;
				iTakeFromTrack[8] = 5;
				iTakeFromTrack[9] = 6;
				break;
			}
			case StepsType_pump_double:
			case StepsType_pump_routine:
			{
				iTakeFromTrack[0] = 8;
				iTakeFromTrack[1] = 9;
				iTakeFromTrack[2] = 7;
				iTakeFromTrack[3] = 5;
				iTakeFromTrack[4] = 6;
				iTakeFromTrack[5] = 3;
				iTakeFromTrack[6] = 4;
				iTakeFromTrack[7] = 2;
				iTakeFromTrack[8] = 0;
				iTakeFromTrack[9] = 1;
				break;
			}
			case StepsType_pump_halfdouble:
			{
				iTakeFromTrack[0] = 5;
				iTakeFromTrack[1] = 3;
				iTakeFromTrack[2] = 4;
				iTakeFromTrack[3] = 1;
				iTakeFromTrack[4] = 2;
				iTakeFromTrack[5] = 0;
				break;
			}
			default:
				needsBackwards = false;
		}
		if (needsBackwards) break;
	}
	case NoteDataUtil::mirror:
		{
			for( int t=0; t<NumTracks; t++ )
				iTakeFromTrack[t] = NumTracks-t-1;
			break;
		}
	case NoteDataUtil::shuffle:
	case NoteDataUtil::super_shuffle:		// use shuffle code to mix up HoldNotes without creating impossible patterns
		{
			// TRICKY: Shuffle so that both player get the same shuffle mapping
			// in the same round.
			int iOrig[MAX_NOTE_TRACKS];
			memcpy( iOrig, iTakeFromTrack, sizeof(iOrig) );

			int iShuffleSeed = GAMESTATE->m_iStageSeed;
			do {
				RandomGen rnd( iShuffleSeed );
				random_shuffle( &iTakeFromTrack[0], &iTakeFromTrack[NumTracks], rnd );
				iShuffleSeed++;
			}
			while ( !memcmp( iOrig, iTakeFromTrack, sizeof(iOrig) ) );
		}
		break;
	case NoteDataUtil::soft_shuffle:
		{
			// XXX: this is still pretty much a stub.

			// soft shuffle, as described at
			// http://www.stepmania.com/forums/showthread.php?t=19469

			/* one of the following at random:
			 *
			 * 0. No columns changed
			 * 1. Left and right columns swapped
			 * 2. Down and up columns swapped
			 * 3. Mirror (left and right swapped, down and up swapped)
			 * ----------------------------------------------------------------
			 * To extend it to handle all game types, it would pick each axis
			 * of symmetry the game type has and either flip it or not flip it.
			 *
			 * For instance, PIU singles has four axes:
			 * horizontal, vertical,
			 * diagonally top left to bottom right,
			 * diagonally bottom left to top right.
			 * (above text from forums) */

			// TRICKY: Shuffle so that both player get the same shuffle mapping
			// in the same round.

			int iShuffleSeed = GAMESTATE->m_iStageSeed;
			RandomGen rnd( iShuffleSeed );
			int iRandChoice = (rnd() % 4);

			// XXX: cases 1 and 2 only implemented for dance_*
			switch( iRandChoice )
			{
				case 1: // left and right mirror
				case 2: // up and down mirror
					switch( st )
					{
					case StepsType_dance_single:
						if( iRandChoice == 1 )
						{
							// left and right
							iTakeFromTrack[0] = 3;
							iTakeFromTrack[3] = 0;
						}
						if( iRandChoice == 2 )
						{
							// up and down
							iTakeFromTrack[1] = 2;
							iTakeFromTrack[2] = 1;
						}
						break;
					case StepsType_dance_double:
					case StepsType_dance_couple:
					case StepsType_dance_routine:
						if( iRandChoice == 1 )
						{
							// left and right
							iTakeFromTrack[0] = 3;
							iTakeFromTrack[3] = 0;
							iTakeFromTrack[4] = 7;
							iTakeFromTrack[7] = 4;
						}
						if( iRandChoice == 2 )
						{
							// up and down
							iTakeFromTrack[1] = 2;
							iTakeFromTrack[2] = 1;
							iTakeFromTrack[5] = 6;
							iTakeFromTrack[6] = 5;
						}
						break;
					// here be dragons (unchanged code)
					case StepsType_dance_solo:
						iTakeFromTrack[0] = 5;
						iTakeFromTrack[1] = 4;
						iTakeFromTrack[2] = 0;
						iTakeFromTrack[3] = 3;
						iTakeFromTrack[4] = 1;
						iTakeFromTrack[5] = 2;
						break;
					case StepsType_pump_single:
					case StepsType_pump_couple:
						iTakeFromTrack[0] = 3;
						iTakeFromTrack[1] = 4;
						iTakeFromTrack[2] = 2;
						iTakeFromTrack[3] = 0;
						iTakeFromTrack[4] = 1;
						iTakeFromTrack[5] = 8;
						iTakeFromTrack[6] = 9;
						iTakeFromTrack[7] = 7;
						iTakeFromTrack[8] = 5;
						iTakeFromTrack[9] = 6;
						break;
					case StepsType_pump_halfdouble:
						iTakeFromTrack[0] = 2;
						iTakeFromTrack[1] = 0;
						iTakeFromTrack[2] = 1;
						iTakeFromTrack[3] = 3;
						iTakeFromTrack[4] = 4;
						iTakeFromTrack[5] = 5;
						break;
					case StepsType_pump_double:
						iTakeFromTrack[0] = 8;
						iTakeFromTrack[1] = 9;
						iTakeFromTrack[2] = 7;
						iTakeFromTrack[3] = 5;
						iTakeFromTrack[4] = 6;
						iTakeFromTrack[5] = 3;
						iTakeFromTrack[6] = 4;
						iTakeFromTrack[7] = 2;
						iTakeFromTrack[8] = 0;
						iTakeFromTrack[9] = 1;
						break;
					default: break;
					}
					break;
				case 3: // full mirror
					GetTrackMapping( st, NoteDataUtil::mirror, NumTracks, iTakeFromTrack );
					break;
				case 0:
				default:
					// case 0 and default are set by identity matrix above
					break;
			}
		}
		break;
	case NoteDataUtil::stomp:
		switch( st )
		{
		case StepsType_dance_single:
		case StepsType_dance_couple:
			iTakeFromTrack[0] = 3;
			iTakeFromTrack[1] = 2;
			iTakeFromTrack[2] = 1;
			iTakeFromTrack[3] = 0;
			iTakeFromTrack[4] = 7;
			iTakeFromTrack[5] = 6;
			iTakeFromTrack[6] = 5;
			iTakeFromTrack[7] = 4;
			break;
		case StepsType_dance_double:
		case StepsType_dance_routine:
			iTakeFromTrack[0] = 1;
			iTakeFromTrack[1] = 0;
			iTakeFromTrack[2] = 3;
			iTakeFromTrack[3] = 2;
			iTakeFromTrack[4] = 5;
			iTakeFromTrack[5] = 4;
			iTakeFromTrack[6] = 7;
			iTakeFromTrack[7] = 6;
			break;
		default: 
			break;
		}
		break;
	default:
		ASSERT(0);
	}
}

static void SuperShuffleTaps( NoteData &inout, int iStartIndex, int iEndIndex )
{
	/*
	 * We already did the normal shuffling code above, which did a good job
	 * of shuffling HoldNotes without creating impossible patterns.
	 * Now, go in and shuffle the TapNotes per-row.
	 *
	 * This is only called by NoteDataUtil::Turn.
	 */
	FOREACH_NONEMPTY_ROW_ALL_TRACKS_RANGE( inout, r, iStartIndex, iEndIndex )
	{
		for( int t1=0; t1<inout.GetNumTracks(); t1++ )
		{
			const TapNote &tn1 = inout.GetTapNote( t1, r );
			switch( tn1.type )
			{
			case TapNote::empty:
			case TapNote::hold_head:
			case TapNote::hold_tail:
			case TapNote::autoKeysound:
				continue;	// skip
			case TapNote::tap:
			case TapNote::mine:
			case TapNote::attack:
			case TapNote::lift:
			case TapNote::fake:
				break;	// shuffle this
			DEFAULT_FAIL( tn1.type );
			}

			DEBUG_ASSERT_M( !inout.IsHoldNoteAtRow(t1,r), ssprintf("There is a tap.type = %d inside of a hold at row %d", tn1.type, r) );

			// Probe for a spot to swap with.
			set<int> vTriedTracks;
			for( int i=0; i<4; i++ )	// probe max 4 times
			{
				int t2 = RandomInt( inout.GetNumTracks() );
				if( vTriedTracks.find(t2) != vTriedTracks.end() )	// already tried this track
					continue;	// skip
				vTriedTracks.insert( t2 );

				// swapping with ourself is a no-op
				if( t1 == t2 )
					break;	// done swapping

				const TapNote &tn2 = inout.GetTapNote( t2, r );
				switch( tn2.type )
				{
				case TapNote::hold_head:
				case TapNote::hold_tail:
				case TapNote::autoKeysound:
					continue;	// don't swap with these
				case TapNote::empty:
				case TapNote::tap:
				case TapNote::mine:
				case TapNote::attack:
				case TapNote::lift:
				case TapNote::fake:
					break;	// ok to swap with this
				DEFAULT_FAIL( tn2.type );
				}

				// don't swap into the middle of a hold note
				if( inout.IsHoldNoteAtRow( t2,r ) )
					continue;

				// do the swap
				const TapNote tnTemp = tn1;
				inout.SetTapNote( t1, r, tn2 );
				inout.SetTapNote( t2, r, tnTemp );
				
				break;	// done swapping
			}
		}
	}
}


void NoteDataUtil::Turn( NoteData &inout, StepsType st, TrackMapping tt, int iStartIndex, int iEndIndex )
{
	int iTakeFromTrack[MAX_NOTE_TRACKS];	// New track "t" will take from old track iTakeFromTrack[t]
	GetTrackMapping( st, tt, inout.GetNumTracks(), iTakeFromTrack );

	NoteData tempNoteData;
	tempNoteData.LoadTransformed( inout, inout.GetNumTracks(), iTakeFromTrack );

	if( tt == super_shuffle )
		SuperShuffleTaps( tempNoteData, iStartIndex, iEndIndex );

	inout.CopyAll( tempNoteData );
}

void NoteDataUtil::Backwards( NoteData &inout )
{
	NoteData out;
	out.SetNumTracks( inout.GetNumTracks() );

	int max_row = inout.GetLastRow();
	for( int t=0; t<inout.GetNumTracks(); t++ )
	{
		FOREACH_NONEMPTY_ROW_IN_TRACK_RANGE( inout, t, r, 0, max_row )
		{
			int iRowEarlier = r;
			int iRowLater = max_row-r;

			const TapNote &tnEarlier = inout.GetTapNote( t, iRowEarlier );
			if( tnEarlier.type == TapNote::hold_head )
				iRowLater -= tnEarlier.iDuration;

			out.SetTapNote( t, iRowLater, tnEarlier );
		}
	}

	inout.swap( out );
}

void NoteDataUtil::SwapSides( NoteData &inout )
{
	int iOriginalTrackToTakeFrom[MAX_NOTE_TRACKS];
	for( int t = 0; t < inout.GetNumTracks()/2; ++t )
	{
		int iTrackEarlier = t;
		int iTrackLater = t + inout.GetNumTracks()/2 + inout.GetNumTracks()%2;
		iOriginalTrackToTakeFrom[iTrackEarlier] = iTrackLater;
		iOriginalTrackToTakeFrom[iTrackLater] = iTrackEarlier;
	}

	NoteData orig( inout );
	inout.LoadTransformed( orig, orig.GetNumTracks(), iOriginalTrackToTakeFrom );
}

void NoteDataUtil::Little( NoteData &inout, int iStartIndex, int iEndIndex )
{
	// filter out all non-quarter notes
	for( int t=0; t<inout.GetNumTracks(); t++ ) 
	{
		FOREACH_NONEMPTY_ROW_IN_TRACK_RANGE( inout, t, i, iStartIndex, iEndIndex )
		{
			if( i % ROWS_PER_BEAT == 0 )
				continue;
			inout.SetTapNote( t, i, TAP_EMPTY );
		}
	}
}

// Make all quarter notes into jumps.
void NoteDataUtil::Wide( NoteData &inout, int iStartIndex, int iEndIndex )
{
	/* Start on an even beat. */
	iStartIndex = Quantize( iStartIndex, BeatToNoteRow(2.0f) );

	FOREACH_NONEMPTY_ROW_ALL_TRACKS_RANGE( inout, i, iStartIndex, iEndIndex )
	{
		if( i % BeatToNoteRow(2.0f) != 0 )
			continue;	// even beats only

		bool bHoldNoteAtBeat = false;
		for( int t = 0; !bHoldNoteAtBeat && t < inout.GetNumTracks(); ++t )
			if( inout.IsHoldNoteAtRow(t, i) )
				bHoldNoteAtBeat = true;
		if( bHoldNoteAtBeat )
			continue;	// skip.  Don't place during holds

		if( inout.GetNumTracksWithTap(i) != 1 )
			continue;	// skip

		bool bSpaceAroundIsEmpty = true;	// no other notes with a 1/8th of this row
		FOREACH_NONEMPTY_ROW_ALL_TRACKS_RANGE( inout, j, i-ROWS_PER_BEAT/2+1, i+ROWS_PER_BEAT/2 )
			if( j!=i  &&  inout.GetNumTapNonEmptyTracks(j) > 0 )
			{
				bSpaceAroundIsEmpty = false;
				break;
			}
				
		if( !bSpaceAroundIsEmpty )
			continue;	// skip

		// add a note determinitsitcally
		int iBeat = lrintf( NoteRowToBeat(i) );
		int iTrackOfNote = inout.GetFirstTrackWithTap(i);
		int iTrackToAdd = iTrackOfNote + (iBeat%5)-2;	// won't be more than 2 tracks away from the existing note
		CLAMP( iTrackToAdd, 0, inout.GetNumTracks()-1 );
		if( iTrackToAdd == iTrackOfNote )
			iTrackToAdd++;
		CLAMP( iTrackToAdd, 0, inout.GetNumTracks()-1 );
		if( iTrackToAdd == iTrackOfNote )
			iTrackToAdd--;
		CLAMP( iTrackToAdd, 0, inout.GetNumTracks()-1 );

		if( inout.GetTapNote(iTrackToAdd, i).type != TapNote::empty  &&  inout.GetTapNote(iTrackToAdd, i).type != TapNote::fake )
		{
			iTrackToAdd = (iTrackToAdd+1) % inout.GetNumTracks();
		}
		inout.SetTapNote(iTrackToAdd, i, TAP_ADDITION_TAP);
	}
}

void NoteDataUtil::Big( NoteData &inout, int iStartIndex, int iEndIndex )
{
	// add 8ths between 4ths
	InsertIntelligentTaps( inout,BeatToNoteRow(1.0f), BeatToNoteRow(0.5f), BeatToNoteRow(1.0f), false,iStartIndex,iEndIndex );
}

void NoteDataUtil::Quick( NoteData &inout, int iStartIndex, int iEndIndex )
{
	// add 16ths between 8ths
	InsertIntelligentTaps( inout, BeatToNoteRow(0.5f), BeatToNoteRow(0.25f), BeatToNoteRow(1.0f), false,iStartIndex,iEndIndex );
}

// Due to popular request by people annoyed with the "new" implementation of Quick, we now have
// this BMR-izer for your steps.  Use with caution.
void NoteDataUtil::BMRize( NoteData &inout, int iStartIndex, int iEndIndex )
{
	Big( inout, iStartIndex, iEndIndex );
	Quick( inout, iStartIndex, iEndIndex );
}

void NoteDataUtil::Skippy( NoteData &inout, int iStartIndex, int iEndIndex )
{
	// add 16ths between 4ths
	InsertIntelligentTaps( inout, BeatToNoteRow(1.0f), BeatToNoteRow(0.75f),BeatToNoteRow(1.0f), true,iStartIndex,iEndIndex );
}

void NoteDataUtil::InsertIntelligentTaps( 
	NoteData &inout, 
	int iWindowSizeRows, 
	int iInsertOffsetRows, 
	int iWindowStrideRows, 
	bool bSkippy, 
	int iStartIndex,
	int iEndIndex )
{
	ASSERT( iInsertOffsetRows <= iWindowSizeRows );
	ASSERT( iWindowSizeRows <= iWindowStrideRows );

	bool bRequireNoteAtBeginningOfWindow = !bSkippy;
	bool bRequireNoteAtEndOfWindow = true;

	/* Start on a multiple of fBeatInterval. */
	iStartIndex = Quantize( iStartIndex, iWindowStrideRows );

	FOREACH_NONEMPTY_ROW_ALL_TRACKS_RANGE( inout, i, iStartIndex, iEndIndex )
	{
		// Insert a beat in the middle of every fBeatInterval.
		if( i % iWindowStrideRows != 0 )
			continue;	// even beats only

		int iRowEarlier = i;
		int iRowLater = i + iWindowSizeRows;
		int iRowToAdd = i + iInsertOffsetRows;
		// following two lines have been changed because the behavior of treating hold-heads
		// as different from taps doesn't feel right, and because we need to check
		// against TAP_ADDITION with the BMRize mod.
		if( bRequireNoteAtBeginningOfWindow )
			if( inout.GetNumTapNonEmptyTracks(iRowEarlier)!=1 || inout.GetNumTracksWithTapOrHoldHead(iRowEarlier)!=1 )
				continue;
		if( bRequireNoteAtEndOfWindow )
			if( inout.GetNumTapNonEmptyTracks(iRowLater)!=1 || inout.GetNumTracksWithTapOrHoldHead(iRowLater)!=1 )
				continue;
		// there is a 4th and 8th note surrounding iRowBetween
		
		// don't insert a new note if there's already one within this interval
		bool bNoteInMiddle = false;
		for( int t = 0; t < inout.GetNumTracks(); ++t )
			if( inout.IsHoldNoteAtRow(t, iRowEarlier+1) )
				bNoteInMiddle = true;
		FOREACH_NONEMPTY_ROW_ALL_TRACKS_RANGE( inout, j, iRowEarlier+1, iRowLater-1 )
			bNoteInMiddle = true;
		if( bNoteInMiddle )
			continue;

		// add a note deterministically somewhere on a track different from the two surrounding notes
		int iTrackOfNoteEarlier = -1;
		bool bEarlierHasNonEmptyTrack = inout.GetTapFirstNonEmptyTrack( iRowEarlier, iTrackOfNoteEarlier );
		int iTrackOfNoteLater = -1;
		inout.GetTapFirstNonEmptyTrack( iRowLater, iTrackOfNoteLater );
		int iTrackOfNoteToAdd = 0;
		if( bSkippy  &&
			iTrackOfNoteEarlier != iTrackOfNoteLater &&   // Don't make skips on the same note
			bEarlierHasNonEmptyTrack )
		{
			iTrackOfNoteToAdd = iTrackOfNoteEarlier;
		}
		else if( abs(iTrackOfNoteEarlier-iTrackOfNoteLater) >= 2 )
		{
			// try to choose a track between the earlier and later notes
			iTrackOfNoteToAdd = min(iTrackOfNoteEarlier,iTrackOfNoteLater)+1;
		}
		else if( min(iTrackOfNoteEarlier,iTrackOfNoteLater)-1 >= 0 )
		{
			// try to choose a track just to the left
			iTrackOfNoteToAdd = min(iTrackOfNoteEarlier,iTrackOfNoteLater)-1;
		}
		else if( max(iTrackOfNoteEarlier,iTrackOfNoteLater)+1 < inout.GetNumTracks() )
		{
			// try to choose a track just to the right
			iTrackOfNoteToAdd = max(iTrackOfNoteEarlier,iTrackOfNoteLater)+1;
		}

		inout.SetTapNote(iTrackOfNoteToAdd, iRowToAdd, TAP_ADDITION_TAP);
	}
}
#if 0
class TrackIterator
{
public:
	TrackIterator();

	/* If called, iterate only over [iStart,iEnd]. */
	void SetRange( int iStart, int iEnd )
	{
	}

	/* If called, pay attention to iTrack only. */
	void SetTrack( iTrack );

	/* Extend iStart and iEnd to include hold notes overlapping the boundaries.  Call SetRange()
	 * and SetTrack() first. */
	void HoldInclusive();

	/* Reduce iStart and iEnd to exclude hold notes overlapping the boundaries.  Call SetRange()
	 * and SetTrack() first. */
	void HoldExclusive();

	/* If called, keep the iterator around.  This results in much faster iteration.  If used,
	 * ensure that the current row will always remain valid.  SetTrack() must be called first. */
	void Fast();

	/* Retrieve an iterator for the current row.  SetTrack() must be called first (but Fast()
	 * does not). */
	TapNote::iterator Get();

	int GetRow() const { return m_iCurrentRow; }
	bool Prev();
	bool Next();

private:
	int m_iStart, m_iEnd;
	int m_iTrack;

	bool m_bFast;

	int m_iCurrentRow;

	NoteData::iterator m_Iterator;

	/* m_bFast only: */
	NoteData::iterator m_Begin, m_End;
};

bool TrackIterator::Next()
{
	if( m_bFast )
	{
		if( m_Iterator == XXX )
			;

	}

}

TrackIterator::TrackIterator()
{
	m_iStart = 0;
	m_iEnd = MAX_NOTE_ROW;
	m_iTrack = -1;
}
#endif

void NoteDataUtil::AddMines( NoteData &inout, int iStartIndex, int iEndIndex )
{
	// Change whole rows at a time to be tap notes.  Otherwise, it causes
	// major problems for our scoring system. -Chris

	int iRowCount = 0;
	int iPlaceEveryRows = 6;
	FOREACH_NONEMPTY_ROW_ALL_TRACKS_RANGE( inout, r, iStartIndex, iEndIndex )
	{
		iRowCount++;

		// place every 6 or 7 rows
		// XXX: What is "6 or 7" derived from?  Can we calculate that in a way
		// that won't break if ROWS_PER_MEASURE changes?
		if( iRowCount>=iPlaceEveryRows )
		{
			for( int t=0; t<inout.GetNumTracks(); t++ )
				if( inout.GetTapNote(t,r).type == TapNote::tap )
					inout.SetTapNote(t,r,TAP_ADDITION_MINE);
			
			iRowCount = 0;
			if( iPlaceEveryRows == 6 )
				iPlaceEveryRows = 7;
			else
				iPlaceEveryRows = 6;
		}
	}

	// Place mines right after hold so player must lift their foot.
	for( int iTrack=0; iTrack<inout.GetNumTracks(); ++iTrack )
	{
		FOREACH_NONEMPTY_ROW_IN_TRACK_RANGE( inout, iTrack, r, iStartIndex, iEndIndex )
		{
			const TapNote &tn = inout.GetTapNote( iTrack, r );
			if( tn.type != TapNote::hold_head )
				continue;

			int iMineRow = r + tn.iDuration + BeatToNoteRow(0.5f);
			if( iMineRow < iStartIndex || iMineRow > iEndIndex )
				continue;

			// Only place a mines if there's not another step nearby
			int iMineRangeBegin = iMineRow - BeatToNoteRow( 0.5f ) + 1;
			int iMineRangeEnd = iMineRow + BeatToNoteRow( 0.5f ) - 1;
			if( !inout.IsRangeEmpty(iTrack, iMineRangeBegin, iMineRangeEnd) )
				continue;
		
			// Add a mine right after the hold end.
			inout.SetTapNote( iTrack, iMineRow, TAP_ADDITION_MINE );

			// Convert all notes in this row to mines.
			for( int t=0; t<inout.GetNumTracks(); t++ )
				if( inout.GetTapNote(t,iMineRow).type == TapNote::tap )
					inout.SetTapNote(t,iMineRow,TAP_ADDITION_MINE);

			iRowCount = 0;
		}
	}
}

void NoteDataUtil::Echo( NoteData &inout, int iStartIndex, int iEndIndex )
{
	// add 8th note tap "echos" after all taps
	int iEchoTrack = -1;

	const int rows_per_interval = BeatToNoteRow( 0.5f );
	iStartIndex = Quantize( iStartIndex, rows_per_interval );

	/* Clamp iEndIndex to the last real tap note.  Otherwise, we'll keep adding
	 * echos of our echos all the way up to MAX_TAP_ROW. */
	iEndIndex = min( iEndIndex, inout.GetLastRow() )+1;

	// window is one beat wide and slides 1/2 a beat at a time
	FOREACH_NONEMPTY_ROW_ALL_TRACKS_RANGE( inout, r, iStartIndex, iEndIndex )
	{
		if( r % rows_per_interval != 0 )
			continue;	// 8th notes only

		const int iRowWindowBegin = r;
		const int iRowWindowEnd = r + rows_per_interval*2;

		const int iFirstTapInRow = inout.GetFirstTrackWithTap(iRowWindowBegin);
		if( iFirstTapInRow != -1 )
			iEchoTrack = iFirstTapInRow;

		if( iEchoTrack==-1 )
			continue;	// don't lay

		// don't insert a new note if there's already a tap within this interval
		bool bTapInMiddle = false;
		FOREACH_NONEMPTY_ROW_ALL_TRACKS_RANGE( inout, r2, iRowWindowBegin+1, iRowWindowEnd-1 )
			bTapInMiddle = true;
		if( bTapInMiddle )
			continue;	// don't lay


		const int iRowEcho = r + rows_per_interval;
		{
			set<int> viTracks;
			inout.GetTracksHeldAtRow( iRowEcho, viTracks );

			// don't lay if holding 2 already
			if( viTracks.size() >= 2 )
				continue;	// don't lay
			
			// don't lay echos on top of a HoldNote
			if( find(viTracks.begin(),viTracks.end(),iEchoTrack) != viTracks.end() )
				continue;	// don't lay
		}

		inout.SetTapNote( iEchoTrack, iRowEcho, TAP_ADDITION_TAP );
	}
}

void NoteDataUtil::Planted( NoteData &inout, int iStartIndex, int iEndIndex )
{
	ConvertTapsToHolds( inout, 1, iStartIndex, iEndIndex );
}
void NoteDataUtil::Floored( NoteData &inout, int iStartIndex, int iEndIndex )
{
	ConvertTapsToHolds( inout, 2, iStartIndex, iEndIndex );
}
void NoteDataUtil::Twister( NoteData &inout, int iStartIndex, int iEndIndex )
{
	ConvertTapsToHolds( inout, 3, iStartIndex, iEndIndex );
}
void NoteDataUtil::ConvertTapsToHolds( NoteData &inout, int iSimultaneousHolds, int iStartIndex, int iEndIndex )
{
	// Convert all taps to freezes.
	FOREACH_NONEMPTY_ROW_ALL_TRACKS_RANGE( inout, r, iStartIndex, iEndIndex )
	{
		int iTrackAddedThisRow = 0;
		for( int t=0; t<inout.GetNumTracks(); t++ )
		{
			if( iTrackAddedThisRow > iSimultaneousHolds )
				break;

			if( inout.GetTapNote(t,r).type == TapNote::tap )
			{
				// Find the ending row for this hold
				int iTapsLeft = iSimultaneousHolds;

				int r2 = r+1;
				bool addHold = true;
				FOREACH_NONEMPTY_ROW_ALL_TRACKS_RANGE( inout, next_row, r+1, iEndIndex )
				{
					r2 = next_row;

					// If there are two taps in a row on the same track, 
					// don't convert the earlier one to a hold.
					if( inout.GetTapNote(t,r2).type != TapNote::empty )
					{
						addHold = false;
						break;
					}

					set<int> tracksDown;
					inout.GetTracksHeldAtRow( r2, tracksDown );
					inout.GetTapNonEmptyTracks( r2, tracksDown );
					iTapsLeft -= tracksDown.size();
					if( iTapsLeft == 0 )
						break;	// we found the ending row for this hold
					else if( iTapsLeft < 0 )
					{
						addHold = false;
						break;
					}
				}

				if (!addHold)
				{
					continue;
				}

				// If the steps end in a tap, convert that tap
				// to a hold that lasts for at least one beat.
				if( r2 == r+1 )
					r2 = r+BeatToNoteRow(1);

				inout.AddHoldNote( t, r, r2, TAP_ORIGINAL_HOLD_HEAD );
				iTrackAddedThisRow++;
			}
		}
	}

}

void NoteDataUtil::Stomp( NoteData &inout, StepsType st, int iStartIndex, int iEndIndex )
{
	// Make all non jumps with ample space around them into jumps.
	int iTrackMapping[MAX_NOTE_TRACKS];
	GetTrackMapping( st, stomp, inout.GetNumTracks(), iTrackMapping );

	FOREACH_NONEMPTY_ROW_ALL_TRACKS_RANGE( inout, r, iStartIndex, iEndIndex )
	{
		if( inout.GetNumTracksWithTap(r) != 1 )
			continue;	// skip

		for( int t=0; t<inout.GetNumTracks(); t++ )
		{
			if( inout.GetTapNote(t, r).type == TapNote::tap )	// there is a tap here
			{
				// Look to see if there is enough empty space on either side of the note
				// to turn this into a jump.
				int iRowWindowBegin = r - BeatToNoteRow(0.5f);
				int iRowWindowEnd = r + BeatToNoteRow(0.5f);

				bool bTapInMiddle = false;
				FOREACH_NONEMPTY_ROW_ALL_TRACKS_RANGE( inout, r2, iRowWindowBegin+1, iRowWindowEnd-1 )
					if( inout.IsThereATapAtRow(r2) && r2 != r )	// don't count the note we're looking around
					{
						bTapInMiddle = true;
						break;
					}
				if( bTapInMiddle )
					continue;

				// don't convert to jump if there's a hold here
				int iNumTracksHeld = inout.GetNumTracksHeldAtRow(r);
				if( iNumTracksHeld >= 1 )
					continue;

				int iOppositeTrack = iTrackMapping[t];
				inout.SetTapNote( iOppositeTrack, r, TAP_ADDITION_TAP );
			}
		}
	}		
}

void NoteDataUtil::SnapToNearestNoteType( NoteData &inout, NoteType nt1, NoteType nt2, int iStartIndex, int iEndIndex )
{
	// nt2 is optional and should be NoteType_Invalid if it is not used

	float fSnapInterval1 = NoteTypeToBeat( nt1 );
	float fSnapInterval2 = 10000; // nothing will ever snap to this.  That's what we want!
	if( nt2 != NoteType_Invalid )
		fSnapInterval2 = NoteTypeToBeat( nt2 );

	// iterate over all TapNotes in the interval and snap them
	FOREACH_NONEMPTY_ROW_ALL_TRACKS_RANGE( inout, iOldIndex, iStartIndex, iEndIndex )
	{
		int iNewIndex1 = Quantize( iOldIndex, BeatToNoteRow(fSnapInterval1) );
		int iNewIndex2 = Quantize( iOldIndex, BeatToNoteRow(fSnapInterval2) );

		bool bNewBeat1IsCloser = abs(iNewIndex1-iOldIndex) < abs(iNewIndex2-iOldIndex);
		int iNewIndex = bNewBeat1IsCloser? iNewIndex1 : iNewIndex2;

		for( int c=0; c<inout.GetNumTracks(); c++ )
		{
			TapNote tnNew = inout.GetTapNote(c, iOldIndex);
			if( tnNew.type == TapNote::empty )
				continue;

			inout.SetTapNote(c, iOldIndex, TAP_EMPTY);

			if( tnNew.type == TapNote::tap && inout.IsHoldNoteAtRow(c, iNewIndex) )
				continue; // HoldNotes override TapNotes

			if( tnNew.type == TapNote::hold_head )
			{
				/* Quantize the duration.  If the result is empty, just discard the hold. */
				tnNew.iDuration = Quantize( tnNew.iDuration, BeatToNoteRow(fSnapInterval1) );
				if( tnNew.iDuration == 0 )
					continue;

				/* We might be moving a hold note downwards, or extending its duration
				 * downwards.  Make sure there isn't anything else in the new range. */
				inout.ClearRangeForTrack( iNewIndex, iNewIndex+tnNew.iDuration+1, c );
			}
			
			inout.SetTapNote( c, iNewIndex, tnNew );
		}
	}
}


void NoteDataUtil::CopyLeftToRight( NoteData &inout )
{
	/* XXX
	inout.ConvertHoldNotesTo4s();
	for( int t=0; t<inout.GetNumTracks()/2; t++ )
	{
		FOREACH_NONEMPTY_ROW_IN_TRACK( inout, t, r )
		{
			int iTrackEarlier = t;
			int iTrackLater = inout.GetNumTracks()-1-t;

			const TapNote &tnEarlier = inout.GetTapNote(iTrackEarlier, r);
			inout.SetTapNote(iTrackLater, r, tnEarlier);
		}
	}
	inout.Convert4sToHoldNotes();
*/
}

void NoteDataUtil::CopyRightToLeft( NoteData &inout )
{
	/* XXX
	inout.ConvertHoldNotesTo4s();
	for( int t=0; t<inout.GetNumTracks()/2; t++ )
	{
		FOREACH_NONEMPTY_ROW_IN_TRACK( inout, t, r )
		{
			int iTrackEarlier = t;
			int iTrackLater = inout.GetNumTracks()-1-t;

			TapNote tnLater = inout.GetTapNote(iTrackLater, r);
			inout.SetTapNote(iTrackEarlier, r, tnLater);
		}
	}
	inout.Convert4sToHoldNotes();
*/
}

void NoteDataUtil::ClearLeft( NoteData &inout )
{
	for( int t=0; t<inout.GetNumTracks()/2; t++ )
		inout.ClearRangeForTrack( 0, MAX_NOTE_ROW, t );
}

void NoteDataUtil::ClearRight( NoteData &inout )
{
	for( int t=(inout.GetNumTracks()+1)/2; t<inout.GetNumTracks(); t++ )
		inout.ClearRangeForTrack( 0, MAX_NOTE_ROW, t );
}

void NoteDataUtil::CollapseToOne( NoteData &inout )
{
	FOREACH_NONEMPTY_ROW_ALL_TRACKS( inout, r )
		for( int t=1; t<inout.GetNumTracks(); t++ )
		{
			NoteData::iterator iter = inout.FindTapNote( t, r );
			if( iter == inout.end(t) )
				continue;
			inout.SetTapNote( 0, r, iter->second );
			inout.RemoveTapNote( t, iter );
		}
}

void NoteDataUtil::CollapseLeft( NoteData &inout )
{
	FOREACH_NONEMPTY_ROW_ALL_TRACKS( inout, r )
	{
		int iNumTracksFilled = 0;
		for( int t=0; t<inout.GetNumTracks(); t++ )
		{
			if( inout.GetTapNote(t,r).type != TapNote::empty )
			{
				TapNote tn = inout.GetTapNote(t,r);
				inout.SetTapNote(t, r, TAP_EMPTY);
				if( iNumTracksFilled < inout.GetNumTracks() )
				{
					inout.SetTapNote(iNumTracksFilled, r, tn);
					++iNumTracksFilled;
				}
			}
		}
	}
}

void NoteDataUtil::ShiftTracks( NoteData &inout, int iShiftBy )
{
	int iOriginalTrackToTakeFrom[MAX_NOTE_TRACKS];
	for( int i = 0; i < inout.GetNumTracks(); ++i )
	{
		int iFrom = i-iShiftBy;
		wrap( iFrom, inout.GetNumTracks() );
		iOriginalTrackToTakeFrom[i] = iFrom;
	}

	NoteData orig( inout );
	inout.LoadTransformed( orig, orig.GetNumTracks(), iOriginalTrackToTakeFrom );
}

void NoteDataUtil::ShiftLeft( NoteData &inout )
{
	ShiftTracks( inout, -1 );
}

void NoteDataUtil::ShiftRight( NoteData &inout )
{
	ShiftTracks( inout, +1 );
}


struct ValidRow
{
	StepsType st;
	bool bValidMask[MAX_NOTE_TRACKS];
};
#define T true
#define f false
const ValidRow g_ValidRows[] = 
{
	{ StepsType_dance_double, { T,T,T,T,f,f,f,f } },
	{ StepsType_dance_double, { f,T,T,T,T,f,f,f } },
	{ StepsType_dance_double, { f,f,f,T,T,T,T,f } },
	{ StepsType_dance_double, { f,f,f,f,T,T,T,T } },
	{ StepsType_pump_double, { T,T,T,T,T,f,f,f,f,f } },
	{ StepsType_pump_double, { f,f,T,T,T,T,T,T,f,f } },
	{ StepsType_pump_double, { f,f,f,f,f,T,T,T,T,T } },
};
#undef T
#undef f

void NoteDataUtil::RemoveStretch( NoteData &inout, StepsType st, int iStartIndex, int iEndIndex )
{
	vector<const ValidRow*> vpValidRowsToCheck;
	for( unsigned i=0; i<ARRAYLEN(g_ValidRows); i++ )
	{
		if( g_ValidRows[i].st == st )
			vpValidRowsToCheck.push_back( &g_ValidRows[i] );
	}

	// bail early if there's nothing to validate against
	if( vpValidRowsToCheck.empty() )
		return;

	// each row must pass at least one valid mask
	FOREACH_NONEMPTY_ROW_ALL_TRACKS_RANGE( inout, r, iStartIndex, iEndIndex )
	{
		// only check rows with jumps
		if( inout.GetNumTapNonEmptyTracks(r) < 2 )
			continue;

		bool bPassedOneMask = false;
		for( unsigned i=0; i<vpValidRowsToCheck.size(); i++ )
		{
			const ValidRow &vr = *vpValidRowsToCheck[i];
			if( NoteDataUtil::RowPassesValidMask(inout,r,vr.bValidMask) )
			{
				bPassedOneMask = true;
				break;
			}
		}

		if( !bPassedOneMask )
			RemoveAllButOneTap( inout, r );
	}
}

bool NoteDataUtil::RowPassesValidMask( NoteData &inout, int row, const bool bValidMask[] )
{
	for( int t=0; t<inout.GetNumTracks(); t++ )
	{
		if( !bValidMask[t] && inout.GetTapNote(t,row).type != TapNote::empty )
			return false;
	}

	return true;
}

void NoteDataUtil::ConvertAdditionsToRegular( NoteData &inout )
{
	for( int t=0; t<inout.GetNumTracks(); t++ )
		FOREACH_NONEMPTY_ROW_IN_TRACK( inout, t, r )
			if( inout.GetTapNote(t,r).source == TapNote::addition )
			{
				TapNote tn = inout.GetTapNote(t,r);
				tn.source = TapNote::original;
				inout.SetTapNote( t, r, tn );
			}
}

void NoteDataUtil::TransformNoteData( NoteData &nd, const AttackArray &aa, StepsType st, Song* pSong )
{
	FOREACH_CONST( Attack, aa, a )
	{
		PlayerOptions po;
		po.FromString( a->sModifiers );
		if( po.ContainsTransformOrTurn() )
		{
			float fStartBeat, fEndBeat;
			a->GetAttackBeats( pSong, fStartBeat, fEndBeat );

			NoteDataUtil::TransformNoteData( nd, po, st, BeatToNoteRow(fStartBeat), BeatToNoteRow(fEndBeat) );
		}
	}
}

void NoteDataUtil::TransformNoteData( NoteData &nd, const PlayerOptions &po, StepsType st, int iStartIndex, int iEndIndex )
{
	// Apply remove transforms before others so that we don't go removing
	// notes we just inserted.  Apply TRANSFORM_NOROLLS before TRANSFORM_NOHOLDS,
	// since NOROLLS creates holds.
	if( po.m_bTransforms[PlayerOptions::TRANSFORM_LITTLE] )		NoteDataUtil::Little( nd, iStartIndex, iEndIndex );
	if( po.m_bTransforms[PlayerOptions::TRANSFORM_NOROLLS] )	NoteDataUtil::ChangeRollsToHolds( nd, iStartIndex, iEndIndex );
	if( po.m_bTransforms[PlayerOptions::TRANSFORM_NOHOLDS] )	NoteDataUtil::RemoveHoldNotes( nd, iStartIndex, iEndIndex );
	if( po.m_bTransforms[PlayerOptions::TRANSFORM_NOMINES] )	NoteDataUtil::RemoveMines( nd, iStartIndex, iEndIndex );
	if( po.m_bTransforms[PlayerOptions::TRANSFORM_NOJUMPS] )	NoteDataUtil::RemoveJumps( nd, iStartIndex, iEndIndex );
	if( po.m_bTransforms[PlayerOptions::TRANSFORM_NOLIFTS] )	NoteDataUtil::RemoveLifts( nd, iStartIndex, iEndIndex );
	if( po.m_bTransforms[PlayerOptions::TRANSFORM_NOFAKES] )	NoteDataUtil::RemoveFakes( nd, iStartIndex, iEndIndex );
	if( po.m_bTransforms[PlayerOptions::TRANSFORM_NOHANDS] )	NoteDataUtil::RemoveHands( nd, iStartIndex, iEndIndex );
	if( po.m_bTransforms[PlayerOptions::TRANSFORM_NOQUADS] )	NoteDataUtil::RemoveQuads( nd, iStartIndex, iEndIndex );
	if( po.m_bTransforms[PlayerOptions::TRANSFORM_NOSTRETCH] )	NoteDataUtil::RemoveStretch( nd, st, iStartIndex, iEndIndex );

	// Apply inserts.
	if( po.m_bTransforms[PlayerOptions::TRANSFORM_BIG] )		NoteDataUtil::Big( nd, iStartIndex, iEndIndex );
	if( po.m_bTransforms[PlayerOptions::TRANSFORM_QUICK] )		NoteDataUtil::Quick( nd, iStartIndex, iEndIndex );
	if( po.m_bTransforms[PlayerOptions::TRANSFORM_BMRIZE] )		NoteDataUtil::BMRize( nd, iStartIndex, iEndIndex );

	// Skippy will still add taps to places that the other 
	// AddIntelligentTaps above won't.
	if( po.m_bTransforms[PlayerOptions::TRANSFORM_SKIPPY] )		NoteDataUtil::Skippy( nd, iStartIndex, iEndIndex );

	// These aren't affects by the above inserts very much.
	if( po.m_bTransforms[PlayerOptions::TRANSFORM_MINES] )		NoteDataUtil::AddMines( nd, iStartIndex, iEndIndex );
	if( po.m_bTransforms[PlayerOptions::TRANSFORM_ECHO] )		NoteDataUtil::Echo( nd, iStartIndex, iEndIndex );

	// Jump-adding transforms aren't much affected by additional taps.
	if( po.m_bTransforms[PlayerOptions::TRANSFORM_WIDE] )		NoteDataUtil::Wide( nd, iStartIndex, iEndIndex );
	if( po.m_bTransforms[PlayerOptions::TRANSFORM_STOMP] )		NoteDataUtil::Stomp( nd, st, iStartIndex, iEndIndex );

	// Transforms that add holds go last.  If they went first, most tap-adding 
	// transforms wouldn't do anything because tap-adding transforms skip areas 
	// where there's a hold.
	if( po.m_bTransforms[PlayerOptions::TRANSFORM_PLANTED] )	NoteDataUtil::Planted( nd, iStartIndex, iEndIndex );
	if( po.m_bTransforms[PlayerOptions::TRANSFORM_FLOORED] )	NoteDataUtil::Floored( nd, iStartIndex, iEndIndex );
	if( po.m_bTransforms[PlayerOptions::TRANSFORM_TWISTER] )	NoteDataUtil::Twister( nd, iStartIndex, iEndIndex );

	// Do this here to turn any added holds into rolls
	if( po.m_bTransforms[PlayerOptions::TRANSFORM_HOLDROLLS] )	NoteDataUtil::ChangeHoldsToRolls( nd, iStartIndex, iEndIndex );

	// Apply turns and shuffles last so that they affect inserts.
	if( po.m_bTurns[PlayerOptions::TURN_MIRROR] )			NoteDataUtil::Turn( nd, st, NoteDataUtil::mirror, iStartIndex, iEndIndex );
	if( po.m_bTurns[PlayerOptions::TURN_BACKWARDS] )	NoteDataUtil::Turn( nd, st, NoteDataUtil::backwards, iStartIndex, iEndIndex );
	if( po.m_bTurns[PlayerOptions::TURN_LEFT] )			NoteDataUtil::Turn( nd, st, NoteDataUtil::left, iStartIndex, iEndIndex );
	if( po.m_bTurns[PlayerOptions::TURN_RIGHT] )			NoteDataUtil::Turn( nd, st, NoteDataUtil::right, iStartIndex, iEndIndex );
	if( po.m_bTurns[PlayerOptions::TURN_SHUFFLE] )			NoteDataUtil::Turn( nd, st, NoteDataUtil::shuffle, iStartIndex, iEndIndex );
	if( po.m_bTurns[PlayerOptions::TURN_SOFT_SHUFFLE] )			NoteDataUtil::Turn( nd, st, NoteDataUtil::soft_shuffle, iStartIndex, iEndIndex );
	if( po.m_bTurns[PlayerOptions::TURN_SUPER_SHUFFLE] )		NoteDataUtil::Turn( nd, st, NoteDataUtil::super_shuffle, iStartIndex, iEndIndex );
}

void NoteDataUtil::AddTapAttacks( NoteData &nd, Song* pSong )
{
	// throw an attack in every 30 seconds

	const char* szAttacks[3] =
	{
		"2x",
		"drunk",
		"dizzy",
	};

	for( float sec=15; sec<pSong->m_fMusicLengthSeconds; sec+=30 )
	{
		float fBeat = pSong->m_SongTiming.GetBeatFromElapsedTime( sec );
		int iBeat = (int)fBeat;
		int iTrack = iBeat % nd.GetNumTracks();	// deterministically calculates track
		TapNote tn(
			TapNote::attack,
			TapNote::SubType_Invalid,
			TapNote::original, 
			szAttacks[RandomInt(ARRAYLEN(szAttacks))],
			15.0f, 
			-1 );
		nd.SetTapNote( iTrack, BeatToNoteRow(fBeat), tn );
	}
}

void NoteDataUtil::Scale( NoteData &nd, float fScale )
{
	ASSERT( fScale > 0 );
	
	NoteData ndOut;
	ndOut.SetNumTracks( nd.GetNumTracks() );
	
	for( int t=0; t<nd.GetNumTracks(); t++ )
	{
		for( NoteData::const_iterator iter = nd.begin(t); iter != nd.end(t); ++iter )
		{
			TapNote tn = iter->second;
			int iNewRow      = lrintf( fScale * iter->first );
			int iNewDuration = lrintf( fScale * (iter->first + tn.iDuration) );
			tn.iDuration = iNewDuration;
			ndOut.SetTapNote( t, iNewRow, tn );
		}
	}
	
	nd.swap( ndOut );
}

/* XXX: move this to an appropriate place, same place as NoteRowToBeat perhaps? */
static inline int GetScaledRow( float fScale, int iStartIndex, int iEndIndex, int iRow )
{
	if( iRow < iStartIndex )
		return iRow;
	else if( iRow > iEndIndex )
		return iRow + lrintf( (iEndIndex - iStartIndex) * (fScale - 1) );
	else
		return lrintf( (iRow - iStartIndex) * fScale ) + iStartIndex;
}

void NoteDataUtil::ScaleRegion( NoteData &nd, float fScale, int iStartIndex, int iEndIndex )
{
	ASSERT( fScale > 0 );
	ASSERT( iStartIndex < iEndIndex );
	ASSERT( iStartIndex >= 0 );
	
	NoteData ndOut;
	ndOut.SetNumTracks( nd.GetNumTracks() );
	
	for( int t=0; t<nd.GetNumTracks(); t++ )
	{
		for( NoteData::const_iterator iter = nd.begin(t); iter != nd.end(t); ++iter )
		{
			TapNote tn = iter->second;
			int iNewRow      = GetScaledRow( fScale, iStartIndex, iEndIndex, iter->first );
			int iNewDuration = GetScaledRow( fScale, iStartIndex, iEndIndex, iter->first + tn.iDuration ) - iNewRow;
			tn.iDuration = iNewDuration;
			ndOut.SetTapNote( t, iNewRow, tn );
		}
	}
	
	nd.swap( ndOut );
}

void NoteDataUtil::InsertRows( NoteData &nd, int iStartIndex, int iRowsToAdd )
{
	ASSERT( iRowsToAdd >= 0 );

	NoteData temp;
	temp.SetNumTracks( nd.GetNumTracks() );
	temp.CopyRange( nd, iStartIndex, MAX_NOTE_ROW );
	nd.ClearRange( iStartIndex, MAX_NOTE_ROW );
	nd.CopyRange( temp, 0, MAX_NOTE_ROW, iStartIndex + iRowsToAdd );		
}

void NoteDataUtil::DeleteRows( NoteData &nd, int iStartIndex, int iRowsToDelete )
{
	ASSERT( iRowsToDelete >= 0 );

	NoteData temp;
	temp.SetNumTracks( nd.GetNumTracks() );
	temp.CopyRange( nd, iStartIndex + iRowsToDelete, MAX_NOTE_ROW );
	nd.ClearRange( iStartIndex, MAX_NOTE_ROW );
	nd.CopyRange( temp, 0, MAX_NOTE_ROW, iStartIndex );		
}

void NoteDataUtil::RemoveAllTapsOfType( NoteData& ndInOut, TapNote::Type typeToRemove )
{
	/* Be very careful when deleting the tap notes. Erasing elements from maps using
	 * iterators invalidates only the iterator that is being erased. To that end,
	 * increment the iterator before deleting the elment of the map.
	 */
	for( int t=0; t<ndInOut.GetNumTracks(); t++ )
	{
		for( NoteData::iterator iter = ndInOut.begin(t); iter != ndInOut.end(t); )
		{
			if( iter->second.type == typeToRemove )
				ndInOut.RemoveTapNote( t, iter++ );
			else
				++iter;
		}
	}
}

void NoteDataUtil::RemoveAllTapsExceptForType( NoteData& ndInOut, TapNote::Type typeToKeep )
{
	/* Same as in RemoveAllTapsOfType(). */
	for( int t=0; t<ndInOut.GetNumTracks(); t++ )
	{
		for( NoteData::iterator iter = ndInOut.begin(t); iter != ndInOut.end(t); )
		{
			if( iter->second.type != typeToKeep )
				ndInOut.RemoveTapNote( t, iter++ );
			else
				++iter;
		}
	}
}

int NoteDataUtil::GetMaxNonEmptyTrack( const NoteData& in )
{
	for( int t=in.GetNumTracks()-1; t>=0; t-- )
		if( !in.IsTrackEmpty( t ) )
			return t;
	return -1;
}

bool NoteDataUtil::AnyTapsAndHoldsInTrackRange( const NoteData& in, int iTrack, int iStart, int iEnd )
{
	if( iStart >= iEnd )
		return false;

	// for each index we crossed since the last update:
	FOREACH_NONEMPTY_ROW_IN_TRACK_RANGE( in, iTrack, r, iStart, iEnd )
	{
		switch( in.GetTapNote( iTrack, r ).type )
		{
		case TapNote::empty:
		case TapNote::mine:
			continue;
		default:
			return true;
		}
	}

	if( in.IsHoldNoteAtRow( iTrack, iEnd ) )
		return true;

	return false;
}

/* Find the next row that either starts a TapNote, or ends a previous one. */
bool NoteDataUtil::GetNextEditorPosition( const NoteData& in, int &rowInOut )
{
	int iOriginalRow = rowInOut;
	bool bAnyHaveNextNote = in.GetNextTapNoteRowForAllTracks( rowInOut );

	int iClosestNextRow = rowInOut;
	if( !bAnyHaveNextNote )
		iClosestNextRow = MAX_NOTE_ROW;

	for( int t=0; t<in.GetNumTracks(); t++ )
	{
		int iHeadRow;
		if( !in.IsHoldHeadOrBodyAtRow(t, iOriginalRow, &iHeadRow) )
			continue;

		const TapNote &tn = in.GetTapNote( t, iHeadRow );
		int iEndRow = iHeadRow + tn.iDuration;
		if( iEndRow == iOriginalRow )
			continue;

		bAnyHaveNextNote = true;
		ASSERT( iEndRow < MAX_NOTE_ROW );
		iClosestNextRow = min( iClosestNextRow, iEndRow );
	}

	if( !bAnyHaveNextNote )
		return false;

	rowInOut = iClosestNextRow;
	return true;
}

bool NoteDataUtil::GetPrevEditorPosition( const NoteData& in, int &rowInOut )
{
	int iOriginalRow = rowInOut;
	bool bAnyHavePrevNote = in.GetPrevTapNoteRowForAllTracks( rowInOut );

	int iClosestPrevRow = rowInOut;
	for( int t=0; t<in.GetNumTracks(); t++ )
	{
		int iHeadRow = iOriginalRow;
		if( !in.GetPrevTapNoteRowForTrack(t, iHeadRow) )
			continue;

		const TapNote &tn = in.GetTapNote( t, iHeadRow );
		if( tn.type != TapNote::hold_head )
			continue;

		int iEndRow = iHeadRow + tn.iDuration;
		if( iEndRow >= iOriginalRow )
			continue;

		bAnyHavePrevNote = true;
		ASSERT( iEndRow < MAX_NOTE_ROW );
		iClosestPrevRow = max( iClosestPrevRow, iEndRow );
	}

	if( !bAnyHavePrevNote )
		return false;

	rowInOut = iClosestPrevRow;
	return true;
}

extern Preference<float> g_fTimingWindowHopo;


void NoteDataUtil::SetHopoPossibleFlags( const Song *pSong, NoteData& ndInOut )
{
	float fLastRowMusicSeconds = -1;
	int iLastTapTrackOfLastRow = -1;
	FOREACH_NONEMPTY_ROW_ALL_TRACKS( ndInOut, r )
	{
		float fBeat = NoteRowToBeat( r );
		float fSeconds = pSong->m_SongTiming.GetElapsedTimeFromBeat( fBeat );

		int iLastTapTrack = ndInOut.GetLastTrackWithTapOrHoldHead( r );
		if( iLastTapTrack != -1  &&  fSeconds <= fLastRowMusicSeconds + g_fTimingWindowHopo )
		{
			int iNumNotesInRow = ndInOut.GetNumTapNotesInRow( r );
			TapNote &tn = ndInOut.FindTapNote( iLastTapTrack, r )->second;
		
			if( iNumNotesInRow == 1  &&  iLastTapTrack != iLastTapTrackOfLastRow )
			{
				tn.bHopoPossible = true;
			}
		}

		fLastRowMusicSeconds = fSeconds;
		iLastTapTrackOfLastRow = iLastTapTrack;
	}
}


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
