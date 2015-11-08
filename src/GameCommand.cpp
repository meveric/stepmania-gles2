#include "global.h"
#include "GameCommand.h"
#include "RageUtil.h"
#include "RageLog.h"
#include "GameManager.h"
#include "GameState.h"
#include "AnnouncerManager.h"
#include "PlayerOptions.h"
#include "ProfileManager.h"
#include "Profile.h"
#include "StepMania.h"
#include "ScreenManager.h"
#include "PrefsManager.h"
#include "Game.h"
#include "Style.h"
#include "Foreach.h"
#include "arch/Dialog/Dialog.h"
#include "GameSoundManager.h"
#include "PlayerState.h"
#include "SongManager.h"
#include "Song.h"
#include "UnlockManager.h"
#include "LocalizedString.h"
#include "arch/ArchHooks/ArchHooks.h"
#include "ScreenPrompt.h"

static LocalizedString COULD_NOT_LAUNCH_BROWSER( "GameCommand", "Could not launch web browser." );

REGISTER_CLASS_TRAITS( GameCommand, new GameCommand(*pCopy) );

void GameCommand::Init()
{
	m_bApplyCommitsScreens = true;
	m_sName = "";
	m_sText = "";
	m_bInvalid = true;
	m_iIndex = -1;
	m_MultiPlayer = MultiPlayer_Invalid;
	m_pStyle = NULL;
	m_pm = PlayMode_Invalid;
	m_dc = Difficulty_Invalid;
	m_CourseDifficulty = Difficulty_Invalid;
	m_sPreferredModifiers = "";
	m_sStageModifiers = "";
	m_sAnnouncer = "";
	m_sScreen = "";
	m_LuaFunction.Unset();
	m_pSong = NULL;
	m_pSteps = NULL;
	m_pCourse = NULL;
	m_pTrail = NULL;
	m_pCharacter = NULL;
	m_SortOrder = SortOrder_Invalid;
	m_sSoundPath = "";
	m_vsScreensToPrepare.clear();
	m_iWeightPounds = -1;
	m_iGoalCalories = -1;
	m_GoalType = GoalType_Invalid;
	m_sProfileID = "";
	m_sUrl = "";
	m_bUrlExits = true;

	m_bInsertCredit = false;
	m_bClearCredits = false;
	m_bStopMusic = false;
	m_bApplyDefaultOptions = false;
	m_bFadeMusic = false;
	m_fMusicFadeOutVolume = -1.0f;
	m_fMusicFadeOutSeconds = -1.0f;
}

class SongOptions;
bool CompareSongOptions( const SongOptions &so1, const SongOptions &so2 );

bool GameCommand::DescribesCurrentModeForAllPlayers() const
{
	FOREACH_HumanPlayer( pn )
		if( !DescribesCurrentMode(pn) )
			return false;

	return true;
}

