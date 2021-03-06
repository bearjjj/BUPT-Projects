 /*----------------------------------------
   Keep your Notebook awake when Lid is closed.
                (c) LJN, 2014
  ----------------------------------------*/

//AwayMode can be set in registry:
//HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Power
//AwayModeEnabled (DWORD 32) 1

#include <windows.h>
#include <powrprof.h>
#include "resource.h"
#pragma comment (lib, "powrprof.lib")

#define IDN_TRAY		1
#define IDM_TRAY		2
#define IDM_HINT		3
#define IDM_LOCK		4
#define IDM_QUIT		5
#define WM_TRAY			WM_USER + 1

BOOL Set_Scheme (GUID *pguid, DWORD fAC, DWORD fDC, BOOL fUpdate);
GUID *Get_Scheme (DWORD *pfAC, DWORD *pfDC);
LRESULT CALLBACK WndProc (HWND, UINT, WPARAM, LPARAM);

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
					PSTR szCmdLine, int iCmdShow)
{
	static TCHAR szAppName[] = TEXT ("Sleep-Manager");
	HWND hwnd;
	MSG msg;
	WNDCLASS wndclass;

/*
	hwnd = FindWindow (szAppName, szAppName);
	if (IsWindow (hwnd))
	{
		SendMessage (hwnd, WM_TRAY, 0, WM_LBUTTONUP);
		return 0;
	}
*/

	HANDLE hMutex = CreateMutex (NULL, FALSE, szAppName);
	if (GetLastError () == ERROR_ALREADY_EXISTS)
	{
		MessageBox (NULL, TEXT ("Sorry, another Sleep Manager is running."),
					TEXT ("Error"), MB_ICONERROR);
		return 0;
	}

	wndclass.style         = CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc   = WndProc;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = 0;
	wndclass.hInstance     = hInstance;
	wndclass.hIcon         = LoadIcon (hInstance, MAKEINTRESOURCE (IDI_APP_ICON));
	wndclass.hCursor       = LoadCursor (NULL, IDC_ARROW);
	wndclass.hbrBackground = (HBRUSH) (GetStockObject (WHITE_BRUSH));
	wndclass.lpszMenuName  = NULL;
	wndclass.lpszClassName = szAppName;
	RegisterClass (&wndclass);

	hwnd = CreateWindow (szAppName, szAppName,
						WS_OVERLAPPEDWINDOW,
						CW_USEDEFAULT, CW_USEDEFAULT,
						CW_USEDEFAULT, CW_USEDEFAULT,
						NULL, NULL, hInstance, NULL);

	if (hwnd)
		while (GetMessage (&msg, NULL, 0, 0))
		{
			TranslateMessage (&msg);
			DispatchMessage (&msg);
		}
	else
		MessageBox (NULL, TEXT ("Access Denied"),
					TEXT ("Sleep Manager"), MB_OK);

	ReleaseMutex (hMutex);
	return 0;
}

