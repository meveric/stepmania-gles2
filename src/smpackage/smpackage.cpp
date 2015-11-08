#define CO_EXIST_WITH_MFC
#include "global.h"
#include "stdafx.h"
#include "smpackage.h"
#include "smpackageExportDlg.h"
#include "smpackageInstallDlg.h"
#include "RageUtil.h"
#include "smpackageUtil.h"
#include "MainMenuDlg.h"
#include "RageFileManager.h"
#include "LuaManager.h"
#include "ThemeManager.h"
#include "SpecialFiles.h"
#include "IniFile.h"
#include "LocalizedString.h"
#include "arch/Dialog/Dialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

BEGIN_MESSAGE_MAP(CSmpackageApp, CWinApp)
	//{{AFX_MSG_MAP(CSmpackageApp)
	//}}AFX_MSG_MAP
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

CSmpackageApp::CSmpackageApp()
{
	// Place all significant initialization in InitInstance
}

CSmpackageApp theApp;
extern const char *const version_date = "";
extern const char *const version_time = "";

#include "RageLog.h"
#include "RageFileManager.h"
#include "archutils/Win32/SpecialDirs.h"
#include "ProductInfo.h"
#include "archutils/Win32/CommandLine.h"
extern RString GetLastErrorString();

static LocalizedString STATS_XML_NOT_YET_CREATED	( "CSmpackageApp", "The file Stats.xml has not yet been created.  It will be created once the game is run." );
static LocalizedString FAILED_TO_OPEN			( "CSmpackageApp", "Failed to open '%s': %s" );
static LocalizedString THE_FILE_DOES_NOT_EXIST	( "CSmpackageApp", "The file '%s' does not exist.  Aborting installation." );
BOOL CSmpackageApp::InitInstance()
{
	char **argv;
	int argc = GetWin32CmdLine( argv );

	/* Almost everything uses this to read and write files.  Load this early. */
	FILEMAN = new RageFileManager( argv[0] );
	FILEMAN->MountInitialFilesystems();

	/* Set this up next.  Do this early, since it's needed for RageException::Throw. */
	LOG			= new RageLog;


	TCHAR szCurrentDirectory[MAX_PATH];
	GetCurrentDirectory( ARRAYLEN(szCurrentDirectory), szCurrentDirectory );
	if( CAN_INSTALL_PACKAGES && SMPackageUtil::IsValidInstallDir(szCurrentDirectory) )
	{
		SMPackageUtil::AddGameInstallDir( szCurrentDirectory );	// add this if it doesn't already exist
		SMPackageUtil::SetDefaultInstallDir( szCurrentDirectory );
	}	

	FILEMAN->Remount( "/", szCurrentDirectory );


	LUA = new LuaManager;
	THEME = new ThemeManager;

	// TODO: Use PrefsManager to get the current language instead?  PrefsManager would 
	// need to be split up to reduce dependencies
	RString sTheme = SpecialFiles::BASE_THEME_NAME;

	{
		RString sType = "Preferences";
		GetFileContents( SpecialFiles::TYPE_TXT_FILE, sType, true );
		IniFile ini;
		if( ini.ReadFile(SpecialFiles::STATIC_INI_PATH) )
		{
			while( 1 )
			{
				if( ini.GetValue(sType, "Theme", sTheme) )
					break;
				if( ini.GetValue(sType, "Fallback", sType) )
					continue;
				break;
			}
		}
	}

	RString sLanguage;
	bool bPseudoLocalize = false;
	bool bShowLogOutput = false;
	bool bLogToDisk = false;
	{
		IniFile ini;
		if( ini.ReadFile(SpecialFiles::PREFERENCES_INI_PATH) )
		{
			ini.GetValue( "Options", "Theme", sTheme );
			ini.GetValue( "Options", "Language", sLanguage );
			ini.GetValue( "Options", "PseudoLocalize", bPseudoLocalize );
			ini.GetValue( "Options", "ShowLogOutput", bShowLogOutput );
			ini.GetValue( "Options", "LogToDisk", bLogToDisk );
		}
	}
	THEME->SwitchThemeAndLanguage( sTheme, sLanguage, bPseudoLocalize );
	LOG->SetShowLogOutput( bShowLogOutput );
	LOG->SetLogToDisk( bLogToDisk );
	LOG->SetInfoToDisk( true );


	// check for --machine-profile-stats and launch Stats.xml
	for( int i=0; i<argc; i++ )
	{
		CString sArg = argv[i];
		if( sArg == "--machine-profile-stats" )
		{
			RString sOSFile = SpecialDirs::GetAppDataDir() + PRODUCT_ID +"/Save/MachineProfile/Stats.xml";
			HINSTANCE hinst = ::ShellExecute( NULL, "open", sOSFile, "", "", SW_SHOWNORMAL );
			// See MSDN for an explanation of this return value
			if( (int)hinst == SE_ERR_FNF )
				Dialog::OK( STATS_XML_NOT_YET_CREATED );
			else if( (int)hinst <= 32 )
				Dialog::OK( ssprintf(FAILED_TO_OPEN.GetValue(),sOSFile.c_str(),GetLastErrorString().c_str()) );
			exit(1);	// better way to quit?
		}
	}

	// check if there's a .smzip command line argument and install it
	for( int i=0; i<argc; i++ )
	{
		RString sPath = argv[i];
		TrimLeft( sPath );
		TrimRight( sPath );
		RString sPathLower = sPath;
		sPathLower.MakeLower();

		// test to see if this is a smzip file
		if( sPathLower.Right(3).CompareNoCase("zip")==0 )
		{
			// We found a zip package.  Prompt the user to install it!
			CSMPackageInstallDlg dlg( sPath );
			int nResponse = dlg.DoModal();
			if( nResponse == IDCANCEL )
				exit(1);	// better way to exit?
		}
	}

	{
		MainMenuDlg dlg;
		int nResponse = dlg.DoModal();
	}

	SAFE_DELETE( THEME );
	SAFE_DELETE( LUA );
	SAFE_DELETE( FILEMAN );


	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}

/*
 * (c) 2002-2005 Chris Danford
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