bool GameCommand::DescribesCurrentMode( PlayerNumber pn ) const
{
	if( m_pm != PlayMode_Invalid && GAMESTATE->m_PlayMode != m_pm )
		return false;
	if( m_pStyle && GAMESTATE->GetCurrentStyle() != m_pStyle )
		return false;
	// HACK: don't compare m_dc if m_pSteps is set.  This causes problems 
	// in ScreenSelectOptionsMaster::ImportOptions if m_PreferredDifficulty 
	// doesn't match the difficulty of m_pCurSteps.
	if( m_pSteps == NULL  &&  m_dc != Difficulty_Invalid )
	{
		// Why is this checking for all players?
		FOREACH_HumanPlayer( human )
			if( GAMESTATE->m_PreferredDifficulty[human] != m_dc )
				return false;
	}

	if( m_sAnnouncer != "" && m_sAnnouncer != ANNOUNCER->GetCurAnnouncerName() )
		return false;

	if( m_sPreferredModifiers != "" )
	{
		PlayerOptions po = GAMESTATE->m_pPlayerState[pn]->m_PlayerOptions.GetPreferred();
		SongOptions so = GAMESTATE->m_SongOptions.GetPreferred();
		po.FromString( m_sPreferredModifiers );
		so.FromString( m_sPreferredModifiers );

		if( po != GAMESTATE->m_pPlayerState[pn]->m_PlayerOptions.GetPreferred() )
			return false;
		if( so != GAMESTATE->m_SongOptions.GetPreferred() )
			return false;
	}
	if( m_sStageModifiers != "" )
	{
		PlayerOptions po = GAMESTATE->m_pPlayerState[pn]->m_PlayerOptions.GetStage();
		SongOptions so = GAMESTATE->m_SongOptions.GetStage();
		po.FromString( m_sStageModifiers );
		so.FromString( m_sStageModifiers );

		if( po != GAMESTATE->m_pPlayerState[pn]->m_PlayerOptions.GetStage() )
			return false;
		if( so != GAMESTATE->m_SongOptions.GetStage() )
			return false;
	}

	if( m_pSong && GAMESTATE->m_pCurSong.Get() != m_pSong )
		return false;
	if( m_pSteps && GAMESTATE->m_pCurSteps[pn].Get() != m_pSteps )
		return false;
	if( m_pCharacter && GAMESTATE->m_pCurCharacters[pn] != m_pCharacter )
		return false;
	if( m_pCourse && GAMESTATE->m_pCurCourse.Get() != m_pCourse )
		return false;
	if( m_pTrail && GAMESTATE->m_pCurTrail[pn].Get() != m_pTrail )
		return false;
	if( !m_sSongGroup.empty() && GAMESTATE->m_sPreferredSongGroup != m_sSongGroup )
		return false;
	if( m_SortOrder != SortOrder_Invalid && GAMESTATE->m_PreferredSortOrder != m_SortOrder )
		return false;
	if( m_iWeightPounds != -1 && PROFILEMAN->GetProfile(pn)->m_iWeightPounds != m_iWeightPounds )
		return false;
	if( m_iGoalCalories != -1 && PROFILEMAN->GetProfile(pn)->m_iGoalCalories != m_iGoalCalories )
		return false;
	if( m_GoalType != GoalType_Invalid && PROFILEMAN->GetProfile(pn)->m_GoalType != m_GoalType )
		return false;
	if( !m_sProfileID.empty() && ProfileManager::m_sDefaultLocalProfileID[pn].Get() != m_sProfileID )
		return false;

	return true;
}

void GameCommand::Load( int iIndex, const Commands& cmds )
{
	m_iIndex = iIndex;
	m_bInvalid = false;
	m_Commands = cmds;

	FOREACH_CONST( Command, cmds.v, cmd )
		LoadOne( *cmd );
}

