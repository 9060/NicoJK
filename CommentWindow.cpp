
#include "stdafx.h"
#include "CommentWindow.h"
#pragma comment(lib, "winmm.lib")

#include <stdarg.h>
//#ifdef _DEBUG
#if 1
inline void dprintf_real( const _TCHAR * fmt, ... )
{
  _TCHAR buf[1024];
  va_list ap;
  va_start(ap, fmt);
  _vsntprintf_s(buf, 1024, fmt, ap);
  va_end(ap);
  OutputDebugString(buf);
}
#  define dprintf dprintf_real
#else
#  define dprintf __noop
#endif

#include <string>
#include <vector>
#include <list>
#include <cstdio>
#include <algorithm>
#include <functional>

HFONT CreateFont(int h) {
	return ::CreateFont(h,    //�t�H���g����
        0,                    //������
        0,                    //�e�L�X�g�̊p�x
        0,                    //�x�[�X���C���Ƃ����Ƃ̊p�x
        FW_SEMIBOLD,            //�t�H���g�̏d���i�����j
        FALSE,                //�C�^���b�N��
        FALSE,                //�A���_�[���C��
        FALSE,                //�ł�������
        SHIFTJIS_CHARSET,    //�����Z�b�g
        OUT_DEFAULT_PRECIS,    //�o�͐��x
        CLIP_DEFAULT_PRECIS,//�N���b�s���O���x
        NONANTIALIASED_QUALITY,        //�o�͕i��
        FIXED_PITCH | FF_MODERN,//�s�b�`�ƃt�@�~���[
        TEXT("MS UI Gothic"));    //���̖�
}

class Chat {
public:
	int vpos; // �\���J�nvpos
	std::wstring second;
	float line; // �ォ��0, 1... �����Ǐ����_�������
	int width; // �\�����i�s�N�Z���j
	Chat(int f, std::wstring s)
		: vpos(f),
		  second(s),
		  line(0),
		  width(200)
	{ }

	Chat(const Chat &c) :
		vpos(c.vpos),
		second(c.second),
		line(c.line),
		width(c.width)
	{ }

	bool operator<(const Chat& b) {
		return vpos < b.vpos;
	}
};

int linecount_ = 0;
std::vector<Chat> chat;
const int VPOS_LEN = 400;

class ChatManager {

	struct filter {
		int minvpos_;
		filter(int minvpos) : minvpos_(minvpos) {}
		bool operator()(const Chat& x) const { return x.vpos < minvpos_; }
	}; 

	int linecount_;
	int window_width_;
	int *lines_;
	int *counts_;

	// GDI
	HWND hWnd_;
	HDC hDC_;
	HFONT hFont_;
	HGDIOBJ hOld_;

	void ClearObjects() {
		if (hDC_) {
			SelectObject(hDC_, hOld_);
			DeleteObject(hFont_);
			ReleaseDC(hWnd_, hDC_);
		}
		hDC_ = NULL;
		hWnd_ = NULL;
	}

public:
	std::list<Chat> chat_;

	ChatManager() :
		linecount_(0),
		lines_(NULL),
		counts_(NULL),
		hDC_(NULL),
		hWnd_(NULL)
	{ }

	bool insert(std::wstring str, int vpos) {
		if (hWnd_) {
			Chat c(vpos, str);
			UpdateChat(c);
			chat_.push_back(Chat(c));
			return true;
		}
		return false;
	}

	bool trim(int minvpos) {
		chat_.remove_if(filter(minvpos));
		return true;
	}

