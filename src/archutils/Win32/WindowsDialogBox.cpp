#include "global.h"
#include "WindowsDialogBox.h"

WindowsDialogBox::WindowsDialogBox()
{
	m_hWnd = NULL;
}

void WindowsDialogBox::Run( int iDialog )
{
	char szFullAppPath[MAX_PATH];
	GetModuleFileName( NULL, szFullAppPath, MAX_PATH );
	HINSTANCE hHandle = LoadLibrary( szFullAppPath );

	DialogBoxParam( hHandle, MAKEINTRESOURCE(iDialog), NULL, DlgProc, (LPARAM) this );
}

BOOL APIENTRY WindowsDialogBox::DlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
	if( msg == WM_INITDIALOG )
		SetProp( hDlg, "WindowsDialogBox", (HANDLE) lParam );

	WindowsDialogBox *pThis = (WindowsDialogBox *) GetProp( hDlg, "WindowsDialogBox" );
	if( pThis == NULL )
		return FALSE;

	if( pThis->m_hWnd == NULL )
		pThis->m_hWnd = hDlg;

	return pThis->HandleMessage( msg, wParam, lParam );
}

/*
 * (c) 2006 Glenn Maynard
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