void GameCommand::LoadOne( const Command& cmd )
{
	RString sName = cmd.GetName();
	if( sName.empty() )
		return;

	RString sValue;
	for( unsigned i = 1; i < cmd.m_vsArgs.size(); ++i )
	{
		if( i > 1 )
			sValue += ",";
		sValue += cmd.m_vsArgs[i];
	}

	if( sName == "style" )
	{
		const Style* style = GAMEMAN->GameAndStringToStyle( GAMESTATE->m_pCurGame, sValue );
		if( style )
			m_pStyle = style;
		else
			m_bInvalid = true;
	}

	else if( sName == "playmode" )
	{
		PlayMode pm = StringToPlayMode( sValue );
		if( pm != PlayMode_Invalid )
			m_pm = pm;
		else
			m_bInvalid = true;
	}

	else if( sName == "difficulty" )
	{
		Difficulty dc = StringToDifficulty( sValue );
		if( dc != Difficulty_Invalid )
			m_dc = dc;
		else
			m_bInvalid = true;
	}

	else if( sName == "announcer" )
	{
		m_sAnnouncer = sValue;
	}

	else if( sName == "name" )
	{
		m_sName = sValue;
	}

	else if( sName == "text" )
	{
		m_sText = sValue;
	}

	else if( sName == "mod" )
	{
		if( m_sPreferredModifiers != "" )
			m_sPreferredModifiers += ",";
		m_sPreferredModifiers += sValue;
	}

	else if( sName == "stagemod" )
	{
		if( m_sStageModifiers != "" )
			m_sStageModifiers += ",";
		m_sStageModifiers += sValue;
	}

	else if( sName == "lua" )
	{
		m_LuaFunction.SetFromExpression( sValue );
		ASSERT_M( !m_LuaFunction.IsNil(), ssprintf("\"%s\" evaluated to nil", sValue.c_str()) );
	}

	else if( sName == "screen" )
	{
		m_sScreen = sValue;
	}

	else if( sName == "song" )
	{
		m_pSong = SONGMAN->FindSong( sValue );
		if( m_pSong == NULL )
		{
			m_sInvalidReason = ssprintf( "Song \"%s\" not found", sValue.c_str() );
			m_bInvalid |= true;
		}
	}

	else if( sName == "steps" )
	{
		RString sSteps = sValue;

		// This must be processed after "song" and "style" commands.
		if( !m_bInvalid )
		{
			Song *pSong = (m_pSong != NULL)? m_pSong:GAMESTATE->m_pCurSong;
			const Style *pStyle = m_pStyle ? m_pStyle : GAMESTATE->GetCurrentStyle();
			if( pSong == NULL || pStyle == NULL )
				RageException::Throw( "Must set Song and Style to set Steps." );

			Difficulty dc = StringToDifficulty( sSteps );
			if( dc != Difficulty_Edit )
				m_pSteps = SongUtil::GetStepsByDifficulty( pSong, pStyle->m_StepsType, dc );
			else
				m_pSteps = SongUtil::GetStepsByDescription( pSong, pStyle->m_StepsType, sSteps );
			if( m_pSteps == NULL )
			{
				m_sInvalidReason = "steps not found";
				m_bInvalid |= true;
			}
		}
	}

	else if( sName == "course" )
	{
		m_pCourse = SONGMAN->FindCourse( "", sValue );
		if( m_pCourse == NULL )
		{
			m_sInvalidReason = ssprintf( "Course \"%s\" not found", sValue.c_str() );
			m_bInvalid |= true;
		}
	}
	
	else if( sName == "trail" )
	{
		RString sTrail = sValue;

		// This must be processed after "course" and "style" commands.
		if( !m_bInvalid )
		{
			Course *pCourse = (m_pCourse != NULL)? m_pCourse:GAMESTATE->m_pCurCourse;
			const Style *pStyle = m_pStyle ? m_pStyle : GAMESTATE->GetCurrentStyle();
			if( pCourse == NULL || pStyle == NULL )
				RageException::Throw( "Must set Course and Style to set Steps." );

			const CourseDifficulty cd = StringToDifficulty( sTrail );
			ASSERT_M( cd != Difficulty_Invalid, ssprintf("Invalid difficulty '%s'", sTrail.c_str()) );

			m_pTrail = pCourse->GetTrail( pStyle->m_StepsType, cd );
			if( m_pTrail == NULL )
			{
				m_sInvalidReason = "trail not found";
				m_bInvalid |= true;
			}
		}
	}
	
	else if( sName == "setenv" )
	{
		if( cmd.m_vsArgs.size() == 3 )
			m_SetEnv[ cmd.m_vsArgs[1] ] = cmd.m_vsArgs[2];
	}
	
	else if( sName == "songgroup" )
	{
		m_sSongGroup = sValue;
	}

	else if( sName == "sort" )
	{
		m_SortOrder = StringToSortOrder( sValue );
		if( m_SortOrder == SortOrder_Invalid )
		{
			m_sInvalidReason = ssprintf( "SortOrder \"%s\" is not valid.", sValue.c_str() );
			m_bInvalid |= true;
		}
	}

	else if( sName == "weight" )
	{
		m_iWeightPounds = StringToInt( sValue );
	}

	else if( sName == "goalcalories" )
	{
		m_iGoalCalories = StringToInt( sValue );
	}

	else if( sName == "goaltype" )
	{
		m_GoalType = StringToGoalType( sValue );
	}

	else if( sName == "profileid" )
	{
		m_sProfileID = sValue;
	}

	else if( sName == "url" )
	{
		m_sUrl = sValue;
		m_bUrlExits = true;
	}

	else if( sName == "sound" )
	{
		m_sSoundPath = sValue;
	}

	else if( sName == "preparescreen" )
	{
		m_vsScreensToPrepare.push_back( sValue );
	}

	else if( sName == "insertcredit" )
	{
		m_bInsertCredit = true;
	}

	else if( sName == "clearcredits" )
	{
		m_bClearCredits = true;
	}

	else if( sName == "stopmusic" )
	{
		m_bStopMusic = true;
	}

	else if( sName == "applydefaultoptions" )
	{
		m_bApplyDefaultOptions = true;
	}

	// sm-ssc additions begin:
	else if( sName == "urlnoexit" )
	{
		m_sUrl = sValue;
		m_bUrlExits = false;
	}

	else if( sName == "setpref" )
	{
		if( cmd.m_vsArgs.size() == 3 )
		{
			IPreference *pPref = IPreference::GetPreferenceByName( cmd.m_vsArgs[1] );
			if( pPref == NULL )
			{
				m_sInvalidReason = ssprintf("unknown preference \"%s\"", cmd.m_vsArgs[1].c_str() );
				m_bInvalid |= true;
			}
			pPref->FromString(cmd.m_vsArgs[2]);
		}
	}

	else if( sName == "fademusic" )
	{
		if( cmd.m_vsArgs.size() == 3 )
		{
			m_bFadeMusic = true;
			m_fMusicFadeOutVolume = static_cast<float>(atof( cmd.m_vsArgs[1] ));
			m_fMusicFadeOutSeconds = static_cast<float>(atof( cmd.m_vsArgs[2] ));
		}
	}

	else
	{
		RString sWarning = ssprintf( "Command '%s' is not valid.", cmd.GetOriginalCommandString().c_str() );
		LOG->Warn( "%s", sWarning.c_str() );
		Dialog::OK( sWarning, "INVALID_GAME_COMMAND" );
	}
}

