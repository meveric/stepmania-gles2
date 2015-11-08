/* RageSoundReader_Filter - simplify the creation of filter RageSoundReaders. */

#ifndef RAGE_SOUND_READER_FILTER_H
#define RAGE_SOUND_READER_FILTER_H

#include "RageSoundReader.h"
#include "RageUtil_AutoPtr.h"
class RageSoundReader_Filter: public RageSoundReader
{
public:
	RageSoundReader_Filter( RageSoundReader *pSource ):
		m_pSource( pSource )
	{
	}

	virtual int GetLength() const { return m_pSource->GetLength(); }
	virtual int GetLength_Fast() const { return m_pSource->GetLength_Fast(); }
	virtual int SetPosition( int iFrame ) { return m_pSource->SetPosition( iFrame ); }
	virtual int Read( float *pBuf, int iFrames ) { return m_pSource->Read( pBuf, iFrames ); }
	virtual int GetSampleRate() const { return m_pSource->GetSampleRate(); }
	virtual unsigned GetNumChannels() const { return m_pSource->GetNumChannels(); }
	virtual bool SetProperty( const RString &sProperty, float fValue ) { return m_pSource->SetProperty( sProperty, fValue ); }
	virtual int GetNextSourceFrame() const { return m_pSource->GetNextSourceFrame(); }
	virtual float GetStreamToSourceRatio() const { return m_pSource->GetStreamToSourceRatio(); }
	virtual RageSoundReader *GetSource() { return &*m_pSource; }
	virtual RString GetError() const { return m_pSource->GetError(); }

protected:
	HiddenPtr<RageSoundReader> m_pSource;
};

#endif

/*
 * Copyright (c) 2006 Glenn Maynard
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
