#include "Config.hpp"
#include "OptionsDialog.hpp"
#include "ShellNotification.hpp"
#include "Main.hpp"
#include "config/LocalSettings.hpp"

#include <uxtheme.h>
#include <commdlg.h>

const eMessageStyle g_indexToMessageStyle[] = {
	MS_3DFACE,
	MS_GRADIENT,
	MS_FLAT,
	MS_FLATBR,
	MS_IMAGE,
};

const int g_indexToUserInterfaceScale[] = {
	1000,
	800,
	1200,
	1500,
};

enum ePage
{
	PG_ACCOUNT_AND_PRIVACY,
	PG_APPEARANCE,
	PG_NOTIFICATIONS,
	PG_CHAT,
	PG_WINDOW,
	PG_CONNECTION,
	PG_PAGE_COUNT,
	PG_FIRST = PG_ACCOUNT_AND_PRIVACY
};

#define C_PAGES (int(PG_PAGE_COUNT))

#pragma pack(push, 1)
typedef struct
{
	WORD dlgVer;
	WORD signature;
	DWORD helpID;
	DWORD exStyle;
	DWORD style;
	WORD cDlgItems;
	short x;
	short y;
	short cx;
	short cy;
}
DLGTEMPLATEEX; // huh??
#pragma pack(pop)

struct DialogHeader
{
	HWND hwndNav = NULL;     // settings navigation
	HWND hwndDisplay = NULL; // current child dialog box
    HWND hwndHeading = NULL;
    HWND hwndDone=NULL;
	RECT rcDisplay{};
	DLGTEMPLATEEX* apRes[C_PAGES] = { NULL };
	int  pageNum = 0;
};

static const char* const kMessageFonts[]={
    "Noto Sans","Lato","Atkinson Hyperlegible","Segoe UI","Calibri","Candara","Corbel","Verdana","Tahoma","Trebuchet MS","gg sans","Inter","Open Sans","Source Sans 3","Roboto"
};

void AddSettingsPage(HWND navigation,int index,LPCTSTR text) {
    SendMessage(navigation,LB_INSERTSTRING,index,(LPARAM)text);
}

static BOOL CALLBACK PreferenceSection(HWND control,LPARAM) {
    wchar_t name[32]={};GetClassNameW(control,name,32);
    if(!wcscmp(name,L"Button")){
        LONG_PTR style=GetWindowLongPtr(control,GWL_STYLE);
        if((style&BS_TYPEMASK)==BS_GROUPBOX){SetPropW(control,L"DMPrefsSection",(HANDLE)1);SetWindowFont(control,g_CategoryFont,TRUE);}
        else SetWindowLongPtr(control,GWL_STYLE,style|BS_NOTIFY);
    }
    return TRUE;
}

DLGTEMPLATEEX* LockDialogResource(LPCTSTR lpszResName)
{
	HRSRC hrsrc = FindResource(NULL, lpszResName, RT_DIALOG);
	HGLOBAL hglb = LoadResource(g_hInstance, hrsrc);
	return (DLGTEMPLATEEX*)LockResource(hglb);
}

// XXX: Shim to avoid linking against uxtheme.lib
HRESULT XEnableThemeDialogTexture(HWND hWnd, DWORD dwFlags)
{
	typedef HRESULT(STDAPICALLTYPE *pEnableThemeDialogTexture)(HWND, DWORD);

	static HMODULE hndl = (HMODULE)INVALID_HANDLE_VALUE;
	static pEnableThemeDialogTexture etdt;
	static bool etdtLoaded = false;

	if (!etdtLoaded)
	{
		if (hndl == INVALID_HANDLE_VALUE)
			hndl = LoadLibrary(TEXT("uxtheme.dll"));

		if (!hndl)
			return S_OK; // simulate OK

		etdt = (pEnableThemeDialogTexture)GetProcAddress(hndl, "EnableThemeDialogTexture");
		etdtLoaded = true;
	}

	if (!etdt) {
		auto err = GetLastError();
		return S_OK;
	}

	return etdt(hWnd, dwFlags);
}

