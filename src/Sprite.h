#ifndef SPRITE_H
#define SPRITE_H

#include "Actor.h"
#include "RageTextureID.h"

void TexCoordArrayFromRect( float fImageCoords[8], const RectF &rect );

class RageTexture;
/** @brief A bitmap Actor that animates and moves around. */
class Sprite: public Actor
{
public:
	Sprite();
	Sprite( const Sprite &cpy );
	virtual ~Sprite();
	
	virtual void InitState();

	void LoadFromNode( const XNode* pNode );
	virtual Sprite *Copy() const;

	virtual bool EarlyAbortDraw() const;
	virtual void DrawPrimitives();
	virtual void Update( float fDeltaTime );

	void UpdateAnimationState();	// take m_fSecondsIntoState, and move to a new state

	// Adjust texture properties for song backgrounds.
	static RageTextureID SongBGTexture( RageTextureID ID );

	// Adjust texture properties for song banners.
	static RageTextureID SongBannerTexture( RageTextureID ID );

	virtual void Load( RageTextureID ID );
	void SetTexture( RageTexture *pTexture );

	void UnloadTexture();
	RageTexture* GetTexture() { return m_pTexture; };

	virtual void EnableAnimation( bool bEnable );

	virtual int GetNumStates() const;
	virtual void SetState( int iNewState );
	int GetState() { return m_iCurState; }
	virtual float GetAnimationLengthSeconds() const;
	virtual void SetSecondsIntoAnimation( float fSeconds );

	RString	GetTexturePath() const;

	void SetCustomTextureRect( const RectF &new_texcoord_frect );
	void SetCustomTextureCoords( float fTexCoords[8] );
	void SetCustomImageRect( RectF rectImageCoords );	// in image pixel space
	void SetCustomImageCoords( float fImageCoords[8] );
	const RectF *GetCurrentTextureCoordRect() const;
	const RectF *GetTextureCoordRectForState( int iState ) const;
	void StopUsingCustomCoords();
	void GetActiveTextureCoords(float fTexCoordsOut[8]) const;
	void StretchTexCoords( float fX, float fY );
	void AddImageCoords( float fX, float fY ); // in image pixel space
	void SetEffectMode( EffectMode em ) { m_EffectMode = em; }

	void SetTexCoordVelocity(float fVelX, float fVelY);
	/**
	 * @brief Scale the Sprite while maintaining the aspect ratio.
	 *
	 * It has to fit within and become clipped to the given parameters.
	 * @param fWidth the new width.
	 * @param fHeight the new height. */
	void ScaleToClipped( float fWidth, float fHeight );
	void CropTo( float fWidth, float fHeight );

	// Commands
	virtual void PushSelf( lua_State *L );

	void SetAllStateDelays(float fDelay);

protected:
	void LoadFromTexture( RageTextureID ID );

private:
	void LoadStatesFromTexture();

	void DrawTexture( const TweenState *state );

	RageTexture* m_pTexture;

	/** @brief The Sprite's present state. */
	struct State
	{
		RectF rect;
		/** @brief The number of "seconds to show". */
		float fDelay;
	};
	vector<State> m_States;
	int		m_iCurState;
	/** @brief The number of seconds that have elapsed since we switched to this frame. */
	float	m_fSecsIntoState;

	EffectMode m_EffectMode;
	bool m_bUsingCustomTexCoords;
	bool m_bSkipNextUpdate;
	/**
	 * @brief Set up the coordinates for the texture.
	 *
	 * The first two parameters are the (x, y) coordinates for the top left.
	 * The remaining six are for the (x, y) coordinates for bottom left,
	 * bottom right, and top right respectively. */
	float m_CustomTexCoords[8];

	// Remembered clipped dimensions are applied on Load().
	// -1 means no remembered dimensions;
	float	m_fRememberedClipWidth, m_fRememberedClipHeight;

	float m_fTexCoordVelocityX;
	float m_fTexCoordVelocityY;
};

#endif

/**
 * @file
 * @author Chris Danford (c) 2001-2004
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