int GetNumCreditsPaid()
{
	int iNumCreditsPaid = GAMESTATE->GetNumSidesJoined();

	// players other than the first joined for free
	if( GAMESTATE->GetPremium() == Premium_2PlayersFor1Credit )
		iNumCreditsPaid = min( iNumCreditsPaid, 1 );

	return iNumCreditsPaid;
}


int GetCreditsRequiredToPlayStyle( const Style *style )
{
	if( GAMESTATE->GetPremium() == Premium_2PlayersFor1Credit )
		return 1;

	switch( style->m_StyleType )
	{
	case StyleType_OnePlayerOneSide:
		return 1;
	case StyleType_TwoPlayersSharedSides:
	case StyleType_TwoPlayersTwoSides:
		return 2;
	case StyleType_OnePlayerTwoSides:
		return (GAMESTATE->GetPremium() == Premium_DoubleFor1Credit) ? 1 : 2;
	DEFAULT_FAIL( style->m_StyleType );
	}
}

static bool AreStyleAndPlayModeCompatible( const Style *style, PlayMode pm )
{
	if( style == NULL || pm == PlayMode_Invalid )
		return true;

	switch( pm )
	{
		case PLAY_MODE_BATTLE:
		case PLAY_MODE_RAVE:
			// Can't play rave if there isn't enough room for two players.
			// This is correct for dance (ie, no rave for solo and doubles),
			// and should be okay for pump.. not sure about other game types.
			// Techno Motion scales down versus arrows, though, so allow this.
			if( style->m_iColsPerPlayer >= 6 && RString(GAMESTATE->m_pCurGame->m_szName) != "techno" )
				return false;

			// Don't allow battle modes if the style takes both sides.
			if( style->m_StyleType==StyleType_OnePlayerTwoSides ||
				style->m_StyleType==StyleType_TwoPlayersSharedSides )
				return false;
		default: return true;
	}
}