void OptionsInitPage(HWND hwndDlg, int pageNum)
{
	//if the page is 0 (Account & Privacy)
	switch (pageNum)
	{
		case PG_ACCOUNT_AND_PRIVACY:
		{
			Profile* pProfile = GetDiscordInstance()->GetProfile();

			HWND hwnd;

			hwnd = GetDlgItem(hwndDlg, IDC_MY_ACCOUNT_NAME);

			std::string name = pProfile->m_name;
			if (pProfile->m_discrim)
				name += "#" + FormatDiscrim(pProfile->m_discrim);

			LPCTSTR lpctstr = ConvertCppStringToTString(name);
			SetWindowText(hwnd, lpctstr);
			free((void*)lpctstr);

			hwnd = GetDlgItem(hwndDlg, IDC_STATIC_PROFILE_IMAGE);

			bool unusedHasAlpha = false;
			HBITMAP hbm = GetAvatarCache()->GetImage(pProfile->m_avatarlnk, unusedHasAlpha)->GetFirstFrame();
			SendMessage(hwnd, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)hbm);

			eExplicitFilter filter = GetSettingsManager()->GetExplicitFilter();
			CheckRadioButton(hwndDlg, IDC_SDM_LEVEL0, IDC_SDM_LEVEL2, IDC_SDM_LEVEL0 + int(filter));

			CheckDlgButton(hwndDlg, IDC_ENABLE_DMS, GetSettingsManager()->GetDMBlockDefault() ? BST_UNCHECKED : BST_CHECKED);
			break;
		}
		case PG_APPEARANCE:
		{
			CheckRadioButton(
				hwndDlg,
				IDC_APPEARANCE_COZY,
				IDC_APPEARANCE_COMPACT,
				GetSettingsManager()->GetMessageCompact() ? IDC_APPEARANCE_COMPACT : IDC_APPEARANCE_COZY
			);

            const auto& fonts=kMessageFonts;
            for(auto font:fonts){std::string label=font;if(label=="gg sans" || label=="Inter" || label=="Open Sans" || label=="Source Sans 3" || label=="Roboto")label+=" (if installed)";auto text=ConvertCppStringToTString(label);int idx=(int)SendDlgItemMessage(hwndDlg,IDC_MESSAGE_FONT,CB_ADDSTRING,0,(LPARAM)text);free(text);if(GetLocalSettings()->MessageFont()==font)SendDlgItemMessage(hwndDlg,IDC_MESSAGE_FONT,CB_SETCURSEL,idx,0);}
            SendDlgItemMessage(hwndDlg,IDC_MESSAGE_FONT,CB_SETDROPPEDWIDTH,ScaleByDPI(300),0);
            for(int size=10;size<=24;++size){TCHAR text[8];_stprintf(text,TEXT("%d px"),size);int idx=(int)SendDlgItemMessage(hwndDlg,IDC_MESSAGE_SIZE,CB_ADDSTRING,0,(LPARAM)text);if(size==GetLocalSettings()->MessageFontSize())SendDlgItemMessage(hwndDlg,IDC_MESSAGE_SIZE,CB_SETCURSEL,idx,0);}
            CheckDlgButton(hwndDlg,IDC_ROUND_AVATARS,GetLocalSettings()->RoundAvatars()?BST_CHECKED:BST_UNCHECKED);
            unsigned c=GetLocalSettings()->ChannelHover();TCHAR hex[16];_stprintf(hex,TEXT("%02X%02X%02X"),c&255,(c>>8)&255,(c>>16)&255);SetDlgItemText(hwndDlg,IDC_CHANNEL_HOVER,hex);
            SendDlgItemMessage(hwndDlg,IDC_CHANNEL_HOVER,EM_SETLIMITTEXT,7,0);
			CheckDlgButton(hwndDlg, IDC_DARK_MODE, GetLocalSettings()->EnableDarkMode() ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_DOUBLE_BUFFERING, GetLocalSettings()->UseDoubleBuffering() ? BST_CHECKED : BST_UNCHECKED);

			if (ShouldBlockDoubleBuffering()) {
				EnableWindow(GetDlgItem(hwndDlg, IDC_DOUBLE_BUFFERING), false);
			}

			CheckDlgButton(hwndDlg, IDC_COMPACT_MEMBER_LIST, GetLocalSettings()->GetCompactMemberList() ? BST_CHECKED : BST_UNCHECKED);

			LPTSTR tstr = ConvertCppStringToTString(GetLocalSettings()->GetImageBackgroundFileName());
			SetDlgItemText(hwndDlg, IDC_ACTIVE_IMAGE_EDIT, tstr);
			free(tstr);

			HWND hCBox = GetDlgItem(hwndDlg, IDC_MESSAGE_STYLE);

			const eMessageStyle* table = g_indexToMessageStyle;
			size_t tableCount = _countof(g_indexToMessageStyle);

			// NOTE: these must match the order in the specified table!
			ComboBox_AddString(hCBox, TEXT("3-D frame"));

			ComboBox_AddString(hCBox, TEXT("Gradient"));

			ComboBox_AddString(hCBox, TEXT("Flat color 1 (3-D face color)"));
			ComboBox_AddString(hCBox, TEXT("Flat color 2 (window color)"));
			ComboBox_AddString(hCBox, TEXT("Flat with image background"));

			// determine message style selection
			ComboBox_SetCurSel(hCBox, 0);
			eMessageStyle msgStyle = GetLocalSettings()->GetMessageStyle();
			for (size_t i = 0; i < tableCount; i++) {
				if (msgStyle == table[i]) {
					ComboBox_SetCurSel(hCBox, i);
					break;
				}
			}

			hCBox = GetDlgItem(hwndDlg, IDC_COMBO_ALIGNMENT);
			// NOTE: these must match the order in eImageAlignment!
			ComboBox_AddString(hCBox, TEXT("Lower right"));
			ComboBox_AddString(hCBox, TEXT("Upper left"));
			ComboBox_AddString(hCBox, TEXT("Center"));
			ComboBox_AddString(hCBox, TEXT("Upper right"));
			ComboBox_AddString(hCBox, TEXT("Lower left"));
			ComboBox_AddString(hCBox, TEXT("Upper center"));
			ComboBox_AddString(hCBox, TEXT("Lower center"));
			ComboBox_AddString(hCBox, TEXT("Middle left"));
			ComboBox_AddString(hCBox, TEXT("Middle right"));
			ComboBox_SetCurSel(hCBox, int(GetLocalSettings()->GetImageAlignment()));

			hCBox = GetDlgItem(hwndDlg, IDC_COMBO_GUI_SCALE);
			ComboBox_AddString(hCBox, TEXT("Normal (100%)"));
			ComboBox_AddString(hCBox, TEXT("Small (80%)"));
			ComboBox_AddString(hCBox, TEXT("Large (120%)"));
			ComboBox_AddString(hCBox, TEXT("Extra Large (150%)"));

			// determine user scale selection
			ComboBox_SetCurSel(hCBox, 0);
			int userScale = GetLocalSettings()->GetUserScale();
			for (size_t i = 0; i < _countof(g_indexToUserInterfaceScale); i++) {
				if (userScale == g_indexToUserInterfaceScale[i]) {
					ComboBox_SetCurSel(hCBox, i);
					break;
				}
			}

			bool isWatermarkStyle = msgStyle == MS_IMAGE;
			EnableWindow(GetDlgItem(hwndDlg, IDC_ACTIVE_IMAGE_EDIT),   isWatermarkStyle);
			EnableWindow(GetDlgItem(hwndDlg, IDC_ACTIVE_IMAGE_BROWSE), isWatermarkStyle);
			EnableWindow(GetDlgItem(hwndDlg, IDC_COMBO_ALIGNMENT),     isWatermarkStyle);

			break;
		}
		case PG_NOTIFICATIONS:
		{
			CheckDlgButton(hwndDlg, IDC_ENABLE_BALLOON_NOTIFS, GetLocalSettings()->EnableNotifications() ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_FLASH_TASKBAR,         GetLocalSettings()->FlashOnNotification() ? BST_CHECKED : BST_UNCHECKED);
			break;
		}
		case PG_CHAT:
		{
			CheckDlgButton(hwndDlg, IDC_DISABLE_FORMATTING,    GetLocalSettings()->DisableFormatting()    ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_USE_12HR_TIME,         GetLocalSettings()->Use12HourTime()        ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_SHOW_BLOCKED_MESSAGES, GetLocalSettings()->ShowBlockedMessages()  ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_IMAGES_WHEN_UPLOADED,  GetLocalSettings()->ShowAttachmentImages() ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_IMAGES_WHEN_EMBEDDED,  GetLocalSettings()->ShowEmbedImages()      ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_SHOW_EMBEDS,           GetLocalSettings()->ShowEmbedContent()     ? BST_CHECKED : BST_UNCHECKED);
			break;
		}
		case PG_WINDOW:
		{
			CheckDlgButton(hwndDlg, IDC_SAVE_WINDOW_SIZE,  GetLocalSettings()->GetSaveWindowSize()  ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_START_MAXIMIZED,   GetLocalSettings()->GetStartMaximized()  ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_OPEN_ON_STARTUP,   GetLocalSettings()->GetOpenOnStartup()   ? BST_CHECKED : BST_UNCHECKED);
			
			CheckDlgButton(hwndDlg, IDC_START_MINIMIZED,   GetLocalSettings()->GetStartMinimized()  ? BST_CHECKED : BST_UNCHECKED);
			EnableWindow(GetDlgItem(hwndDlg, IDC_START_MINIMIZED), GetLocalSettings()->GetOpenOnStartup());

			CheckDlgButton(hwndDlg, IDC_MINIMIZE_TO_NOTIF, GetLocalSettings()->GetMinimizeToNotif() ? BST_CHECKED : BST_UNCHECKED);

			break;
		}
		case PG_CONNECTION:
		{
			LPTSTR tstrAPI = ConvertCppStringToTString(GetDiscordAPI());
			LPTSTR tstrCDN = ConvertCppStringToTString(GetDiscordCDN());

			SetDlgItemText(hwndDlg, IDC_EDIT_DISCORDAPI, tstrAPI);
			SetDlgItemText(hwndDlg, IDC_EDIT_DISCORDCDN, tstrCDN);

			CheckDlgButton(hwndDlg, IDC_ENABLE_TLS_CHECKS,  GetLocalSettings()->EnableTLSVerification() ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_ENABLE_DIAGNOSTICS, GetLocalSettings()->EnableDiagnostics() ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_CHECK_UPDATES,      GetLocalSettings()->CheckUpdatesOption()    ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_TOGGLE_XSUPERPROPS, GetLocalSettings()->AddExtraHeaders()       ? BST_CHECKED : BST_UNCHECKED);

			free(tstrAPI);
			free(tstrCDN);

			break;
		}
	}

}