	void UpdateChat(Chat &c) {
		// �����Ă���T���ij ��j
		int j;
		for (j=0; j<linecount_; j++) {
			if (lines_[j] < c.vpos) {
				break;
			}
		}
		c.line = (float)j;

		// �󂫂��Ȃ������̂œK����vpos�̏������Ƃ��ɂ��炵�ĕ\��
		if (j == linecount_) {
			int vpos_min = 0x7fffffff;
			int line_index = -1;
			for (j=0; j<linecount_; j++) {
				if (lines_[j] < vpos_min) {
					vpos_min = lines_[j];
					line_index = j;
				}
			}
			j = line_index;
			c.line = (float)0.5 + j;
		}

		SIZE size;
		GetTextExtentPoint32W(hDC_, c.second.c_str(), c.second.length(), &size);
		counts_[j]++;
		c.width = size.cx;
		//dprintf(L"vpod: %d, line: %d, width: %d", c.vpos, c.line, c.width);
		float ratio = (float)size.cx / window_width_;
		if (ratio < 0.1) {
			lines_[j] = c.vpos + (float)VPOS_LEN * (ratio + 0.1);
		} else if (ratio < 0.2) {
			lines_[j] = c.vpos + (float)VPOS_LEN * (ratio + 0.1);
		} else if (ratio < 0.4) {
			lines_[j] = c.vpos + (float)VPOS_LEN * 0.5;
		} else if (ratio < 0.6) {
			lines_[j] = c.vpos + (float)VPOS_LEN * 0.7;
		} else {
			lines_[j] = c.vpos + (float)VPOS_LEN * 0.9;
		}
	}

	void Clear() {
		chat_.clear();
		linecount_ = -1;
		delete [] lines_;
		delete [] counts_;
		lines_ = NULL;
		counts_ = NULL;
		ClearObjects();
	}

	void PrepareChats(HWND hWnd) {
		ClearObjects();
		hWnd_ = hWnd;
		hDC_ = GetDC(hWnd);
		hFont_ = CreateFont(28);
		HGDIOBJ hOld = SelectObject(hDC_, hFont_);

		RECT rc;
		GetWindowRect(hWnd, &rc);
		window_width_ = rc.right - rc.left;
		int newLineCount = (rc.bottom - rc.top) / 30 - 1;
		if (linecount_ == newLineCount) {
			return;
		}

		delete [] lines_;
		delete [] counts_;

		linecount_ = newLineCount;
		lines_ = new int[newLineCount];
		counts_ = new int[newLineCount];
		memset(lines_, 0, sizeof(int) * newLineCount);
		memset(counts_, 0, sizeof(int) * newLineCount);

		std::list<Chat>::iterator i;
		for (i=chat_.begin(); i!=chat_.end(); ++i) {
			UpdateChat(*i);
		}
		for (int i=0; i<newLineCount; i++) {
			dprintf(L"line[%d] = %d", i, counts_[i]);
		}
	}
};
ChatManager manager;

void PrepareChats(HWND hWnd) {
	HDC dc = GetDC(hWnd);
	HFONT hFont = CreateFont(28);
	HGDIOBJ hOld = SelectObject(dc, hFont);

	RECT rc;
	GetWindowRect(hWnd, &rc);
	int window_width = rc.right - rc.left;
	int newLineCount = (rc.bottom - rc.top) / 30 - 1;
	if (linecount_ == newLineCount) {
		//return;
	}
	linecount_ = newLineCount;
	int *lines = new int[newLineCount];
	int counts[50] = {0};
	for (int i=0; i<newLineCount; i++) {
		lines[i] = 0;
	}

	SIZE size;
	std::vector<Chat>::iterator i = chat.begin();
	while(i != chat.end()) {
		Chat &c = *i;
		// �����Ă���T���ij ��j
		int j;
		for (j=0; j<newLineCount; j++) {
			if (lines[j] < c.vpos) {
				break;
			}
		}
		c.line = (float)j;

		// �󂫂��Ȃ������̂œK����vpos�̏������Ƃ��ɂ��炵�ĕ\��
		if (j == newLineCount) {
			int vpos_min = 0x7fffffff;
			int line_index = -1;
			for (j=0; j<newLineCount; j++) {
				if (lines[j] < vpos_min) {
					vpos_min = lines[j];
					line_index = j;
				}
			}
			j = line_index;
			c.line = (float)0.5 + j;
		}
		
		GetTextExtentPoint32W(dc, c.second.c_str(), c.second.length(), &size);
		counts[j]++;
		c.width = size.cx;
		//dprintf(L"vpod: %d, line: %d, width: %d", c.vpos, c.line, c.width);
		float ratio = (float)size.cx / window_width;
		if (ratio < 0.1) {
			lines[j] = c.vpos + (float)VPOS_LEN * (ratio + 0.1);
		} else if (ratio < 0.2) {
			lines[j] = c.vpos + (float)VPOS_LEN * (ratio + 0.1);
		} else if (ratio < 0.4) {
			lines[j] = c.vpos + (float)VPOS_LEN * 0.5;
		} else if (ratio < 0.6) {
			lines[j] = c.vpos + (float)VPOS_LEN * 0.7;
		} else {
			lines[j] = c.vpos + (float)VPOS_LEN * 0.9;
		}
		++i;
	}
	for (int i=0; i<newLineCount; i++) {
		dprintf(L"line[%d] = %d", i, counts[i]);
	}

	delete [] lines;
	SelectObject(dc, hOld);
	DeleteObject(hFont);
	ReleaseDC(hWnd, dc);
}

