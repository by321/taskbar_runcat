#include "framework.h"
#include "resource.h"
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comdlg32.lib")

const TCHAR ICONS_DIR[] = L"icons"; // base dir for icon sub-directories
const float FPS_VALUES[] = { 0.1f, 0.3f, 1, 3, 10, 30 };

void PopulateFrequencyListbox(HWND hwndDlg, int listboxId) {
	HWND hListbox = GetDlgItem(hwndDlg, listboxId);
	SendMessage(hListbox, LB_RESETCONTENT, 0, 0);
	TCHAR buf[16];
	for (float h : FPS_VALUES) {
		swprintf(buf, 16, L"%.1f hz", h);
		SendMessage(hListbox, LB_ADDSTRING, 0, (LPARAM)buf);
	}
	SendMessage(hListbox, LB_SETCURSEL, 2, 0);
}

void CenterWindowOnScreen(HWND h) {
	RECT rect;
	GetWindowRect(h, &rect);
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	int dialogWidth = rect.right - rect.left;
	int dialogHeight = rect.bottom - rect.top;
	int x = (screenWidth - dialogWidth) / 2;
	int y = (screenHeight - dialogHeight) / 2;
	SetWindowPos(h, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

bool PopulateSubdirListbox(HWND hwndDlg, int listboxId) {
	HWND hListbox = GetDlgItem(hwndDlg, listboxId);
	SendMessageW(hListbox, LB_RESETCONTENT, 0, 0);

	try { //enum subdirectories
		for (const auto& entry : std::filesystem::directory_iterator(ICONS_DIR)) {
			if (entry.is_directory()) {
				SendMessage(hListbox, LB_ADDSTRING, 0, (LPARAM)entry.path().filename().c_str());
			}
		}
		if (SendMessage(hListbox, LB_GETCOUNT, 0, 0) > 0) {
			SendMessage(hListbox, LB_SETCURSEL, 0, 0);
			return true;
		}
		MessageBox(hwndDlg, L"No icons subdirectories found", L"Error", MB_OK | MB_ICONERROR);
	} catch (const std::exception& e) {
		MessageBoxA(hwndDlg, e.what(), "Error", MB_OK | MB_ICONERROR);
	}
	return false;
}

// load all ICO files from a directory
std::vector<HICON> LoadICOFrames(const TCHAR*pSubDir) {
	std::vector<HICON> icons;
	TCHAR buf[_MAX_PATH];
	wsprintf(buf, L"%s\\%s", ICONS_DIR, pSubDir);
	std::vector<std::filesystem::directory_entry> files;
	for (const auto& entry : std::filesystem::directory_iterator(buf)) {
		if (entry.path().extension() == L".ico") {
			files.push_back(entry);
		}
	}
	std::sort(files.begin(), files.end());

	for (const auto& filePath : files) {
		HANDLE h = LoadImage(NULL, filePath.path().c_str(), IMAGE_ICON,
							 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
		if (h) icons.push_back((HICON)h);
	}
	return icons;
}

// Enable or disable FPS and animation selection controls
void EnableControls(HWND hwndDlg, BOOL enable) {
	EnableWindow(GetDlgItem(hwndDlg, LIST_ICONDIRS), enable);
	EnableWindow(GetDlgItem(hwndDlg, LIST_FREQUENCIES), enable);
}

void UpdateIcon(HWND hwndDlg, HICON hIcon) {
	SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
	SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
}


INT_PTR CALLBACK MainDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	static std::vector<HICON> icons;
	static UINT frameIndex = 0, timerId = 1;
	static BOOL isAnimating = FALSE;
	static HICON systemAppIcon; //we will let OS free this

	switch (msg) {
		case WM_INITDIALOG:
			systemAppIcon = LoadIcon(NULL, IDI_APPLICATION);
			SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)systemAppIcon);
			SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)systemAppIcon);
			CenterWindowOnScreen(hwndDlg);
			PopulateFrequencyListbox(hwndDlg, LIST_FREQUENCIES);
			if (!PopulateSubdirListbox(hwndDlg, LIST_ICONDIRS)) {
				EndDialog(hwndDlg, -1);
				return FALSE;
			}
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDC_START: {
					if (isAnimating) return TRUE;

					LRESULT si = SendDlgItemMessage(hwndDlg, LIST_FREQUENCIES, LB_GETCURSEL, 0, 0);
					if (LB_ERR == si) {
						MessageBox(hwndDlg, L"No FPS value selected.", L"Error", MB_OK | MB_ICONERROR);
						return TRUE;
					}
					UINT timerInterval = (UINT)(1000 / FPS_VALUES[si] + 0.5f);

					si = SendDlgItemMessage(hwndDlg, LIST_ICONDIRS, LB_GETCURSEL, 0, 0);
					if (LB_ERR == si) {
						MessageBox(hwndDlg, L"No icons folder selected.", L"Error", MB_OK | MB_ICONERROR);
						return TRUE;
					}

					TCHAR subDirPath[_MAX_PATH];
					SendDlgItemMessage(hwndDlg, LIST_ICONDIRS, LB_GETTEXT, si, (LPARAM)subDirPath);
					SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)systemAppIcon);
					SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)systemAppIcon);
					for (HICON hIcon : icons) DestroyIcon(hIcon); //delete old icons
					//icons.clear();
					icons = LoadICOFrames(subDirPath);
					if (icons.size() == 0) {
						MessageBox(hwndDlg, L"No valid ICO files loaded from selected directory.", L"Error", MB_OK | MB_ICONERROR);
						return TRUE;
					}

					isAnimating = TRUE;
					EnableControls(hwndDlg, FALSE); // Disable radio buttons
					SetTimer(hwndDlg, timerId, timerInterval, nullptr);
					UpdateIcon(hwndDlg, icons[0]); //set icon now so we won't see a long delay when FPS is low
					frameIndex = (icons.size() == 1) ? 0 : 1;
					break;
				}
				case IDC_STOP:
					if (isAnimating) {
						KillTimer(hwndDlg, timerId);
						isAnimating = FALSE;
						EnableControls(hwndDlg, TRUE); // Enable radio buttons
					}
					break;
				case IDOK:
					EndDialog(hwndDlg, IDOK);
					break;
				default: return FALSE;
			} //switch (LOWORD(wParam)) {
			break;
		case WM_TIMER:
			if (wParam == timerId && !icons.empty()) {
				UpdateIcon(hwndDlg, icons[frameIndex]);
				frameIndex = (frameIndex + 1) % icons.size();
			}
			break;
		case WM_CLOSE:
			for (HICON hIcon : icons) DestroyIcon(hIcon);
			KillTimer(hwndDlg, timerId);
			EndDialog(hwndDlg, 0);
			break;
		default: return FALSE;
	} //switch (msg) {
	return TRUE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
	INT_PTR ret = DialogBox(hInstance, MAKEINTRESOURCE(IDD_MAIN_DIALOG), nullptr, MainDialogProc);
	//if (ret == -1) MessageBox(nullptr, L"Failed to create dialog.", L"Error", MB_OK | MB_ICONERROR);
	return (ret == -1) ? 1 : 0;
}
