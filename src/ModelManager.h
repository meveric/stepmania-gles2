#ifndef MODEL_MANAGER_H
#define MODEL_MANAGER_H

#include "RageModelGeometry.h"

#include <map>

struct ModelManagerPrefs
{
	bool m_bDelayedUnload;

	ModelManagerPrefs()
	{
		m_bDelayedUnload = false;
	}
	ModelManagerPrefs( bool bDelayedUnload )
	{
		m_bDelayedUnload = bDelayedUnload;
	}

	bool operator!=( const ModelManagerPrefs& rhs )
	{
		return 
			m_bDelayedUnload != rhs.m_bDelayedUnload;
	}
};
/**
 * @brief Class for loading and releasing textures.
 *
 * Funnily enough, the original documentation claimed this was an Interface. */
class ModelManager
{
public:
	ModelManager();
	~ModelManager();

	RageModelGeometry* LoadMilkshapeAscii( const RString& sFile, bool bNeedNormals );
	void UnloadModel( RageModelGeometry *m );
//	void ReloadAll();

	/**
	 * @brief Set up new preferences.
	 * @param prefs the new preferences to set up.
	 * @return true if the display needs to be reset, false otherwise. */
	bool SetPrefs( const ModelManagerPrefs& prefs );
	const ModelManagerPrefs& GetPrefs() { return m_Prefs; }

protected:

	std::map<RString, RageModelGeometry*> m_mapFileToGeometry;

	ModelManagerPrefs m_Prefs;
};

extern ModelManager*	MODELMAN;	// global and accessable from anywhere in our program

#endif

/**
 * @file
 * @author Chris Danford (c) 2003-2004
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