bool GameCommand::IsPlayable( RString *why ) const
{
	if( m_bInvalid )
	{
		if( why )
			*why = m_sInvalidReason;
		return false;
	}

	if ( m_pStyle )
	{
		int iCredits = NUM_PLAYERS;
		const int iNumCreditsPaid = GetNumCreditsPaid();
		const int iNumCreditsRequired = GetCreditsRequiredToPlayStyle(m_pStyle);

		/* With PREFSMAN->m_bDelayedCreditsReconcile disabled, enough credits must
		 * be paid. (This means that enough sides must be joined.)  Enabled, simply
		 * having enough credits lying in the machine is sufficient; we'll deduct the
		 * extra in Apply(). */
		int iNumCreditsAvailable = iNumCreditsPaid;
		if( PREFSMAN->m_bDelayedCreditsReconcile )
			iNumCreditsAvailable += iCredits;

		if( iNumCreditsAvailable < iNumCreditsRequired )
		{
			if( why )
				*why = ssprintf( "need %i credits, have %i", iNumCreditsRequired, iNumCreditsAvailable );
			return false;
		}

		/* If both sides are joined, disallow singles modes, since easy to select
		 * them accidentally, instead of versus mode. */
		if( m_pStyle->m_StyleType == StyleType_OnePlayerOneSide &&
			GAMESTATE->GetNumSidesJoined() > 1 )
		{
			if( why )
				*why = "too many players joined for ONE_PLAYER_ONE_CREDIT";
			return false;
		}
	}

	/* Don't allow a PlayMode that's incompatible with our current Style (if set),
	 * and vice versa. */
	if( m_pm != PlayMode_Invalid || m_pStyle != NULL )
	{
		const PlayMode pm = (m_pm != PlayMode_Invalid) ? m_pm : GAMESTATE->m_PlayMode;
		const Style *style = (m_pStyle != NULL)? m_pStyle: GAMESTATE->GetCurrentStyle();
		if( !AreStyleAndPlayModeCompatible( style, pm ) )
		{
			if( why )
				*why = ssprintf("mode %s is incompatible with style %s",
					PlayModeToString(pm).c_str(), style->m_szName );

			return false;
		}
	}

	if( !m_sScreen.CompareNoCase("ScreenEditCoursesMenu") )
	{
		vector<Course*> vCourses;
		SONGMAN->GetAllCourses( vCourses, false );

		if( vCourses.size() == 0 )
		{
			if( why )
				*why = "No courses are installed";
			return false;
		}
	}

	if( (!m_sScreen.CompareNoCase("ScreenJukeboxMenu") ||
		!m_sScreen.CompareNoCase("ScreenEditMenu") ||
		!m_sScreen.CompareNoCase("ScreenEditCoursesMenu")) )
	{
		if( SONGMAN->GetNumSongs() == 0 )
		{
			if( why )
				*why = "No songs are installed";
			return false;
		}
	}

	if( !m_sPreferredModifiers.empty() )
	{
		// TODO: Split this and check each modifier individually
		if( UNLOCKMAN->ModifierIsLocked(m_sPreferredModifiers) )
		{	if( why )
				*why = "Modifier is locked";
			return false;
		}
	}

	if( !m_sStageModifiers.empty() )
	{
		// TODO: Split this and check each modifier individually
		if( UNLOCKMAN->ModifierIsLocked(m_sStageModifiers) )
		{	if( why )
				*why = "Modifier is locked";
			return false;
		}
	}

	return true;
}

void GameCommand::ApplyToAllPlayers() const
{
	vector<PlayerNumber> vpns;

	FOREACH_PlayerNumber( pn )
		vpns.push_back( pn );

	Apply( vpns );
}

void GameCommand::Apply( PlayerNumber pn ) const
{
	vector<PlayerNumber> vpns;
	vpns.push_back( pn );
	Apply( vpns );
}

void GameCommand::Apply( const vector<PlayerNumber> &vpns ) const
{
	if( m_Commands.v.size() )
	{
		// We were filled using a GameCommand from metrics. Apply the options in order.
		FOREACH_CONST( Command, m_Commands.v, cmd )
		{
			GameCommand gc;
			gc.m_bInvalid = false;
			gc.m_bApplyCommitsScreens = m_bApplyCommitsScreens;
			gc.LoadOne( *cmd );
			gc.ApplySelf( vpns );
		}
	}
	else
	{
		// We were filled by an OptionRowHandler in code. m_Commands isn't filled,
		// so just apply the values that are already set in this.
		this->ApplySelf( vpns );
	}
}