void WINAPI OnChildDialogInit(HWND hwndDlg)
{
	HWND hwndParent = GetParent(hwndDlg);
	DialogHeader* pHdr = (DialogHeader*)GetWindowLongPtr(hwndParent, GWLP_USERDATA);

	SetWindowPos(hwndDlg, NULL, pHdr->rcDisplay.left,
		pHdr->rcDisplay.top,//-2,
		(pHdr->rcDisplay.right - pHdr->rcDisplay.left),
		(pHdr->rcDisplay.bottom - pHdr->rcDisplay.top),
		SWP_SHOWWINDOW);

    SetWindowLongPtr(hwndDlg,GWL_EXSTYLE,GetWindowLongPtr(hwndDlg,GWL_EXSTYLE)|WS_EX_CONTROLPARENT);
    EnumChildWindows(hwndDlg,PreferenceSection,0);

	int pageNum = pHdr->pageNum;

	OptionsInitPage(hwndDlg, pageNum);
}

INT_PTR OptionsHandleCommand(HWND hwndParent, HWND hWnd, int pageNum, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (LOWORD(wParam) == IDOK)
		return EndDialog(hWnd, IDOK);
	
	switch (pageNum)
	{
		case PG_ACCOUNT_AND_PRIVACY:
		{
			switch (LOWORD(wParam))
			{
				case IDC_REVEALEMAIL: {
					Profile* pProfile = GetDiscordInstance()->GetProfile();
					HWND hwndText = GetDlgItem(hWnd, IDC_EMAIL_DISPLAY);
					LPCTSTR lpctstr = ConvertCppStringToTString(pProfile->m_email);
					SetWindowText(hwndText, lpctstr);
					free((void*)lpctstr);
					break;
				}
				case IDC_LOG_OUT: {
					if (MessageBox(hWnd, TmGetTString(IDS_LOG_OUT_MESSAGE), TmGetTString(IDS_PROGRAM_NAME), MB_YESNO | MB_ICONQUESTION) == IDYES)
					{
						// Log them out!
						if (hWnd != hwndParent)
							EndDialog(hWnd, 0);
						EndDialog(hwndParent, OPTIONS_RESULT_LOGOUT);
						return TRUE;
					}
					break;
				}
				case IDC_SDM_LEVEL0:
					GetSettingsManager()->SetExplicitFilter(FILTER_NONE);
					GetSettingsManager()->FlushSettings();
					break;
				case IDC_SDM_LEVEL1:
					GetSettingsManager()->SetExplicitFilter(FILTER_EXCEPTFRIENDS);
					GetSettingsManager()->FlushSettings();
					break;
				case IDC_SDM_LEVEL2:
					GetSettingsManager()->SetExplicitFilter(FILTER_TOTAL);
					GetSettingsManager()->FlushSettings();
					break;
				case IDC_ENABLE_DMS:
				{
					bool updateAllServers = MessageBox(
						hwndParent,
						TmGetTString(IDS_DMBLOCK_APPLY_RETROACTIVELY),
						TmGetTString(IDS_PROGRAM_NAME),
						MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2
					) == IDYES;

					bool isChecked = IsDlgButtonChecked(hWnd, IDC_ENABLE_DMS);

					std::vector<Snowflake> blocklist;
					if (updateAllServers)
					{
						if (!isChecked)
							GetDiscordInstance()->GetGuildIDs(blocklist, false);

						GetSettingsManager()->SetGuildDMBlocklist(blocklist);
					}

					GetSettingsManager()->SetDMBlockDefault(!isChecked);
					GetSettingsManager()->FlushSettings();
					break;
				}
			}
			break;
		}
		case PG_APPEARANCE:
		{
			switch (LOWORD(wParam))
			{
                case IDC_MESSAGE_FONT:
                    if(HIWORD(wParam)==CBN_SELCHANGE){const auto& fonts=kMessageFonts;int idx=(int)SendDlgItemMessage(hWnd,IDC_MESSAGE_FONT,CB_GETCURSEL,0,0);if(idx>=0 && idx<int(_countof(kMessageFonts)))GetLocalSettings()->SetMessageFont(fonts[idx]);GetLocalSettings()->Save();}break;
                case IDC_MESSAGE_SIZE:
                    if(HIWORD(wParam)==CBN_SELCHANGE){int idx=(int)SendDlgItemMessage(hWnd,IDC_MESSAGE_SIZE,CB_GETCURSEL,0,0);if(idx!=CB_ERR)GetLocalSettings()->SetMessageFontSize(idx+10);GetLocalSettings()->Save();}break;
                case IDC_ROUND_AVATARS:
                    GetLocalSettings()->SetRoundAvatars(IsDlgButtonChecked(hWnd,IDC_ROUND_AVATARS)==BST_CHECKED);GetLocalSettings()->Save();SendMessage(g_Hwnd,WM_RECALCMSGLIST,0,0);break;
                case IDC_CHANNEL_HOVER:
                    if(HIWORD(wParam)==EN_KILLFOCUS){TCHAR hex[16];GetDlgItemText(hWnd,IDC_CHANNEL_HOVER,hex,16);TCHAR* value=hex[0]==TEXT('#')?hex+1:hex;TCHAR* end=NULL;unsigned long rgb=_tcstoul(value,&end,16);if(_tcslen(value)==6 && end && !*end){GetLocalSettings()->SetChannelHover(RGB(rgb>>16&255,rgb>>8&255,rgb&255));GetLocalSettings()->Save();}}break;
				case IDC_MESSAGE_STYLE:
				{
					if (HIWORD(wParam) != CBN_SELCHANGE)
						break;

					const eMessageStyle* table = g_indexToMessageStyle;
					size_t tableCount = _countof(g_indexToMessageStyle);

					int sel = ComboBox_GetCurSel((HWND) lParam);
					if (sel == CB_ERR || sel < 0 || sel >= int(tableCount))
						break;

					eMessageStyle style = table[sel];
					GetLocalSettings()->SetMessageStyle(style);

					bool enable = style == MS_IMAGE;
					EnableWindow(GetDlgItem(hWnd, IDC_ACTIVE_IMAGE_EDIT),   enable);
					EnableWindow(GetDlgItem(hWnd, IDC_ACTIVE_IMAGE_BROWSE), enable);
					EnableWindow(GetDlgItem(hWnd, IDC_COMBO_ALIGNMENT),     enable);
					
					SendMessage(g_Hwnd, WM_MSGLISTUPDATEMODE, 0, 0);
					break;
				}
				case IDC_COMBO_ALIGNMENT:
				{
					if (HIWORD(wParam) != CBN_SELCHANGE)
						break;

					int sel = ComboBox_GetCurSel((HWND)lParam);
					if (sel == CB_ERR || sel < 0 || sel >= int(ALIGN_COUNT))
						break;

					eImageAlignment align = eImageAlignment(sel);
					GetLocalSettings()->SetImageAlignment(align);

					if (GetLocalSettings()->GetMessageStyle() == MS_IMAGE)
						SendMessage(g_Hwnd, WM_MSGLISTUPDATEMODE, 0, 0);

					break;
				}
				case IDC_COMBO_GUI_SCALE:
				{
					if (HIWORD(wParam) != CBN_SELCHANGE)
						break;

					int sel = ComboBox_GetCurSel((HWND)lParam);
					if (sel == CB_ERR || sel < 0 || sel >= int(_countof(g_indexToUserInterfaceScale)))
						break;

					GetLocalSettings()->SetUserScale(g_indexToUserInterfaceScale[sel]);
					MessageBox(hWnd, TmGetTString(IDS_GUI_SCALE_CHANGED), TmGetTString(IDS_PROGRAM_NAME), MB_OK | MB_ICONINFORMATION);
					break;
				}
				case IDC_ACTIVE_IMAGE_BROWSE:
				{
					const int MAX_FILE = 4096;
					TCHAR buffer[MAX_FILE];
					buffer[0] = 0;

				#ifdef WEBP_SUP
				#define COND_WEBP ";*.webp"
				#else
				#define COND_WEBP ""
				#endif

					OPENFILENAME ofn{};
					ofn.lStructSize    = SIZEOF_OPENFILENAME_NT4;
					ofn.hwndOwner      = g_Hwnd;
					ofn.hInstance      = g_hInstance;
					ofn.nMaxFile       = MAX_FILE;
					ofn.lpstrFile      = buffer;
					ofn.nMaxFileTitle  = 0;
					ofn.lpstrFileTitle = NULL;
					ofn.lpstrFilter    = TEXT("Image files\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tga" COND_WEBP "\0\0");
					ofn.lpstrTitle     = TmGetTString(IDS_SELECT_BACKGROUND_IMAGE);

					if (!GetOpenFileName(&ofn)) {
						// operation cancelled
						break;
					}

					std::string fileName = MakeStringFromTString(ofn.lpstrFile);
					GetLocalSettings()->SetImageBackgroundFileName(fileName);
					SendMessage(g_Hwnd, WM_MSGLISTUPDATEMODE, 0, 0);
					SetDlgItemText(hWnd, IDC_ACTIVE_IMAGE_EDIT, ofn.lpstrFile);
					break;
				}

				case IDC_APPEARANCE_COZY:
					GetSettingsManager()->SetMessageCompact(false);
					GetSettingsManager()->FlushSettings();
					SendMessage(g_Hwnd, WM_RECALCMSGLIST, 0, 0);
					break;
				case IDC_APPEARANCE_COMPACT:
					GetSettingsManager()->SetMessageCompact(true);
					GetSettingsManager()->FlushSettings();
					SendMessage(g_Hwnd, WM_RECALCMSGLIST, 0, 0);
					break;
				case IDC_COMPACT_MEMBER_LIST:
					GetLocalSettings()->SetCompactMemberList(IsDlgButtonChecked(hWnd, IDC_COMPACT_MEMBER_LIST));
					SendMessage(g_Hwnd, WM_RECREATEMEMBERLIST, 0, 0);
					break;
				case IDC_DARK_MODE:
					GetLocalSettings()->SetEnableDarkMode(IsDlgButtonChecked(hWnd, IDC_DARK_MODE) == BST_CHECKED);
					GetLocalSettings()->Save();
					break;
				case IDC_DOUBLE_BUFFERING:
					GetLocalSettings()->SetUseDoubleBuffering(IsDlgButtonChecked(hWnd, IDC_DOUBLE_BUFFERING));
					break;
			}
			break;
		}
		case PG_NOTIFICATIONS:
		{
			switch (LOWORD(wParam))
			{
				case IDC_ENABLE_BALLOON_NOTIFS:
					GetLocalSettings()->SetEnableNotifications(IsDlgButtonChecked(hWnd, IDC_ENABLE_BALLOON_NOTIFS));
					break;
				case IDC_FLASH_TASKBAR:
					GetLocalSettings()->SetFlashOnNotification(IsDlgButtonChecked(hWnd, IDC_FLASH_TASKBAR));
					break;
			}
			break;
		}
		case PG_CHAT:
		{
			switch (LOWORD(wParam))
			{
				case IDC_DISABLE_FORMATTING:
					GetLocalSettings()->SetDisableFormatting(IsDlgButtonChecked(hWnd, IDC_DISABLE_FORMATTING));
					SendMessage(g_Hwnd, WM_RECALCMSGLIST, 0, 0);
					break;
				case IDC_USE_12HR_TIME:
					GetLocalSettings()->SetUse12HourTime(IsDlgButtonChecked(hWnd, IDC_USE_12HR_TIME));
					SendMessage(g_Hwnd, WM_RECALCMSGLIST, 0, 0);
					break;
				case IDC_SHOW_BLOCKED_MESSAGES:
					GetLocalSettings()->SetShowBlockedMessages(IsDlgButtonChecked(hWnd, IDC_SHOW_BLOCKED_MESSAGES));
					SendMessage(g_Hwnd, WM_RECALCMSGLIST, 0, 0);
					break;
				case IDC_IMAGES_WHEN_UPLOADED:
					GetLocalSettings()->SetShowAttachmentImages(IsDlgButtonChecked(hWnd, IDC_IMAGES_WHEN_UPLOADED));
					SendMessage(g_Hwnd, WM_RECALCMSGLIST, 0, 0);
					break;
				case IDC_IMAGES_WHEN_EMBEDDED:
					GetLocalSettings()->SetShowEmbedImages(IsDlgButtonChecked(hWnd, IDC_IMAGES_WHEN_EMBEDDED));
					SendMessage(g_Hwnd, WM_RECALCMSGLIST, 0, 0);
					break;
				case IDC_SHOW_EMBEDS:
					GetLocalSettings()->SetShowEmbedContent(IsDlgButtonChecked(hWnd, IDC_SHOW_EMBEDS));
					SendMessage(g_Hwnd, WM_RECALCMSGLIST, 0, 0);
					break;
			}
			break;
		}
		case PG_WINDOW:
		{
			switch (LOWORD(wParam))
			{
				case IDC_SAVE_WINDOW_SIZE:
					GetLocalSettings()->SetSaveWindowSize(IsDlgButtonChecked(hWnd, IDC_SAVE_WINDOW_SIZE));
					break;
				case IDC_START_MAXIMIZED:
					GetLocalSettings()->SetStartMaximized(IsDlgButtonChecked(hWnd, IDC_START_MAXIMIZED));
					break;
				case IDC_OPEN_ON_STARTUP:
				{
					const bool checked = IsDlgButtonChecked(hWnd, IDC_OPEN_ON_STARTUP);
					GetLocalSettings()->SetOpenOnStartup(checked);
					EnableWindow(GetDlgItem(hWnd, IDC_START_MINIMIZED), checked);
					break;
				}
				case IDC_START_MINIMIZED:
					GetLocalSettings()->SetStartMinimized(IsDlgButtonChecked(hWnd, IDC_START_MINIMIZED));
					break;
				case IDC_MINIMIZE_TO_NOTIF:
					GetLocalSettings()->SetMinimizeToNotif(IsDlgButtonChecked(hWnd, IDC_MINIMIZE_TO_NOTIF));
					break;
			}
			break;
		}
		case PG_CONNECTION:
		{
			switch (LOWORD(wParam))
			{
				case IDC_ENABLE_TLS_CHECKS:
				{
					bool state = IsDlgButtonChecked(hWnd, IDC_ENABLE_TLS_CHECKS);
					if (!state) {
						if (MessageBox(hWnd, TmGetTString(IDS_CERT_SETTING_CONFIRM), TmGetTString(IDS_PROGRAM_NAME), MB_ICONWARNING | MB_YESNO) != IDYES)
						{
							CheckDlgButton(hWnd, IDC_ENABLE_TLS_CHECKS, BST_CHECKED);
							break;
						}
					}

					GetLocalSettings()->SetEnableTLSVerification(state);
					break;
				}

				case IDC_ENABLE_DIAGNOSTICS:
					GetLocalSettings()->SetEnableDiagnostics(IsDlgButtonChecked(hWnd, IDC_ENABLE_DIAGNOSTICS) == BST_CHECKED);
					GetLocalSettings()->Save();
					break;
				case IDC_CHECK_UPDATES:
				{
					GetLocalSettings()->SetCheckUpdates(IsDlgButtonChecked(hWnd, IDC_CHECK_UPDATES));
					break;
				}

				case IDC_TOGGLE_XSUPERPROPS:
				{
					GetLocalSettings()->SetAddExtraHeaders(IsDlgButtonChecked(hWnd, IDC_TOGGLE_XSUPERPROPS));
					break;
				}

				case IDC_REVERTTODEFAULT:
				case IDC_UPDATE:
				{
					std::string api, cdn;

					if (wParam == IDC_REVERTTODEFAULT)
					{
						api = OFFICIAL_DISCORD_API;
						cdn = OFFICIAL_DISCORD_CDN;
					}
					else
					{
						TCHAR tchrAPI[512], tchrCDN[512];
						if (!GetDlgItemText(hWnd, IDC_EDIT_DISCORDAPI, tchrAPI, _countof(tchrAPI)) ||
							!GetDlgItemText(hWnd, IDC_EDIT_DISCORDCDN, tchrCDN, _countof(tchrCDN))) {
							MessageBox(hWnd, TmGetTString(IDS_URL_EMPTY), TmGetTString(IDS_PROGRAM_NAME), MB_ICONERROR | MB_OK);
							break;
						}

						api = MakeStringFromTString(tchrAPI);
						cdn = MakeStringFromTString(tchrCDN);
					}

					if (api.empty() || cdn.empty()) {
						MessageBox(hWnd, TmGetTString(IDS_URL_EMPTY), TmGetTString(IDS_PROGRAM_NAME), MB_ICONERROR | MB_OK);
						break;
					}

					if (api[api.size() - 1] != '/') api += '/';
					if (cdn[cdn.size() - 1] != '/') cdn += '/';

					if (MessageBox(hWnd, TmGetTString(wParam == IDC_UPDATE ? IDS_CONFIRM_SET_URLS : IDS_CONFIRM_OFFICIAL_URLS),
						TmGetTString(IDS_PROGRAM_NAME), MB_ICONQUESTION | MB_YESNO) != IDYES)
						break;
							
					// Ok!... your choice.
					GetLocalSettings()->SetToken("");
					GetLocalSettings()->SetDiscordAPI(api);
					GetLocalSettings()->SetDiscordCDN(cdn);
					GetDiscordInstance()->ResetGatewayURL();
					
					if (hWnd != hwndParent)
						EndDialog(hWnd, 0);
					
					EndDialog(hwndParent, OPTIONS_RESULT_LOGOUT);

					break;
				}
			}
			break;
		}
	}

	return 0;
}