LRESULT CALLBACK WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static HINSTANCE hInstance;
	static HMENU hMenu;
	static NOTIFYICONDATA Notify;
	static POINT pt;

	static GUID *pguid;
	static DWORD fAC_restore, fDC_restore;
	static BOOL fOnce = TRUE, fOnce2 = TRUE;
	static UINT fAbort, fLock;

	static HPOWERNOTIFY hPowerSchemeNotify, hPowerLidNotify;
	PPOWERBROADCAST_SETTING ppbs;

	static TCHAR szAppName[] = TEXT ("Sleep Manager 2015 LJN");  //©
	static TCHAR szAwakeTip[] = TEXT ("Sleep Manager");
	static TCHAR szAwakeInfo[] = TEXT ("Sleep Manager is running in the background.");

	switch (message)
	{
	case WM_CREATE:
		hInstance = ((LPCREATESTRUCT) lParam) -> hInstance;

		if (GetPrivateProfileInt (szAppName, TEXT ("IsAbort"),
				0, TEXT (".//Sleep_Manager.ini")))
			MessageBox (NULL, TEXT ("Sleep Manager aborted last time..."),
				TEXT ("Sleep Manager"), MB_ICONERROR);

		fLock = GetPrivateProfileInt (szAppName, TEXT ("IsLock"),
				1, TEXT (".//Sleep_Manager.ini"));

		fAbort = 1;  //Mark the -1s
		if (!(pguid = Get_Scheme (&fAC_restore, &fDC_restore)))
			return -1;
		if (!Set_Scheme (pguid, FALSE, FALSE, TRUE))
		{
			LocalFree (pguid);
			return -1;
		}

		hPowerSchemeNotify = RegisterPowerSettingNotification (hwnd,
						&GUID_ACTIVE_POWERSCHEME, DEVICE_NOTIFY_WINDOW_HANDLE);
		hPowerLidNotify = RegisterPowerSettingNotification (hwnd,
						&GUID_LIDSWITCH_STATE_CHANGE, DEVICE_NOTIFY_WINDOW_HANDLE);
		if (!hPowerSchemeNotify || !hPowerLidNotify)
		{
			if (!Set_Scheme (pguid, fAC_restore, fDC_restore, TRUE))
				MessageBox (NULL, TEXT ("FATAL ERROR:\nUnable to restore the previous setting."),
						TEXT ("Sleep Manager"), MB_ICONERROR);
			LocalFree (pguid);
			return -1;
		}

		hMenu = CreatePopupMenu ();
		AppendMenu (hMenu, MF_STRING, IDM_LOCK, TEXT ("Lock?"));
		AppendMenu (hMenu, MF_STRING, IDM_HINT, TEXT ("Hint"));
		AppendMenu (hMenu, MF_SEPARATOR, 0, 0);
		AppendMenu (hMenu, MF_STRING, IDM_QUIT, TEXT ("Quit"));
		if (fLock)
			ModifyMenu (hMenu, IDM_LOCK, MF_BYCOMMAND | MF_CHECKED, IDM_LOCK, TEXT ("Lock?"));
		else
			ModifyMenu (hMenu, IDM_LOCK, MF_BYCOMMAND | MF_UNCHECKED, IDM_LOCK, TEXT ("Lock?"));

		Notify.cbSize = (DWORD) sizeof (NOTIFYICONDATA);
		Notify.hWnd = hwnd;
		Notify.uID = IDN_TRAY;
		Notify.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO;
		Notify.uCallbackMessage = WM_TRAY;
		Notify.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON;
		Notify.uTimeout = 1000;
		wcscpy_s (Notify.szInfoTitle, sizeof szAppName, szAppName);

		Notify.hIcon = Notify.hBalloonIcon =
			LoadIcon (hInstance, MAKEINTRESOURCE (IDI_APP_ICON));
		wcscpy_s (Notify.szTip, sizeof szAwakeTip, szAwakeTip);
		wcscpy_s (Notify.szInfo, sizeof szAwakeInfo, szAwakeInfo);
		Shell_NotifyIcon (NIM_ADD, &Notify);

		WritePrivateProfileString (szAppName, TEXT ("IsAbort"),
			TEXT ("1"), TEXT (".//Sleep_Manager.ini"));
		fAbort = 0;
		return 0;

	case WM_TRAY:
		switch (lParam)
		{
		case WM_RBUTTONUP:
			SetForegroundWindow (hwnd);
			GetCursorPos (&pt);
			TrackPopupMenu (hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
			break;
		} 
		return 0;

	case WM_COMMAND:
	case WM_SYSCOMMAND:
		switch (LOWORD (wParam))
		{
		case IDM_LOCK:
			fLock = !fLock;
			if (fLock)
			{
				ModifyMenu (hMenu, IDM_LOCK, MF_BYCOMMAND | MF_CHECKED,
					IDM_LOCK, TEXT ("Lock?"));
				WritePrivateProfileString (szAppName, TEXT ("IsLock"),
					TEXT ("1"), TEXT (".//Sleep_Manager.ini"));
			}
			else
			{
				ModifyMenu (hMenu, IDM_LOCK, MF_BYCOMMAND | MF_UNCHECKED,
					IDM_LOCK, TEXT ("Lock?"));
				WritePrivateProfileString (szAppName, TEXT ("IsLock"),
					TEXT ("0"), TEXT (".//Sleep_Manager.ini"));
			}
			break;

		case IDM_HINT:
			MessageBox (NULL,TEXT ("Lock?: Require Password when waking up or not;\n")
				TEXT ("Quit: Leave and reset power policy;\n")
				TEXT ("Sleep_Manager.ini: Record your preference;"),
				TEXT ("Sleep Manager"), MB_OK);
			break;

		case IDM_QUIT:
			SendMessage (hwnd, WM_DESTROY, 0, 0);
			return 0;
		}
		break;

	case WM_POWERBROADCAST:
		switch (wParam)
		{
		case PBT_POWERSETTINGCHANGE:
			ppbs = (POWERBROADCAST_SETTING*) lParam;

			if (!memcmp (&ppbs -> PowerSetting,
				&GUID_LIDSWITCH_STATE_CHANGE, sizeof (GUID)))  //Lid Open or Closed
			{
				if (fOnce)
				{
					fOnce = FALSE;
					break;
				}

				if (!*(unsigned int*) ppbs -> Data)  //Close
				{
					SetThreadExecutionState (ES_SYSTEM_REQUIRED | ES_CONTINUOUS);
					if (fLock) LockWorkStation ();
				}
				else  //Open
				{
					SetThreadExecutionState (ES_CONTINUOUS);
					Shell_NotifyIcon (NIM_MODIFY, &Notify);
				}
			}
			else if (!memcmp (&ppbs -> PowerSetting,
				&GUID_ACTIVE_POWERSCHEME, sizeof (GUID)))  //Power Scheme changed
			{
				if (fOnce2)
				{
					fOnce2 = FALSE;
					break;
				}

				if (!Set_Scheme (pguid, fAC_restore, fDC_restore, FALSE))
				{
					MessageBox (NULL, TEXT ("Unable to restore the previous setting."),
							TEXT ("Sleep Manager"), MB_ICONERROR);
					PostMessage (hwnd, WM_DESTROY, 0, 0);
				}
				LocalFree (pguid);

				if (!(pguid = Get_Scheme (&fAC_restore, &fDC_restore)))
				{
					MessageBox (NULL, TEXT ("Unable to get the new power policy."),
							TEXT ("Sleep Manager"), MB_ICONERROR);
					PostMessage (hwnd, WM_DESTROY, 0, 0);
				}

				if (!Set_Scheme (pguid, FALSE, FALSE, TRUE))
				{
					LocalFree (pguid);
					MessageBox (NULL, TEXT ("Unable to patch the new power policy."),
							TEXT ("Sleep Manager"), MB_ICONERROR);
					PostMessage (hwnd, WM_DESTROY, 0, 0);
				}

				Shell_NotifyIcon (NIM_MODIFY, &Notify);
			}
			else
			{
				MessageBox (NULL,
					TEXT ("Sorry, Your Power Scheme has been changed in an unexpected way :-("),
					TEXT ("Sleep Manager"), MB_ICONERROR);
				break;
			}
		}
		return 0;

	case WM_QUERYENDSESSION:
	case WM_DESTROY:
		if (fAbort) return 0;

		if (!Set_Scheme (pguid, fAC_restore, fDC_restore, TRUE))
			MessageBox (NULL, TEXT ("Unable to restore the previous setting."),
					TEXT ("Sleep Manager"), MB_ICONERROR);
		LocalFree (pguid);

		DestroyMenu (hMenu);
		DestroyIcon (Notify.hIcon);
		Shell_NotifyIcon (NIM_DELETE, &Notify);
		UnregisterPowerSettingNotification (hPowerSchemeNotify);
		UnregisterPowerSettingNotification (hPowerLidNotify);

		WritePrivateProfileString (szAppName, TEXT ("IsAbort"),
			NULL, TEXT (".//Sleep_Manager.ini"));

		PostQuitMessage (0);
		return 0;
	}
	return DefWindowProc (hwnd, message, wParam, lParam);
}