void GameCommand::ApplySelf( const vector<PlayerNumber> &vpns ) const
{
	const PlayMode OldPlayMode = GAMESTATE->m_PlayMode;

	if( m_pm != PlayMode_Invalid )
		GAMESTATE->m_PlayMode.Set( m_pm );

	if( m_pStyle != NULL )
	{
		GAMESTATE->SetCurrentStyle( m_pStyle );

		// If only one side is joined and we picked a style that requires both
		// sides, join the other side.
		switch( m_pStyle->m_StyleType )
		{
		case StyleType_OnePlayerOneSide:
			break;
		case StyleType_TwoPlayersTwoSides:
		case StyleType_OnePlayerTwoSides:
		case StyleType_TwoPlayersSharedSides:
			{
				FOREACH_PlayerNumber( p )
					GAMESTATE->JoinPlayer( p );
			}
			break;
		default:
			FAIL_M(ssprintf("Invalid StyleType: %i", m_pStyle->m_StyleType));
		}
	}
	if( m_dc != Difficulty_Invalid )
		FOREACH_CONST( PlayerNumber, vpns, pn )
			GAMESTATE->m_PreferredDifficulty[*pn].Set( m_dc );
	if( m_sAnnouncer != "" )
		ANNOUNCER->SwitchAnnouncer( m_sAnnouncer );
	if( m_sPreferredModifiers != "" )
		FOREACH_CONST( PlayerNumber, vpns, pn )
			GAMESTATE->ApplyPreferredModifiers( *pn, m_sPreferredModifiers );
	if( m_sStageModifiers != "" )
		FOREACH_CONST( PlayerNumber, vpns, pn )
			GAMESTATE->ApplyStageModifiers( *pn, m_sStageModifiers );
	if( m_LuaFunction.IsSet() )
	{
		Lua *L = LUA->Get();
		FOREACH_CONST( PlayerNumber, vpns, pn )
		{
			m_LuaFunction.PushSelf( L );
			ASSERT( !lua_isnil(L, -1) );

			lua_pushnumber( L, *pn ); // 1st parameter
			lua_call( L, 1, 0 ); // call function with 1 argument and 0 results
		}
		LUA->Release(L);
	}
	if( m_sScreen != "" && m_bApplyCommitsScreens )
		SCREENMAN->SetNewScreen( m_sScreen );
	if( m_pSong )
	{
		GAMESTATE->m_pCurSong.Set( m_pSong );
		GAMESTATE->m_pPreferredSong = m_pSong;
	}
	if( m_pSteps )
		FOREACH_CONST( PlayerNumber, vpns, pn )
			GAMESTATE->m_pCurSteps[*pn].Set( m_pSteps );
	if( m_pCourse )
	{
		GAMESTATE->m_pCurCourse.Set( m_pCourse );
		GAMESTATE->m_pPreferredCourse = m_pCourse;
	}
	if( m_pTrail )
		FOREACH_CONST( PlayerNumber, vpns, pn )
			GAMESTATE->m_pCurTrail[*pn].Set( m_pTrail );
	if( m_CourseDifficulty != Difficulty_Invalid )
		FOREACH_CONST( PlayerNumber, vpns, pn )
			GAMESTATE->ChangePreferredCourseDifficulty( *pn, m_CourseDifficulty );
	if( m_pCharacter )
		FOREACH_CONST( PlayerNumber, vpns, pn )
			GAMESTATE->m_pCurCharacters[*pn] = m_pCharacter;
	for( map<RString,RString>::const_iterator i = m_SetEnv.begin(); i != m_SetEnv.end(); i++ )
	{
		Lua *L = LUA->Get();
		GAMESTATE->m_Environment->PushSelf(L);
		lua_pushstring( L, i->first );
		lua_pushstring( L, i->second );
		lua_settable( L, -3 );
		lua_pop( L, 1 );
		LUA->Release(L);
	}
	if( !m_sSongGroup.empty() )
		GAMESTATE->m_sPreferredSongGroup.Set( m_sSongGroup );
	if( m_SortOrder != SortOrder_Invalid )
		GAMESTATE->m_PreferredSortOrder = m_SortOrder;
	if( m_sSoundPath != "" )
		SOUND->PlayOnce( THEME->GetPathS( "", m_sSoundPath ) );
	if( m_iWeightPounds != -1 )
		FOREACH_CONST( PlayerNumber, vpns, pn )
			PROFILEMAN->GetProfile(*pn)->m_iWeightPounds = m_iWeightPounds;
	if( m_iGoalCalories != -1 )
		FOREACH_CONST( PlayerNumber, vpns, pn )
			PROFILEMAN->GetProfile(*pn)->m_iGoalCalories = m_iGoalCalories;
	if( m_GoalType != GoalType_Invalid )
		FOREACH_CONST( PlayerNumber, vpns, pn )
			PROFILEMAN->GetProfile(*pn)->m_GoalType = m_GoalType;
	if( !m_sProfileID.empty() )
		FOREACH_CONST( PlayerNumber, vpns, pn )
			ProfileManager::m_sDefaultLocalProfileID[*pn].Set( m_sProfileID );
	if( !m_sUrl.empty() )
	{
		if( HOOKS->GoToURL( m_sUrl ) )
		{
			if( m_bUrlExits )
				SCREENMAN->SetNewScreen( "ScreenExit" );
		}
		else
			ScreenPrompt::Prompt( SM_None, COULD_NOT_LAUNCH_BROWSER );
	}

	/* If we're going to stop music, do so before preparing new screens, so we
	 * don't stop music between preparing screens and loading screens. */
	if( m_bStopMusic )
		SOUND->StopMusic();
	if( m_bFadeMusic )
		SOUND->DimMusic(m_fMusicFadeOutVolume, m_fMusicFadeOutSeconds);

	FOREACH_CONST( RString, m_vsScreensToPrepare, s )
		SCREENMAN->PrepareScreen( *s );

	if( m_bInsertCredit )
	{
		StepMania::InsertCredit();
	}

	if( m_bClearCredits )
		StepMania::ClearCredits();

	if( m_bApplyDefaultOptions )
	{
		// applying options affects only the current stage
		FOREACH_PlayerNumber( p )
		{
			PlayerOptions po;
			GAMESTATE->GetDefaultPlayerOptions( po );
			GAMESTATE->m_pPlayerState[p]->m_PlayerOptions.Assign( ModsLevel_Stage, po );
		}

		SongOptions so;
		GAMESTATE->GetDefaultSongOptions( so );
		GAMESTATE->m_SongOptions.Assign( ModsLevel_Stage, so );
	}
	// HACK: Set life type to BATTERY just once here so it happens once and 
	// we don't override the user's changes if they back out.
	if( GAMESTATE->m_PlayMode == PLAY_MODE_ONI && 
		GAMESTATE->m_PlayMode != OldPlayMode &&
		GAMESTATE->m_SongOptions.GetStage().m_LifeType == SongOptions::LIFE_BAR )
	{
		SO_GROUP_ASSIGN( GAMESTATE->m_SongOptions, ModsLevel_Stage, m_LifeType, SongOptions::LIFE_BATTERY );
	}
}