static void ScrollSettingsPage(HWND page,int axis,int position) {
    SCROLLINFO si={};si.cbSize=sizeof(si);si.fMask=SIF_ALL;GetScrollInfo(page,axis,&si);
    int old=si.nPos;si.nPos=position;SetScrollInfo(page,axis,&si,TRUE);GetScrollInfo(page,axis,&si);
    ScrollWindowEx(page,axis==SB_HORZ?old-si.nPos:0,axis==SB_VERT?old-si.nPos:0,NULL,NULL,NULL,NULL,SW_SCROLLCHILDREN|SW_INVALIDATE|SW_ERASE);
}

INT_PTR CALLBACK OptionsChildDialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HWND hwndParent = GetParent(hWnd);
	DialogHeader* pHdr = (DialogHeader*)GetWindowLongPtr(hwndParent, GWLP_USERDATA);
    if(!pHdr)return FALSE;
	switch (uMsg)
	{
        case WM_SIZE: {
            if(GetPropW(hWnd,L"DMPrefsSizing"))return TRUE;
            SetPropW(hWnd,L"DMPrefsSizing",(HANDLE)1);
            RECT content={0,0,pHdr->apRes[pHdr->pageNum]->cx,pHdr->apRes[pHdr->pageNum]->cy};MapDialogRect(hWnd,&content);
            for(int pass=0;pass<2;++pass)for(int axis=0;axis<2;++axis){
                RECT view;GetClientRect(hWnd,&view);int bar=axis?SB_HORZ:SB_VERT;
                SCROLLINFO old={};old.cbSize=sizeof(old);old.fMask=SIF_POS;GetScrollInfo(hWnd,bar,&old);
                SCROLLINFO si={};si.cbSize=sizeof(si);si.fMask=SIF_RANGE|SIF_PAGE;si.nMax=(axis?content.right:content.bottom)-1;si.nPage=axis?view.right:view.bottom;
                SetScrollInfo(hWnd,bar,&si,TRUE);si.fMask=SIF_POS;GetScrollInfo(hWnd,bar,&si);
                if(si.nPos!=old.nPos)ScrollWindowEx(hWnd,axis?old.nPos-si.nPos:0,axis?0:old.nPos-si.nPos,NULL,NULL,NULL,NULL,SW_SCROLLCHILDREN|SW_INVALIDATE|SW_ERASE);
            }
            RemovePropW(hWnd,L"DMPrefsSizing");return TRUE;
        }
        case WM_VSCROLL: case WM_HSCROLL: {
            int axis=uMsg==WM_VSCROLL?SB_VERT:SB_HORZ;SCROLLINFO si={};si.cbSize=sizeof(si);si.fMask=SIF_ALL;GetScrollInfo(hWnd,axis,&si);
            int pos=si.nPos;
            switch(LOWORD(wParam)){
                case SB_LINEUP:pos-=ScaleByDPI(24);break;case SB_LINEDOWN:pos+=ScaleByDPI(24);break;
                case SB_PAGEUP:pos-=si.nPage;break;case SB_PAGEDOWN:pos+=si.nPage;break;
                case SB_THUMBTRACK:case SB_THUMBPOSITION:pos=si.nTrackPos;break;
                case SB_TOP:pos=si.nMin;break;case SB_BOTTOM:pos=si.nMax;break;
            }
            ScrollSettingsPage(hWnd,axis,pos);return TRUE;
        }
        case WM_MOUSEWHEEL: {
            SCROLLINFO si={};si.cbSize=sizeof(si);si.fMask=SIF_POS;GetScrollInfo(hWnd,SB_VERT,&si);
            ScrollSettingsPage(hWnd,SB_VERT,si.nPos-GET_WHEEL_DELTA_WPARAM(wParam)*ScaleByDPI(48)/WHEEL_DELTA);return TRUE;
        }

		case WM_COMMAND:
            if(lParam && GetFocus()==(HWND)lParam){
                RECT control,view;GetWindowRect((HWND)lParam,&control);MapWindowPoints(NULL,hWnd,(POINT*)&control,2);GetClientRect(hWnd,&view);
                for(int axis=0;axis<2;++axis){
                    int first=axis?control.left:control.top,last=axis?control.right:control.bottom,edge=axis?view.right:view.bottom;
                    int delta=first<0?first:last>edge?last-edge:0;
                    if(delta){SCROLLINFO si={};si.cbSize=sizeof(si);si.fMask=SIF_POS;GetScrollInfo(hWnd,axis?SB_HORZ:SB_VERT,&si);ScrollSettingsPage(hWnd,axis?SB_HORZ:SB_VERT,si.nPos+delta);}
                }
            }
            if(lParam){wchar_t name[32]={};GetClassNameW((HWND)lParam,name,32);
                if(!wcscmp(name,L"Button") && HIWORD(wParam)!=BN_CLICKED && HIWORD(wParam)!=BN_DOUBLECLICKED)return TRUE;
            }
			return OptionsHandleCommand(hwndParent, hWnd, pHdr->pageNum, uMsg, wParam, lParam);

		case WM_INITDIALOG:
		{
			OnChildDialogInit(hWnd);
			break;
		}
	}
	return 0L;
}

