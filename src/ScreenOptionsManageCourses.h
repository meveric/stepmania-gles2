#ifndef ScreenOptionsManageCourses_H
#define ScreenOptionsManageCourses_H

#include "ScreenOptions.h"
#include "GameConstantsAndTypes.h"
#include "RageSound.h"

class Course;

class ScreenOptionsManageCourses : public ScreenOptions
{
public:
	void Init();
	virtual void BeginScreen();
	virtual void HandleScreenMessage( const ScreenMessage SM );
	virtual bool MenuSelect( const InputEventPlus &input );

protected:
	virtual void ImportOptions( int iRow, const vector<PlayerNumber> &vpns );
	virtual void ExportOptions( int iRow, const vector<PlayerNumber> &vpns );
	
	virtual void AfterChangeRow( PlayerNumber pn );
	virtual void ProcessMenuStart( const InputEventPlus &input );

private:
	Course *GetCourseWithFocus() const;

	RageSound m_soundDifficultyChanged;
	vector<Course*> m_vpCourses;
	ThemeMetric<EditMode> EDIT_MODE;
	ThemeMetric<RString> CREATE_NEW_SCREEN;
};

#endif

/*
 * (c) 2003-2006 Chris Danford, Steve Checkoway
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