bool GameCommand::IsZero() const
{
	if( 	m_pm != PlayMode_Invalid ||
		m_pStyle != NULL ||
		m_dc != Difficulty_Invalid ||
		m_sAnnouncer != "" ||
		m_sPreferredModifiers != "" ||
		m_sStageModifiers != "" ||
		m_pSong != NULL || 
		m_pSteps != NULL || 
		m_pCourse != NULL || 
		m_pTrail != NULL || 
		m_pCharacter != NULL || 
		m_CourseDifficulty != Difficulty_Invalid ||
		!m_sSongGroup.empty() ||
		m_SortOrder != SortOrder_Invalid ||
		m_iWeightPounds != -1 ||
		m_iGoalCalories != -1 ||
		m_GoalType != GoalType_Invalid ||
		!m_sProfileID.empty() ||
		!m_sUrl.empty() )
		return false;

	return true;
}

// lua start
#include "LuaBinding.h"
#include "Game.h"
#include "Steps.h"
#include "Character.h"

/** @brief Allow Lua to have access to the GameCommand. */ 
class LunaGameCommand: public Luna<GameCommand>
{
public:
	static int GetName( T* p, lua_State *L )	{ lua_pushstring(L, p->m_sName ); return 1; }
	static int GetText( T* p, lua_State *L )	{ lua_pushstring(L, p->m_sText ); return 1; }
	static int GetIndex( T* p, lua_State *L )	{ lua_pushnumber(L, p->m_iIndex ); return 1; }
	static int GetMultiPlayer( T* p, lua_State *L )	{ lua_pushnumber(L, p->m_MultiPlayer); return 1; }
	static int GetStyle( T* p, lua_State *L )	{ if(p->m_pStyle==NULL) lua_pushnil(L); else {Style *pStyle = (Style*)p->m_pStyle; pStyle->PushSelf(L);} return 1; }
	static int GetScreen( T* p, lua_State *L )	{ lua_pushstring(L, p->m_sScreen ); return 1; }
	static int GetProfileID( T* p, lua_State *L )	{ lua_pushstring(L, p->m_sProfileID ); return 1; }
	static int GetSong( T* p, lua_State *L )	{ if(p->m_pSong==NULL) lua_pushnil(L); else p->m_pSong->PushSelf(L); return 1; }
	static int GetSteps( T* p, lua_State *L )	{ if(p->m_pSteps==NULL) lua_pushnil(L); else p->m_pSteps->PushSelf(L); return 1; }
	static int GetCourse( T* p, lua_State *L )	{ if(p->m_pCourse==NULL) lua_pushnil(L); else p->m_pCourse->PushSelf(L); return 1; }
	static int GetTrail( T* p, lua_State *L )	{ if(p->m_pTrail==NULL) lua_pushnil(L); else p->m_pTrail->PushSelf(L); return 1; }
	static int GetCharacter( T* p, lua_State *L )	{ if(p->m_pCharacter==NULL) lua_pushnil(L); else p->m_pCharacter->PushSelf(L); return 1; }
	static int GetSongGroup( T* p, lua_State *L )	{ lua_pushstring(L, p->m_sSongGroup ); return 1; }
	static int GetUrl( T* p, lua_State *L )	{ lua_pushstring(L, p->m_sUrl ); return 1; }
	static int GetAnnouncer( T* p, lua_State *L )	{ lua_pushstring(L, p->m_sAnnouncer ); return 1; }
	static int GetPreferredModifiers( T* p, lua_State *L )	{ lua_pushstring(L, p->m_sPreferredModifiers ); return 1; }
	static int GetStageModifiers( T* p, lua_State *L )	{ lua_pushstring(L, p->m_sStageModifiers ); return 1; }