// Switch the selected settings page without changing its existing bindings.
// hwndDlg - handle to the parent dialog box.
//
void OnSelChanged(HWND hwndDlg)
{

	// Get the dialog header data.
	DialogHeader *pHdr = (DialogHeader*) GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

	// Get the selected navigation entry.
	int iSel = (int)SendMessage(pHdr->hwndNav,LB_GETCURSEL,0,0);
    if(iSel<0 || iSel>=C_PAGES)return;
    TCHAR heading[256]={};SendMessage(pHdr->hwndNav,LB_GETTEXT,iSel,(LPARAM)heading);SetWindowText(pHdr->hwndHeading,heading);
    if(pHdr->hwndDisplay)SetFocus(pHdr->hwndNav);

	// Destroy the current child dialog box, if any.
	if (pHdr->hwndDisplay != NULL)
		DestroyWindow(pHdr->hwndDisplay);

	// Create the new child dialog box. Note that g_hInst is the
	// global instance handle.
	pHdr->pageNum     = iSel;
	pHdr->hwndDisplay = CreateDialogIndirect(g_hInstance, (DLGTEMPLATE *)pHdr->apRes[iSel], hwndDlg, OptionsChildDialogProc);

	return;
}

static void LayoutPreferences(HWND window) {
    auto data=(DialogHeader*)GetWindowLongPtr(window,GWLP_USERDATA);if(!data || !data->hwndNav)return;
    RECT r;GetClientRect(window,&r);int pad=ScaleByDPI(16),nav=ScaleByDPI(175),title=ScaleByDPI(32);
    MoveWindow(data->hwndNav,pad,pad,nav,std::max(1,int(r.bottom)-pad*2),TRUE);
    MoveWindow(data->hwndHeading,nav+pad*2,pad,std::max(1,int(r.right)-nav-pad*3),title,TRUE);
    data->rcDisplay={nav+pad*2,pad+title,std::max(nav+pad*2+1,int(r.right)-pad),std::max(pad+title+1,int(r.bottom)-pad-ScaleByDPI(40))};
    MoveWindow(data->hwndDone,r.right-pad-ScaleByDPI(88),r.bottom-pad-ScaleByDPI(28),ScaleByDPI(88),ScaleByDPI(28),TRUE);
    if(data->hwndDisplay)MoveWindow(data->hwndDisplay,data->rcDisplay.left,data->rcDisplay.top,data->rcDisplay.right-data->rcDisplay.left,data->rcDisplay.bottom-data->rcDisplay.top,TRUE);
}

