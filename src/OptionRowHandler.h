#ifndef OptionRowHandler_H
#define OptionRowHandler_H

#include "GameCommand.h"
#include "LuaReference.h"
#include "RageUtil.h"
#include <set>
struct MenuRowDef;
class OptionRow;
struct ConfOption;
/** @brief How many options can be selected on this row? */
enum SelectType
{
	SELECT_ONE, /**< Only one option can be chosen on this row. */
	SELECT_MULTIPLE, /**< Multiple options can be chosen on this row. */
	SELECT_NONE, /**< No options can be chosen on this row. */
	NUM_SelectType,
	SelectType_Invalid
};
const RString& SelectTypeToString( SelectType pm );
SelectType StringToSelectType( const RString& s );
LuaDeclareType( SelectType );
/** @brief How many items are shown on the row? */
enum LayoutType
{
	LAYOUT_SHOW_ALL_IN_ROW, /**< All of the options are shown at once. */
	LAYOUT_SHOW_ONE_IN_ROW, /**< Only one option is shown at a time. */
	NUM_LayoutType,
	LayoutType_Invalid
};
const RString& LayoutTypeToString( LayoutType pm );
LayoutType StringToLayoutType( const RString& s );
LuaDeclareType( LayoutType );

/** @brief Define the purpose of the OptionRow. */
struct OptionRowDefinition
{
	/** @brief the name of the option row. */
	RString m_sName;
	/** @brief an explanation of the row's purpose. */
	RString m_sExplanationName;
	/** @brief Do all players have to share one option from the row? */
	bool m_bOneChoiceForAllPlayers;
	SelectType m_selectType;
	LayoutType m_layoutType;
	vector<RString> m_vsChoices;
	set<PlayerNumber> m_vEnabledForPlayers;	// only players in this set may change focus to this row
	int m_iDefault;
	bool	m_bExportOnChange;
	/**
	 * @brief Are theme items allowed here?
	 *
	 * This should be true for dynamic strings. */
	bool	m_bAllowThemeItems;
	/**
	 * @brief Are theme titles allowed here?
	 *
	 * This should be true for dynamic strings. */
	bool	m_bAllowThemeTitle;
	/**
	 * @brief Are explanations allowed for this row?
	 *
	 * If this is false, it will ignore the ScreenOptions::SHOW_EXPLANATIONS metric.
	 *
	 * This should be true for dynamic strings. */
	bool	m_bAllowExplanation;
	bool	m_bShowChoicesListOnSelect; // (currently unused)

	/**
	 * @brief Is this option enabled for the Player?
	 * @param pn the Player the PlayerNumber represents.
	 * @return true if the option is enabled, false otherwise. */
	bool IsEnabledForPlayer( PlayerNumber pn ) const 
	{
		return m_vEnabledForPlayers.find(pn) != m_vEnabledForPlayers.end(); 
	}

	OptionRowDefinition(): m_sName(""), m_sExplanationName(""),
		m_bOneChoiceForAllPlayers(false), m_selectType(SELECT_ONE),
		m_layoutType(LAYOUT_SHOW_ALL_IN_ROW), m_vsChoices(), 
		m_vEnabledForPlayers(), m_iDefault(-1),
		m_bExportOnChange(false), m_bAllowThemeItems(true),
		m_bAllowThemeTitle(true), m_bAllowExplanation(true),
		m_bShowChoicesListOnSelect(false)
	{
		FOREACH_PlayerNumber( pn )
			m_vEnabledForPlayers.insert( pn ); 
	}
	void Init()
	{
		m_sName = "";
		m_sExplanationName = "";
		m_bOneChoiceForAllPlayers = false;
		m_selectType = SELECT_ONE;
		m_layoutType = LAYOUT_SHOW_ALL_IN_ROW;
		m_vsChoices.clear();
		m_vEnabledForPlayers.clear();
		FOREACH_PlayerNumber( pn )
			m_vEnabledForPlayers.insert( pn );
		m_iDefault = -1;
		m_bExportOnChange = false;
		m_bAllowThemeItems = true;
		m_bAllowThemeTitle = true;
		m_bAllowExplanation = true;
		m_bShowChoicesListOnSelect = false;
	}