	DEFINE_METHOD( GetCourseDifficulty,	m_CourseDifficulty )
	DEFINE_METHOD( GetDifficulty,	m_dc )
	DEFINE_METHOD( GetPlayMode,		m_pm )
	DEFINE_METHOD( GetSortOrder,	m_SortOrder )

	LunaGameCommand()
	{
		ADD_METHOD( GetName );
		ADD_METHOD( GetText );
		ADD_METHOD( GetIndex );
		ADD_METHOD( GetMultiPlayer );
		ADD_METHOD( GetStyle );
		ADD_METHOD( GetDifficulty );
		ADD_METHOD( GetCourseDifficulty );
		ADD_METHOD( GetScreen );
		ADD_METHOD( GetPlayMode );
		ADD_METHOD( GetProfileID );
		ADD_METHOD( GetSong );
		ADD_METHOD( GetSteps );
		ADD_METHOD( GetCourse );
		ADD_METHOD( GetTrail );
		ADD_METHOD( GetCharacter );
		ADD_METHOD( GetSongGroup );
		ADD_METHOD( GetSortOrder );
		ADD_METHOD( GetUrl );
		ADD_METHOD( GetAnnouncer );
		ADD_METHOD( GetPreferredModifiers );
		ADD_METHOD( GetStageModifiers );
	}
};

LUA_REGISTER_CLASS( GameCommand )
// lua end

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