HRESULT OnPreferenceDialogInit(HWND hWnd)
{


	DialogHeader* pHeader = new DialogHeader;
	SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) pHeader);

	// Get the vertical settings navigation.
	HWND hwndNav = GetDlgItem(hWnd, IDC_OPTIONS_NAV);
	if (!hwndNav) {
		delete pHeader;
		return HRESULT_FROM_WIN32(GetLastError());
	}

    SetPropW(hwndNav,L"DMPrefsNavigation",(HANDLE)1);
	pHeader->hwndNav = hwndNav;
    SendMessage(hwndNav,LB_SETITEMHEIGHT,0,ScaleByDPI(36));SetWindowFont(hwndNav,g_MessageTextFont,TRUE);
    pHeader->hwndHeading=CreateWindow(TEXT("STATIC"),TEXT(""),WS_CHILD|WS_VISIBLE,0,0,1,1,hWnd,NULL,g_hInstance,NULL);
    SetWindowFont(pHeader->hwndHeading,g_GuildCaptionFont,TRUE);
    pHeader->hwndDone=CreateWindow(TEXT("BUTTON"),TEXT("Done"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,0,0,1,1,hWnd,(HMENU)IDC_PREFS_DONE,g_hInstance,NULL);
    SetWindowFont(pHeader->hwndDone,g_MessageTextFont,TRUE);

	// Add settings pages; navigation scrolls vertically as more pages are added.

	AddSettingsPage(hwndNav, PG_ACCOUNT_AND_PRIVACY, (LPTSTR)TmGetTString(IDS_ACCOUNT_PRIVACY));
	AddSettingsPage(hwndNav, PG_APPEARANCE,          (LPTSTR)TmGetTString(IDS_APPEARANCE));
	AddSettingsPage(hwndNav, PG_NOTIFICATIONS,       (LPTSTR)TmGetTString(IDS_NOTIFICATIONS));
	AddSettingsPage(hwndNav, PG_CHAT,                (LPTSTR)TmGetTString(IDS_CHAT));
	AddSettingsPage(hwndNav, PG_WINDOW,              (LPTSTR)TmGetTString(IDS_WINDOW));
	AddSettingsPage(hwndNav, PG_CONNECTION,          (LPTSTR)TmGetTString(IDS_CONNECTION));

	// Lock the resources for the child dialog boxes.
	pHeader->apRes[PG_ACCOUNT_AND_PRIVACY] = LockDialogResource(MAKEINTRESOURCE(IDD_DIALOG_MY_ACCOUNT));
	pHeader->apRes[     PG_APPEARANCE    ] = LockDialogResource(MAKEINTRESOURCE(IDD_DIALOG_APPEARANCE));
	pHeader->apRes[    PG_NOTIFICATIONS  ] = LockDialogResource(MAKEINTRESOURCE(IDD_DIALOG_NOTIFSETTINGS));
	pHeader->apRes[        PG_CHAT       ] = LockDialogResource(MAKEINTRESOURCE(IDD_DIALOG_CHATSETTINGS));
	pHeader->apRes[       PG_WINDOW      ] = LockDialogResource(MAKEINTRESOURCE(IDD_DIALOG_WINDOWSETTINGS));
	pHeader->apRes[     PG_CONNECTION    ] = LockDialogResource(MAKEINTRESOURCE(IDD_DIALOG_CONNECTION));

	RECT rcTab;
	SetRectEmpty(&rcTab);

	for (int i = 0; i < C_PAGES; i++)
	{
		if (rcTab.right < pHeader->apRes[i]->cx)
			rcTab.right = pHeader->apRes[i]->cx;
		if (rcTab.bottom < pHeader->apRes[i]->cy)
			rcTab.bottom = pHeader->apRes[i]->cy;
	}

	MapDialogRect(hWnd, &rcTab);

    RECT desired={0,0,ScaleByDPI(175)+rcTab.right+ScaleByDPI(48),rcTab.bottom+ScaleByDPI(104)};
    AdjustWindowRectEx(&desired,(DWORD)GetWindowLongPtr(hWnd,GWL_STYLE),FALSE,(DWORD)GetWindowLongPtr(hWnd,GWL_EXSTYLE));
    MONITORINFO monitor={sizeof(monitor)};GetMonitorInfo(MonitorFromWindow(hWnd,MONITOR_DEFAULTTONEAREST),&monitor);
    int width=std::min(int(desired.right-desired.left),int(monitor.rcWork.right-monitor.rcWork.left));
    int height=std::min(int(desired.bottom-desired.top),int(monitor.rcWork.bottom-monitor.rcWork.top));
    SetWindowPos(hWnd,NULL,monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-width)/2,monitor.rcWork.top+(monitor.rcWork.bottom-monitor.rcWork.top-height)/2,width,height,SWP_NOZORDER);
    LayoutPreferences(hWnd);
    SendMessage(hwndNav,LB_SETCURSEL,0,0);
	// Simulate selection of the first item.
	OnSelChanged(hWnd);

	return S_OK;
}