BOOL Set_Scheme (GUID *pguid, DWORD fAC, DWORD fDC, BOOL fUpdate)
{
	if (ERROR_SUCCESS != PowerWriteACValueIndex (NULL, pguid,
				&GUID_SYSTEM_BUTTON_SUBGROUP,
				&GUID_LIDCLOSE_ACTION,
				fAC))
		return FALSE;
	if (ERROR_SUCCESS != PowerWriteDCValueIndex (NULL, pguid,
				&GUID_SYSTEM_BUTTON_SUBGROUP,
				&GUID_LIDCLOSE_ACTION,
				fDC))
	{
		PowerWriteACValueIndex (NULL, pguid,
				&GUID_SYSTEM_BUTTON_SUBGROUP,
				&GUID_LIDCLOSE_ACTION,
				fAC);
		return FALSE;
	}
	if (!fUpdate) return TRUE;

	if (ERROR_SUCCESS == PowerSetActiveScheme (NULL, pguid))
		return TRUE;
	else
		return FALSE;
}

GUID *Get_Scheme (DWORD *pfAC, DWORD *pfDC)
{
	GUID *pguid;

	if (ERROR_SUCCESS != PowerGetActiveScheme (NULL, &pguid))
		return NULL;
	if (ERROR_SUCCESS != PowerReadACValueIndex (NULL, pguid,
				&GUID_SYSTEM_BUTTON_SUBGROUP,
                &GUID_LIDCLOSE_ACTION,
				pfAC))
		return NULL;
	if (ERROR_SUCCESS != PowerReadDCValueIndex (NULL, pguid,
				&GUID_SYSTEM_BUTTON_SUBGROUP,
				&GUID_LIDCLOSE_ACTION,
				pfDC))
		return NULL;

	return pguid;
}