Cjk::Cjk(TVTest::CTVTestApp *pApp) :
	m_pApp(pApp),
	hWnd_(NULL),
	memDC_(NULL),
	msPosition_(0)
{ }

void Cjk::Create(HWND hParent) {
	WNDCLASSEX wc;
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;    //�v���V�[�W����
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = NULL;//�C���X�^���X
	wc.hIcon = NULL;
	wc.hCursor = (HCURSOR)LoadImage(NULL,
		MAKEINTRESOURCE(IDC_ARROW),
		IMAGE_CURSOR,
		0,
		0,
		LR_DEFAULTSIZE | LR_SHARED);
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName = NULL;    //���j���[��
	wc.lpszClassName = TEXT("ru.jk");
	wc.hIconSm = (HICON)LoadImage(NULL,
		MAKEINTRESOURCE(IDI_APPLICATION),
		IMAGE_ICON,
		0,
		0,
		LR_DEFAULTSIZE | LR_SHARED);

	RegisterClassEx(&wc);

	hWnd_ = CreateWindowEx(WS_EX_LAYERED,
			(LPCTSTR)_T("ru.jk"),
			TEXT("TvtPlayJK"), //�^�C�g���o�[�ɂ��̖��O���\������܂�
			WS_POPUP, //�E�B���h�E�̎��
			100,    //�w���W
			100,    //�x���W
			200,    //��
			200,    //����
			hParent, //�e�E�B���h�E�̃n���h���A�e�����Ƃ���NULL
			NULL, //���j���[�n���h���A�N���X���j���[���g���Ƃ���NULL
			NULL, //�C���X�^���X�n���h��
			(LPVOID)this);

	HWND hButton = CreateWindow(
		_T("BUTTON"),                            // �E�B���h�E�N���X��
		_T("�R�����g�Đ��J�n"),                                 // �L���v�V����
		WS_CHILD | BS_PUSHBUTTON,   // �X�^�C���w��
		10, 10,                                    // ���W
		200, 30,                                   // �T�C�Y
		hWnd_,                                    // �e�E�B���h�E�̃n���h��
		(HMENU)(INT_PTR)IDB_START,                      // ���j���[�n���h���i������ID�w��ɗ��p�ł���j
		NULL,                                 // �C���X�^���X�n���h��
		NULL                                     // ���̑��̍쐬�f�[�^
	);
	//SetWindowPos(hButton, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
	// ShowWindow��xml�ǂݍ��݌�

	ShowWindow(hWnd_, SW_SHOW);
	UpdateWindow(hWnd_);

	RECT rcParent;
	GetWindowRect(hParent, &rcParent);
	Resize(rcParent.left, rcParent.top, rcParent.right - rcParent.left, rcParent.bottom - rcParent.top);
}

void Cjk::Destroy() {
	if (socGetflv_ != INVALID_SOCKET) {
		closesocket(socGetflv_);
		socGetflv_ = INVALID_SOCKET;
	}
	if (socComment_ != INVALID_SOCKET) {
		closesocket(socComment_);
		socComment_ = INVALID_SOCKET;
	}
	ClearObjects();
	DestroyWindow(hWnd_);
}

HWND Cjk::GetFullscreenWindow()
{
	TVTest::HostInfo hostInfo;
	if (m_pApp->GetFullscreen() && m_pApp->GetHostInfo(&hostInfo)) {
		wchar_t className[64];
		::lstrcpynW(className, hostInfo.pszAppName, 48);
		::lstrcatW(className, L" Fullscreen");
		
		HWND hwnd = NULL;
		while ((hwnd = ::FindWindowEx(NULL, hwnd, className, NULL)) != NULL) {
			DWORD pid;
			::GetWindowThreadProcessId(hwnd, &pid);
			if (pid == ::GetCurrentProcessId()) return hwnd;
		}
	}
	return NULL;
}