	OptionRowDefinition( const char *n, bool b, const char *c0=NULL, 
			    const char *c1=NULL, const char *c2=NULL, 
			    const char *c3=NULL, const char *c4=NULL, 
			    const char *c5=NULL, const char *c6=NULL, 
			    const char *c7=NULL, const char *c8=NULL, 
			    const char *c9=NULL, const char *c10=NULL, 
			    const char *c11=NULL, const char *c12=NULL, 
			    const char *c13=NULL, const char *c14=NULL, 
			    const char *c15=NULL, const char *c16=NULL, 
			    const char *c17=NULL, const char *c18=NULL, 
			    const char *c19=NULL ): m_sName(n),
		m_sExplanationName(""), m_bOneChoiceForAllPlayers(b),
		m_selectType(SELECT_ONE),
		m_layoutType(LAYOUT_SHOW_ALL_IN_ROW), m_vsChoices(), 
		m_vEnabledForPlayers(), m_iDefault(-1),
		m_bExportOnChange(false), m_bAllowThemeItems(true),
		m_bAllowThemeTitle(true), m_bAllowExplanation(true),
		m_bShowChoicesListOnSelect(false)
	{
		FOREACH_PlayerNumber( pn )
			m_vEnabledForPlayers.insert( pn );
		
#define PUSH( c )	if(c) m_vsChoices.push_back(c);
		PUSH(c0);PUSH(c1);PUSH(c2);PUSH(c3);PUSH(c4);PUSH(c5);
		PUSH(c6);PUSH(c7);PUSH(c8);PUSH(c9);PUSH(c10);PUSH(c11);
		PUSH(c12);PUSH(c13);PUSH(c14);PUSH(c15);PUSH(c16);PUSH(c17);
		PUSH(c18);PUSH(c19);
#undef PUSH
	}
};

/** @brief Shows PlayerOptions and SongOptions in icon form. */
class OptionRowHandler
{
public:
	OptionRowDefinition m_Def;
	vector<RString> m_vsReloadRowMessages;	// refresh this row on on these messages

	OptionRowHandler(): m_Def(), m_vsReloadRowMessages() { }
	virtual ~OptionRowHandler() { }
	virtual void Init()
	{
		m_Def.Init();
		m_vsReloadRowMessages.clear();
	}
	void Load( const Commands &cmds )
	{
		Init();
		this->LoadInternal( cmds );
	}
	RString OptionTitle() const;
	RString GetThemedItemText( int iChoice ) const;

	virtual void LoadInternal( const Commands & ) { }

	/* We may re-use OptionRowHandlers. This is called before each use. If the
	 * contents of the row are dependent on external state (for example, the
	 * current song), clear the row contents and reinitialize them. As an
	 * optimization, rows which do not change can be initialized just once and
	 * left alone.
	 * If the row has been reinitialized, return RELOAD_CHANGED_ALL, and the
	 * graphic elements will also be reinitialized. If only m_vEnabledForPlayers
	 * has been changed, return RELOAD_CHANGED_ENABLED. If the row is static, and
	 * nothing has changed, return RELOAD_CHANGED_NONE. */
	enum ReloadChanged { RELOAD_CHANGED_NONE, RELOAD_CHANGED_ENABLED, RELOAD_CHANGED_ALL };
	virtual ReloadChanged Reload() { return RELOAD_CHANGED_NONE; }

	virtual int GetDefaultOption() const { return -1; }
	virtual void ImportOption( OptionRow *, const vector<PlayerNumber> &, vector<bool> vbSelectedOut[NUM_PLAYERS] ) const { }
	// Returns an OPT mask.
	virtual int ExportOption( const vector<PlayerNumber> &, const vector<bool> vbSelected[NUM_PLAYERS] ) const { return 0; }
	virtual void GetIconTextAndGameCommand( int iFirstSelection, RString &sIconTextOut, GameCommand &gcOut ) const;
	virtual RString GetScreen( int /* iChoice */ ) const { return RString(); }
};

/** @brief Utilities for the OptionRowHandlers. */
namespace OptionRowHandlerUtil
{
	OptionRowHandler* Make( const Commands &cmds );
	OptionRowHandler* MakeNull();
	OptionRowHandler* MakeSimple( const MenuRowDef &mrd );

	void SelectExactlyOne( int iSelection, vector<bool> &vbSelectedOut );
	int GetOneSelection( const vector<bool> &vbSelected );
}

inline void VerifySelected( SelectType st, const vector<bool> &vbSelected, const RString &sName )
{
	int iNumSelected = 0;
	if( st == SELECT_ONE )
	{
		ASSERT_M( vbSelected.size() > 0, ssprintf("%s: %i/%i", sName.c_str(), iNumSelected, int(vbSelected.size())) );
		for( unsigned e = 0; e < vbSelected.size(); ++e )
			if( vbSelected[e] )
				iNumSelected++;
		ASSERT_M( iNumSelected == 1, ssprintf("%s: %i/%i", sName.c_str(), iNumSelected, int(vbSelected.size())) );
	}
}

#endif

/**
 * @file
 * @author Chris Danford (c) 2002-2004
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
