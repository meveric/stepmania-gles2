/* pos_map_queue - A container that maps one set of frame numbers to another. */

#ifndef RAGE_SOUND_POS_MAP_H
#define RAGE_SOUND_POS_MAP_H

struct pos_map_impl;
class pos_map_queue
{
public:
	pos_map_queue();
	~pos_map_queue();
	pos_map_queue( const pos_map_queue &cpy );
	pos_map_queue &operator=( const pos_map_queue &rhs );

	/* Insert a mapping from iSourceFrame to iDestFrame, containing iFrames. */
	void Insert( int64_t iSourceFrame, int iFrames, int64_t iDestFrame, float fSourceToDestRatio = 1.0f );

	/* Return the iDestFrame for the given iSourceFrame. */
	int64_t Search( int64_t iSourceFrame, bool *bApproximate ) const;

	/* Erase all mappings. */
	void Clear();

	bool IsEmpty() const;

private:
	pos_map_impl *m_pImpl;
};

#endif

/*
 * Copyright (c) 2002-2004 Glenn Maynard
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
