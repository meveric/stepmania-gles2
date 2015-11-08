#ifndef GameplayAssist_H
#define GameplayAssist_H

#include "RageSound.h"
#include "PlayerState.h"

class NoteData;
/** @brief The handclaps and metronomes ready to assist the player. */
class GameplayAssist
{
public:
	/** @brief Load the sounds. */
	void Init();
	/**
	 * @brief Play the sounds in question for the particular chart.
	 * @param nd the note data used for playing the ticks.
	 * @param ps the player's state (and number) for Split Timing. */
	void PlayTicks( const NoteData &nd, const PlayerState *ps );
	/** @brief Stop playing the sounds. */
	void StopPlaying();
private:
	/** @brief the sound made when a note is to be hit. */
	RageSound	m_soundAssistClap;
	/** @brief the sound made when crossing a new measure. */
	RageSound	m_soundAssistMetronomeMeasure;
	/** @brief the sound made when crossing a new beat. */
	RageSound	m_soundAssistMetronomeBeat;

};

#endif

/**
 * @file
 * @author Chris Danford (c) 2003-2006
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