void Cjk::Resize(HWND hApp) {
	RECT rc;
	GetWindowRect(hApp, &rc);
	Resize(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
}

void Cjk::Resize(int left, int top, int width, int height) {
	bool fullScreen = m_pApp->GetFullscreen();
	dprintf(L"Resize %x", hWnd_);
	if (fullScreen) {
		HWND hFull = GetFullscreenWindow();
		RECT rc;
		GetWindowRect(hFull, &rc);
		left = rc.left;
		top = rc.top;
		width = rc.right - rc.left;
		height = rc.bottom - rc.top;
		SetWindowPos(hWnd_, HWND_TOPMOST, left + 10, top + 30, width - 20, height - 60, SWP_NOACTIVATE);
	} else {
		if (width && height) {
			SetWindowPos(hWnd_, HWND_NOTOPMOST, left + 10, top + 30, width - 20, height - 60, SWP_NOACTIVATE);
		} else {
			SetWindowPos(hWnd_, HWND_NOTOPMOST, left + 10, top + 30, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE);
		}
	}
	SetupObjects();
	//PrepareChats(hWnd_);
}

void Cjk::SetupObjects() {
	if (memDC_) {
		ClearObjects();
	}

	HDC wndDC = GetDC(hWnd_);
	memDC_ = CreateCompatibleDC(wndDC);

	RECT rc;
	GetWindowRect(hWnd_, &rc);
	hBitmap_ = CreateCompatibleBitmap(wndDC, 1920, 1080);
	if (!hBitmap_) {
		dprintf(L"CreateCompatibleBitmap failed");
	}
	hPrevBitmap_ = (HBITMAP)SelectObject(memDC_, hBitmap_);
	
	dprintf(L"Setup %x, %x, %x", hWnd_, memDC_, hBitmap_);
	//ReleaseDC(hWnd_, wndDC);
}

void Cjk::ClearObjects() {
	if (memDC_ == NULL) {
		return ;
	}
	dprintf(L"Clear");
	SelectObject(memDC_, hPrevBitmap_);
	DeleteObject(hBitmap_);
	DeleteDC(memDC_);
	memDC_ = NULL;
}


#pragma comment(lib, "ws2_32.lib")
#define WMS_JKHOST_EVENT (WM_USER + 1)
#define WMS_GETFLV_EVENT (WM_USER + 2)
#define WMS_COMMENT_EVENT (WM_USER + 3)

// token�� ms= �̂悤�� = ������B
bool GetToken(const char *in, const char *token, char *out, int len) {
	const char *start = strstr(in, token);
	if (start) {
		start += strlen(token);
		const char *end = strchr(start, '&');
		if (end) {
			strncpy(out, start, min(end - start, len - 1));
			out[min(end - start, len - 1)] = '\0';
			return true;
		}
	}
	return false;
}

void Cjk::Open(int jkCh) {
	// �E�B���h�E����U��
	HWND hParent = GetParent(hWnd_);
	manager.Clear();
	Destroy();
	Create(m_pApp->GetAppWindow());

	linecount_ = 0;
	msPositionBase_ = 0;
	jkCh_ = jkCh;

	WSAAsyncGetHostByName(hWnd_, WMS_JKHOST_EVENT, "jk.nicovideo.jp", szHostbuf_, MAXGETHOSTSTRUCT);

	this->SetPosition(timeGetTime());
	this->Start();
}

void Cjk::Open(LPCTSTR tsName) {
	// �E�B���h�E����U��
	HWND hParent = GetParent(hWnd_);
	Destroy();
	Create(hParent);
	ShowWindow(GetDlgItem(hWnd_, IDB_START), SW_HIDE);

	chat.clear();
	linecount_ = 0;
	msPositionBase_ = 0;

	TCHAR filename[2048];
	_tcscpy_s(filename, tsName);
	TCHAR *pdot = _tcsrchr(filename, TEXT('.'));
	if (!pdot) {	
		return;
	}
	_tcscpy(pdot, TEXT(".xml"));
	FILE * fp = _tfopen(filename, TEXT("r"));
	if (!fp) {
		return ;
	}
	char sz[2048];
	wchar_t unicode[1024];
	wchar_t comment[1024];
	while(fgets(sz, 2048, fp)) {
		MultiByteToWideChar(CP_UTF8, 0, sz, strlen(sz)+1, unicode, 1024);

		wchar_t *p = wcsstr(unicode, L" vpos=\"");
		if (p) {
			long vpos = _wtol(p + 7);
			wchar_t *ps = wcsstr(unicode, L">");
			wchar_t *pe = wcsstr(ps, L"<");
			
			if (ps && pe) {
				wcsncpy(comment, ps + 1, pe - ps - 1);
				comment[pe - ps - 1] = L'\0';
				chat.push_back(Chat(vpos, std::wstring(comment)));
				//dprintf(L"%d: %s", vpos, comment);
			}
		}
	}
	fclose(fp);

	if (chat.size()) {
		std::sort(chat.begin(), chat.end());
		PrepareChats(hWnd_);
		ShowWindow(GetDlgItem(hWnd_, IDB_START), SW_SHOW);
		dprintf(L"Open: %d", chat.size());
	}
}

void Cjk::Start() {
	//if (chat.size()) {
		msPositionBase_ = msPosition_;
		SetLayeredWindowAttributes(hWnd_, RGB(254, 254, 254), 0, LWA_COLORKEY);
	//}
	//ShowWindow(GetDlgItem(hWnd_, IDB_START), SW_HIDE);
}

void Cjk::SetPosition(int posms) {
	msSystime_ = 0;//timeGetTime();
	msPosition_ = timeGetTime();
}

void Cjk::DrawComments(HWND hWnd, HDC hDC) {
	if (!msPositionBase_) {
		return;
	}
	if (memDC_) {
		SetBkMode(memDC_, TRANSPARENT);
		RECT rc = {0, 0, 1920, 1080};
		HBRUSH transBrush = CreateSolidBrush(RGB(254, 254, 254));
		FillRect(memDC_, &rc, transBrush);
		DeleteObject(transBrush);

		RECT rcWnd;
		GetWindowRect(hWnd, &rcWnd);
		int width = rcWnd.right - rcWnd.left;

		int basevpos = timeGetTime() / 10; //(msPosition_ - msPositionBase_ + (timeGetTime() - msSystime_)) / 10;
#if 0
		int n = chat.size();
		for (int i=0; i<n; i++) {
			Chat &c = chat.at(i);
			if (c.vpos > basevpos) {
				n = i - 1;
				break;
			}
		}

		if (n == chat.size()) {
			--n;
		}
#endif	
		SetBkMode(memDC_, TRANSPARENT);
		HGDIOBJ hPrevFont = SelectObject(memDC_, CreateFont(28));
		
		std::list<Chat>::iterator i = manager.chat_.begin();
		for (i=manager.chat_.begin(); i!=manager.chat_.end(); ++i) {
			// skip
			if (i->vpos < basevpos - VPOS_LEN) {
				continue;
			}
			if (i->vpos > basevpos) {
				break;
			}
			int x = static_cast<int>(width - (float)(i->width + width) * (basevpos - i->vpos) / VPOS_LEN);
			std::wstring &str = i->second;
			int y = static_cast<int>(10 + 30 * i->line);
			TextOutW(memDC_, x+2, y+2, str.c_str(), str.length());
			SetTextColor(memDC_, RGB(255, 255, 255));
			TextOutW(memDC_, x, y, str.c_str(), str.length());
			SetTextColor(memDC_, RGB(0, 0, 0));
		}
		DeleteObject(SelectObject(memDC_, hPrevFont));

		BitBlt(hDC, 0, 0, rcWnd.right - rcWnd.left, rc.bottom - rc.top, memDC_, 0, 0, SRCCOPY);
	}
}

LRESULT CALLBACK Cjk::WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
	HDC hdc;
	PAINTSTRUCT ps;

	static Cjk *pThis = NULL;

	switch (msg) {
		case WM_CREATE:
			SetLayeredWindowAttributes(hWnd, RGB(255, 255, 255), 0, LWA_COLORKEY);
			pThis = (Cjk*)((CREATESTRUCT*)lp)->lpCreateParams;
			SetTimer(hWnd, jkTimerID, 50, NULL);
			pThis->socGetflv_ = INVALID_SOCKET;
			pThis->socComment_ = INVALID_SOCKET;
			manager.PrepareChats(hWnd);
			break;
		case WM_COMMAND:
			switch(LOWORD(wp)) {
			case IDB_START:
				pThis->Start();
				break;
			}
			break;
		case WM_PAINT:
			hdc = BeginPaint(hWnd, &ps);
			pThis->DrawComments(hWnd, hdc);
			EndPaint(hWnd, &ps);
			break;
		case WM_TIMER:
			hdc = GetDC(hWnd);
			pThis->DrawComments(hWnd, hdc);
			ReleaseDC(hWnd, hdc);
			break;
		case WM_CLOSE:
			PostMessage(GetParent(hWnd), WM_CLOSE, 0, 0);
			DestroyWindow(hWnd);
			break;
		case WMS_JKHOST_EVENT:
		if (WSAGETASYNCERROR(lp) == 0) {
			HOSTENT *hostent = (HOSTENT*)pThis->szHostbuf_;
			OutputDebugString(_T("JKHOST_EVENT"));
			pThis->socGetflv_ = socket(PF_INET, SOCK_STREAM, 0);
			if (pThis->socGetflv_ == INVALID_SOCKET) {
				//error
			}
			WSAAsyncSelect(pThis->socGetflv_, hWnd, WMS_GETFLV_EVENT, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE);
			
			struct  sockaddr_in	 serversockaddr;		/* �T�[�o�̃A�h���X */
			/* �T�[�o�̃A�h���X�̍\���̂ɃT�[�o��IP�A�h���X�ƃ|�[�g�ԍ���ݒ肵�܂� */
			serversockaddr.sin_family = AF_INET;				/* �C���^�[�l�b�g�̏ꍇ */
			serversockaddr.sin_addr.s_addr = *(UINT*)(hostent->h_addr);				/* �T�[�o��IP�A�h���X */
			serversockaddr.sin_port = htons((unsigned short)80);		/* �|�[�g�ԍ� */
			memset(serversockaddr.sin_zero,(int)0,sizeof(serversockaddr.sin_zero));
			if(connect(pThis->socGetflv_, (struct sockaddr *)&serversockaddr,sizeof(serversockaddr)) == SOCKET_ERROR){
				if(WSAGetLastError() != WSAEWOULDBLOCK){
					MessageBox(hWnd, L"jk�̐ڑ��Ɏ��s���܂����B", L"Error", MB_OK | MB_ICONERROR);
				}
			}
		}
		return TRUE;
	case WMS_GETFLV_EVENT:
		{
			SOCKET soc = (SOCKET)wp;
			char *szGetTemplate = "GET /api/getflv?v=jk%d HTTP/1.0\r\nHost: jk.nicovideo.jp\r\n\r\n";
			char szGet[1024];
			wsprintfA(szGet, szGetTemplate, pThis->jkCh_);
			static bool sent = false;
			switch(WSAGETSELECTEVENT(lp))
			{
			case FD_CONNECT:		/* �T�[�o�ڑ������C�x���g�̏ꍇ */
				OutputDebugString(_T("getflv Connected"));
				break;
			case FD_WRITE:
				if (sent == false) {
					OutputDebugString(_T("getflv Write"));
					send(soc, szGet, strlen(szGet)+1, 0);
					sent = false;
				}
				break;
			case FD_READ:
				char buf[10240];
				memset(buf, '\0', 10240);
				int read;
				read = recv(soc, buf, sizeof(buf)-1, 0);
				buf[read] = '\0';
				if (strstr(buf, "done=true")) {
					char szMs[1024], szMsPort[1024];
					if (GetToken(buf, "ms=", szMs, 1024)
							&& GetToken(buf, "ms_port=", szMsPort, 1024)
							&& GetToken(buf, "thread_id=", pThis->szThread_, 1024)) {
						pThis->socComment_ = socket(PF_INET, SOCK_STREAM, 0);
						if (pThis->socComment_ == INVALID_SOCKET) {
							//error
						}
						WSAAsyncSelect(pThis->socComment_, hWnd, WMS_COMMENT_EVENT, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE);

						struct sockaddr_in serversockaddr;
						serversockaddr.sin_family = AF_INET;
						serversockaddr.sin_addr.s_addr = inet_addr(szMs);
						serversockaddr.sin_port = htons((unsigned short)atoi(szMsPort));
						memset(serversockaddr.sin_zero,(int)0,sizeof(serversockaddr.sin_zero));

						// �R�����g�T�[�o�ɐڑ�
						if(connect(pThis->socComment_,(struct sockaddr *)&serversockaddr,sizeof(serversockaddr)) == SOCKET_ERROR){
							if(WSAGetLastError() != WSAEWOULDBLOCK){
								MessageBox(hWnd,L"�T�[�o�ւ̐ڑ��Ɏ��s���܂����B",L"Error",MB_OK | MB_ICONERROR);
							}
						}
					}
				}
				OutputDebugStringA(buf);
				closesocket(soc);
				break;
			case FD_CLOSE:
				OutputDebugString(_T("getflv Closed"));
				pThis->socGetflv_ = INVALID_SOCKET;
				break;
			}
		}
		return TRUE;
	case WMS_COMMENT_EVENT:
		{
			SOCKET soc = (SOCKET)wp;
			char szRequest[1024];
			wsprintfA(szRequest, "<thread res_from=\"-10\" version=\"20061206\" thread=\"%s\" />", pThis->szThread_);
			static bool sent = false;
			switch(WSAGETSELECTEVENT(lp))
			{
			case FD_CONNECT:		/* �T�[�o�ڑ������C�x���g�̏ꍇ */
				OutputDebugString(_T("Connected"));
				manager.PrepareChats(hWnd);
				break;
			case FD_WRITE:
				if (sent == false) {
					OutputDebugString(_T("Write"));
					send(soc, szRequest, strlen(szRequest)+1, 0);
					sent = false;
				}
				break;
			case FD_READ:
				char buf[10240];
				memset(buf, '\0', 10240);
				int read;
				read = recv(soc, buf, sizeof(buf)-1, 0);
				wchar_t wcsBuf[10240];
				int wcsLen;
				wcsLen = MultiByteToWideChar(CP_UTF8, 0, buf, read, wcsBuf, 10240);
				int pos;
				pos = 0;
				do {
					wchar_t *line = &wcsBuf[pos];
					int len = wcslen(line);
					if (wcsncmp(line, L"<chat ", 6) == 0) {
						wchar_t *es = wcschr(line, L'>');
						wchar_t *ee = wcschr(line + 1, L'<');
						if (es && ee) {
							++es;
							*ee = L'\0';
							OutputDebugString(es);
							manager.insert(es, timeGetTime() / 10);
							chat.push_back(Chat(timeGetTime()/10, es));
						}
					}
					pos += len + 1;
				} while(pos < wcsLen);
				manager.trim(timeGetTime() / 10 - VPOS_LEN * 3);
				manager.PrepareChats(hWnd);
				break;
			case FD_CLOSE:
				pThis->socComment_ = INVALID_SOCKET;
				OutputDebugString(_T("Closed"));
				break;
			}
		}
		return TRUE;
		default:
			return (DefWindowProc(hWnd, msg, wp, lp));
	}
	return 0;
}

BOOL Cjk::WindowMsgCallback(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam,LRESULT *pResult) {
    switch (uMsg) {
	case WM_ACTIVATE:
		if (WA_INACTIVE == LOWORD(wParam)) {
			return FALSE;
		}
	case WM_MOVE:
    case WM_SIZE:
		Resize(m_pApp->GetAppWindow());
		break;
	}
	return FALSE;
}

LRESULT Cjk::EventCallback(UINT Event, LPARAM lParam1, LPARAM lParam2) {
	switch (Event) {
    case TVTest::EVENT_PLUGINENABLE:
        // �v���O�C���̗L����Ԃ��ω�����
        //return pThis->EnablePlugin(lParam1 != 0);
		break;
    case TVTest::EVENT_FULLSCREENCHANGE:
        // �S��ʕ\����Ԃ��ω�����
        if (m_pApp->IsPluginEnabled()) {
            Resize(m_pApp->GetAppWindow());
		}
        break;
	}
	return 0;
}
