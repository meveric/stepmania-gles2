/** @brief ScreenDimensions - defines for screen resolutions. */

#ifndef SCREEN_DIMENSIONS_H
#define SCREEN_DIMENSIONS_H

namespace ScreenDimensions
{
	float GetThemeAspectRatio();
	float GetScreenWidth();
	float GetScreenHeight();
	void ReloadScreenDimensions();
};

#define SCREEN_WIDTH	ScreenDimensions::GetScreenWidth()
#define SCREEN_HEIGHT	ScreenDimensions::GetScreenHeight()

#define	SCREEN_LEFT	(0)
#define	SCREEN_RIGHT	(SCREEN_WIDTH)
#define	SCREEN_TOP	(0)
#define	SCREEN_BOTTOM	(SCREEN_HEIGHT)

#define	SCREEN_CENTER_X	(SCREEN_LEFT + (SCREEN_RIGHT - SCREEN_LEFT)/2.0f)
#define	SCREEN_CENTER_Y	(SCREEN_TOP + (SCREEN_BOTTOM - SCREEN_TOP)/2.0f)

#define THEME_NATIVE_ASPECT (THEME_SCREEN_WIDTH/THEME_SCREEN_HEIGHT)
#define ASPECT_SCALE_FACTOR ((SCREEN_WIDTH/SCREEN_HEIGHT)/THEME_NATIVE_ASPECT)

#define FullScreenRectF RectF(SCREEN_LEFT,SCREEN_TOP,SCREEN_RIGHT,SCREEN_BOTTOM)

/**
 * @brief The size of the arrows.
 *
 * This is referenced in ArrowEffects, GameManager, NoteField, and SnapDisplay.
 * XXX: doesn't always have to be 64. -aj
 */
#define	ARROW_SIZE	(64)

#endif

/**
 * @file
 * @author Chris Danford (c) 2001-2002
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