static INT_PTR CALLBACK DialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
        case WM_SIZE: LayoutPreferences(hWnd);return TRUE;
        case WM_GETMINMAXINFO: {
            auto info=(MINMAXINFO*)lParam;info->ptMinTrackSize={ScaleByDPI(480),ScaleByDPI(360)};return TRUE;
        }
        case WM_MEASUREITEM:
            if(wParam==IDC_OPTIONS_NAV){((MEASUREITEMSTRUCT*)lParam)->itemHeight=ScaleByDPI(36);return TRUE;}break;
        case WM_DRAWITEM: {
            auto item=(DRAWITEMSTRUCT*)lParam;
            if(item->CtlID!=IDC_OPTIONS_NAV || item->itemID==UINT(-1))break;
            TCHAR text[256]={};SendMessage(item->hwndItem,LB_GETTEXT,item->itemID,(LPARAM)text);
            int saved=SaveDC(item->hDC);bool selected=(item->itemState&ODS_SELECTED)!=0;
            COLORREF bg=IsDarkTheme()?(selected?RGB(53,54,60):ThemePalette::Canvas):ThemeColor(selected?COLOR_HIGHLIGHT:COLOR_WINDOW);
            SetDCBrushColor(item->hDC,bg);FillRect(item->hDC,&item->rcItem,(HBRUSH)GetStockObject(DC_BRUSH));
            SetTextColor(item->hDC,ThemeColor(selected?COLOR_HIGHLIGHTTEXT:COLOR_WINDOWTEXT));SetBkMode(item->hDC,TRANSPARENT);SelectObject(item->hDC,g_MessageTextFont);
            RECT label=item->rcItem;label.left+=ScaleByDPI(12);label.right-=ScaleByDPI(8);
            DrawText(item->hDC,text,-1,&label,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_HIDEPREFIX);
            if(selected){RECT marker=item->rcItem;marker.right=marker.left+ScaleByDPI(3);FillRect(item->hDC,&marker,ThemeBrush(item->itemState&ODS_FOCUS?COLOR_HOTLIGHT:COLOR_GRAYTEXT));}
            RestoreDC(item->hDC,saved);return TRUE;
        }

		case WM_INITDIALOG:
		{
			OnPreferenceDialogInit(hWnd);
			break;
		}
		case WM_DESTROY:
		{
			DialogHeader* pData = (DialogHeader*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
            SetWindowLongPtr(hWnd,GWLP_USERDATA,0);
			if (pData)
				delete pData;

			// fallthrough
		}
		case WM_COMMAND:
		{
            if(LOWORD(wParam)==IDC_OPTIONS_NAV && HIWORD(wParam)==LBN_SELCHANGE){OnSelChanged(hWnd);return TRUE;}
            if(LOWORD(wParam)==IDC_PREFS_DONE || LOWORD(wParam)==IDCANCEL){SetFocus(hWnd);EndDialog(hWnd,OPTIONS_RESULT_OK);return TRUE;}
			return 0L;
		}

		case WM_CLOSE:
		{
            SetFocus(hWnd); // Flush edit focus-loss handlers before destroying a page.
			EndDialog(hWnd, OPTIONS_RESULT_OK);
			break;
		}
	}

	return 0L;
}

int ShowOptionsDialog()
{
	return (int) DialogBox(g_hInstance, MAKEINTRESOURCE(DMDI(IDD_DIALOG_OPTIONS)), g_Hwnd, DialogProc);
}
