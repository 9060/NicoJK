
#include "stdafx.h"
#include "CommentWindow.h"
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "ws2_32.lib")

// �R�����g�擾�p�C�x���g
#define WMS_JKHOST_EVENT (WM_USER + 1)
#define WMS_GETFLV_EVENT (WM_USER + 2)
#define WMS_COMMENT_EVENT (WM_USER + 3)

#ifdef _DEBUG
#include <stdarg.h>
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

enum CHAT_POSITION {
	CHAT_POS_DEFAULT = 0,
	CHAT_POS_SHITA = 1,
	CHAT_POS_UE = 2
};

class Chat {
public:
	int vpos; // �\���J�nvpos
	std::wstring text;
	float line; // �ォ��0, 1... �����Ǐ����_�������
	int width; // �\�����i�s�N�Z���j
	COLORREF color;
	CHAT_POSITION position;
	Chat(int vpos_in, std::wstring text_in)
		: vpos(vpos_in),
		  text(text_in),
		  line(0),
		  width(200),
		  color(RGB(255, 255, 255)),
		  position(CHAT_POS_DEFAULT)
	{ }

	bool operator<(const Chat& b) {
		return vpos < b.vpos;
	}
};

struct COMMAND2COLOR {
	COLORREF color;
	wchar_t *command;
};

struct COMMAND2COLOR command2color[] = {
	{RGB(0xFF, 0x00, 0x00), L"red"},
	{RGB(0xFF, 0x80, 0x80), L"pink"},
	{RGB(0xFF, 0xC0, 0x00), L"orange"},
	{RGB(0xFF, 0xFF, 0x00), L"yellow"},
	{RGB(0x00, 0xFF, 0x00), L"green"},
	{RGB(0x00, 0xFF, 0xFF), L"cyan"},
	{RGB(0x00, 0x00, 0xFF), L"blue"},
	{RGB(0xC0, 0x00, 0xFF), L"purple"},
	{RGB(0x00, 0x00, 0x00), L"black"},
	{RGB(0xCC, 0xCC, 0x99), L"white2"},
	{RGB(0xCC, 0xCC, 0x99), L"niconicowhite"},
	{RGB(0xCC, 0x00, 0x33), L"red2"},
	{RGB(0xCC, 0x00, 0x33), L"truered"},
	{RGB(0xFF, 0x33, 0xCC), L"pink2"},
	{RGB(0xFF, 0x66, 0x00), L"orange2"},
	{RGB(0xFF, 0x66, 0x00), L"passionorange"},
	{RGB(0x99, 0x99, 0x00), L"yellow2"},
	{RGB(0x99, 0x99, 0x00), L"madyellow"},
	{RGB(0x00, 0xCC, 0x66), L"green2"},
	{RGB(0x00, 0xCC, 0x66), L"elementalgreen"},
	{RGB(0x00, 0xCC, 0xCC), L"cyan2"},
	{RGB(0x33, 0x99, 0xFF), L"blue2"},
	{RGB(0x33, 0x99, 0xFF), L"marineblue"},
	{RGB(0x66, 0x33, 0xCC), L"purple2"},
	{RGB(0x66, 0x33, 0xCC), L"nobleviolet"},
	{RGB(0x66, 0x66, 0x66), L"black2"}
};

#define COMMAND2COLOR_SIZE (sizeof(command2color)/sizeof(COMMAND2COLOR))
#define NO_COLOR	0xFFFFFFFF
COLORREF GetColor(const wchar_t *command) {
	if (command[0] == L'#') {
		if (wcslen(command) != 7) {
			return NO_COLOR;
		}
		int color = 0;
		for (int i=1; i<7; i++) {
			color *= 16;
			wchar_t c = towupper(command[i]);
			if (L'0' <= c && c <= L'9') {
				color += c - L'0';
			} else if (L'A' <= c && c <= L'F') {
				color += c - L'A' + 10;
			} else {
				return NO_COLOR;
			}
		}
		return RGB((color & 0xFF0000) >> 16, (color & 0x00FF00) >> 8, color & 0xFF);
	}
	for (int i=0; i<COMMAND2COLOR_SIZE; ++i) {
		if (wcscmp(command2color[i].command, command) == 0) {
			return command2color[i].color;
		}
	}
	return NO_COLOR;
}

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
	int *linesShita_;
	int *linesUe_;
	int *counts_;
	int iYPitch_;

	// GDI
	RECT oldRect_;
	HWND hWnd_;
	HWND hNotify_;
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

	bool xmlGetText(const wchar_t *str, wchar_t *out, int len) {
		const wchar_t *es = wcschr(str, L'>');
		const wchar_t *ee = wcschr(str + 1, L'<');
		if (es && ee) {
			++es;
			wcsncpy_s(out, len, es, min(len - 1, ee - es));

			// entity reference
			wchar_t *p = wcschr(out, L'&');
			while(p) {
				if (wcsncmp(p, L"&lt;", 4) == 0) {
					*p = L'<';
					wcscpy_s(p+1, len - (p - out) - 1, p + 4);
				} else if (wcsncmp(p, L"&gt;", 4) == 0) {
					*p = L'>';
					wcscpy_s(p+1, len - (p - out) - 1, p + 4);
				} else if (wcsncmp(p, L"&amp;", 5) == 0) {
					*p = L'&';
					wcscpy_s(p+1, len - (p - out) - 1, p + 5);
				} else if (wcsncmp(p, L"&apos;", 6) == 0) {
					*p = L'"';
					wcscpy_s(p+1, len - (p - out) - 1, p + 6);
				}
				p = wcschr(p + 1, L'&');
			}
			return true;
		}
		return false;
	}

	// token�̓X�y�[�X�Ŏn�܂�=�܂ł��� ��F" mail=" or ATTR("mail")
#define ATTR(name) (L" " ##name L"=\"")
	bool xmlGetAttr(const wchar_t *in, const wchar_t *token, wchar_t *out, int len) {
		const wchar_t *start = wcsstr(in, token);
		if (start) {
			start += wcslen(token);
			const wchar_t *end = wcschr(start, L'"');
			if (end) {
				return wcsncpy_s(out, len, start, min(end - start, len - 1)) != EINVAL;
			}
		}
		return false;
	}
public:
	std::list<Chat> chat_;

	ChatManager() :
		linecount_(0),
		lines_(NULL),
		linesShita_(NULL),
		linesUe_(NULL),
		counts_(NULL),
		iYPitch_(0),
		hDC_(NULL),
		hWnd_(NULL),
		hNotify_(NULL)
	{
		SetRectEmpty(&oldRect_);
	}

	bool insert(wchar_t *str, int vpos) {
		wchar_t szText[1024];
		wchar_t szCommand[1024] = L"";
		if (wcsncmp(str, L"<chat ", 6) == 0) {
			if (!xmlGetText(str, szText, 1024)) {
				return false;
			}
			
			Chat c(vpos, szText);
			if (xmlGetAttr(str, ATTR(L"mail"), szCommand, 1024)) {
				wchar_t *context = NULL;
				wchar_t *pt = wcstok_s(szCommand, L" ", &context);
				while (pt != NULL) {
					if (wcscmp(pt, L"184") == 0) {
						; // skip
					} else if (wcscmp(pt, L"shita") == 0) {
						c.position = CHAT_POS_SHITA;
					} else if (wcscmp(pt, L"ue") == 0) {
						c.position = CHAT_POS_UE;
					} else {
						COLORREF color = GetColor(pt);
						if (color != NO_COLOR) {
							c.color = color;
						}
					}
					pt = wcstok_s(NULL, L" ", &context);
				}
			}

			if (hWnd_) {
				UpdateChat(c);
				// �R�����g�E�B���h�E�ɃR�����g��ʒm
			}
			if (hNotify_) {
				SendMessage(hNotify_, WM_NEWCOMMENT, 0L, (LPARAM)szText);
			}

			chat_.push_back(c);
		}
		return true;
	}

	bool trim(int minvpos) {
		chat_.remove_if(filter(minvpos));
		return true;
	}

	void UpdateChat(Chat &c) {
		// �����Ă���T���ij ��j
		// TODO: ����鑬�x�ɉ����đI��
		int *target = lines_;
		if (c.position == CHAT_POS_SHITA) {
			target = linesShita_;
		} else if (c.position == CHAT_POS_UE) {
			// TODO: ����Ή��E�E�E���Ȃ�����
		}

		int j;
		for (j=0; j<linecount_; j++) {
			if (target[j] < c.vpos) {
				break;
			}
		}
		c.line = (float)j;

		// �󂫂��Ȃ������̂œK����vpos�̏������Ƃ��ɂ��炵�ĕ\��
		if (j == linecount_) {
			int vpos_min = 0x7fffffff;
			int line_index = 0;
			for (j=0; j<linecount_; j++) {
				if (target[j] < vpos_min) {
					vpos_min = lines_[j];
					line_index = j;
				}
			}
			j = line_index;
			c.line = static_cast<float>(0.5 + j);
		}

		if (c.position == CHAT_POS_SHITA) {
			linesShita_[j] = c.vpos + VPOS_LEN;
			return;
		}

		SIZE size;
		GetTextExtentPoint32W(hDC_, c.text.c_str(), c.text.length(), &size);
		counts_[j]++;
		c.width = size.cx;
		//dprintf(L"vpod: %d, line: %d, width: %d", c.vpos, c.line, c.width);
		float ratio = (float)size.cx / window_width_;
		double newvpos;
		if (ratio < 0.1) {
			newvpos = c.vpos + static_cast<double>(VPOS_LEN) * (ratio + 0.1);
		} else if (ratio < 0.2) {
			newvpos = c.vpos + static_cast<double>(VPOS_LEN) * (ratio + 0.1);
		} else if (ratio < 0.4) {
			newvpos = c.vpos + static_cast<double>(VPOS_LEN) * 0.5;
		} else if (ratio < 0.6) {
			newvpos = c.vpos + static_cast<double>(VPOS_LEN) * 0.7;
		} else {
			newvpos = c.vpos + static_cast<double>(VPOS_LEN) * 0.9;
		}
		lines_[j] = static_cast<int>(newvpos);
	}

	void Clear() {
		chat_.clear();
		linecount_ = -1;
		delete [] lines_;
		delete [] counts_;
		delete [] linesShita_;
		delete [] linesUe_;
		lines_ = NULL;
		counts_ = NULL;
		linesShita_ = NULL;
		linesUe_ = NULL;
		ClearObjects();
	}

	void PrepareChats(HWND hWnd) {
		if (!hWnd) {
			return;
		}

		RECT rc;
		GetClientRect(hWnd, &rc);
		if (EqualRect(&rc, &oldRect_)) {
			return;
		}
		CopyRect(&oldRect_, &rc);
		
		window_width_ = rc.right;
		int yPitch = 30;
		int newLineCount = max(1, rc.bottom / yPitch - 1);
		// �K���ɒ���
		if (newLineCount > 15) {
			newLineCount = 15;
			yPitch = rc.bottom / newLineCount;
		} else if (newLineCount < 8) {
			newLineCount = 8;
			yPitch = rc.bottom / newLineCount;
		}

		if (linecount_ == newLineCount) {
			return;
		}

		ClearObjects();
		hWnd_ = hWnd;
		hDC_ = GetDC(hWnd);
		hFont_ = CreateFont(yPitch - 2);
		iYPitch_ = yPitch;
		HGDIOBJ hOld = SelectObject(hDC_, hFont_);

		delete [] lines_;
		delete [] counts_;
		delete [] linesShita_;
		delete [] linesUe_;

		linecount_ = newLineCount;
		lines_ = new int[newLineCount];
		counts_ = new int[newLineCount];
		linesShita_ = new int[newLineCount];
		linesUe_ = new int[newLineCount];
		memset(lines_, 0, sizeof(int) * newLineCount);
		memset(counts_, 0, sizeof(int) * newLineCount);
		memset(linesShita_, 0, sizeof(int) * newLineCount);
		memset(linesUe_, 0, sizeof(int) * newLineCount);

		std::list<Chat>::iterator i;
		for (i=chat_.begin(); i!=chat_.end(); ++i) {
			UpdateChat(*i);
		}
		for (int i=0; i<newLineCount; i++) {
			dprintf(L"line[%d] = %d", i, counts_[i]);
		}
	}

	HFONT GetFont() {
		return hFont_;
	}

	int GetYPitch() {
		return iYPitch_;
	}

	void SetNotifyWindow(HWND hWnd) {
		hNotify_ = hWnd;
	}
};
ChatManager manager;

Cjk::Cjk(TVTest::CTVTestApp *pApp, HWND hForce) :
	m_pApp(pApp),
	hForce_(hForce),
	hWnd_(NULL),
	memDC_(NULL),
	msPosition_(0)
{ }

void Cjk::Create(HWND hParent) {
	static bool bInitialized = false;
	if (!bInitialized) {
		WNDCLASSEX wc;
		wc.cbSize = sizeof(WNDCLASSEX);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = WndProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = NULL;
		wc.hIcon = NULL;
		wc.hCursor = NULL;
		wc.hbrBackground = CreateSolidBrush(COLOR_TRANSPARENT);
		wc.lpszMenuName = NULL;
		wc.lpszClassName = TEXT("ru.jk");
		wc.hIconSm = NULL;

		RegisterClassEx(&wc);

		wc.lpfnWndProc = SocketProc;
		wc.hbrBackground = NULL;
		wc.lpszClassName = _T("ru.jk.socket");
		RegisterClassEx(&wc);

		hSocket_ = CreateWindowEx(
			WS_EX_NOACTIVATE,
			_T("ru.jk.socket"),
			_T("TvtPlayJK"),
			WS_POPUP,
			0, 0,
			1, 1,
			m_pApp->GetAppWindow(),
			NULL,
			NULL,
			(LPVOID)this);

		bInitialized = true;
	}

	if (m_pApp->GetFullscreen()) {
		hParent = GetFullscreenWindow();
	}

	hWnd_ = CreateWindowEx(
		WS_EX_LAYERED | WS_EX_NOACTIVATE,
		(LPCTSTR)_T("ru.jk"),
		_T("NicoJK"),
		WS_POPUP | WS_VISIBLE,
		100, 100,
		400, 400,
		hParent,
		NULL,
		NULL,
		(LPVOID)this);

	HWND hButton = CreateWindow(
		_T("BUTTON"),
		_T("�R�����g�Đ��J�n"),
		WS_CHILD | BS_PUSHBUTTON,
		10, 10,
		200, 30,
		hWnd_,
		(HMENU)(INT_PTR)IDB_START,
		NULL,
		NULL
		);

	this->ResizeToVideoWindow();
}

void Cjk::Destroy() {
	ClearObjects();
	DestroyWindow(hWnd_);
	hWnd_ = NULL;
}

void Cjk::DestroySocket() {
	if (socGetflv_ != INVALID_SOCKET) {
		shutdown(socGetflv_, SD_BOTH);
		closesocket(socComment_);
		socGetflv_ = INVALID_SOCKET;
	}
	if (socComment_ != INVALID_SOCKET) {
		shutdown(socComment_, SD_BOTH);
		closesocket(socComment_);
		socComment_ = INVALID_SOCKET;
	}
}

HWND Cjk::GetFullscreenWindow() {
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

void Cjk::ResizeToVideoWindow() {
	RECT rc;
	if (m_pApp->GetFullscreen()) {
		GetWindowRect(GetFullscreenWindow(), &rc);
		SetWindowPos(
			hWnd_,
			HWND_TOPMOST,
			rc.left,
			rc.top,
			rc.right - rc.left,
			rc.bottom - rc.top,
			SWP_NOACTIVATE);
	} else {
		GetWindowRect(m_pApp->GetAppWindow(), &rc);
		SetWindowPos(
			hWnd_,
			NULL,
			rc.left + 10,
			rc.top + 30,
			rc.right - rc.left - 20,
			rc.bottom - rc.top - 60,
			SWP_NOZORDER | SWP_NOACTIVATE);
	}
	SetupObjects();
	manager.PrepareChats(hWnd_);
}

void Cjk::SetupObjects() {
	if (memDC_) {
		ClearObjects();
	}

	HDC wndDC = GetDC(hWnd_);
	memDC_ = CreateCompatibleDC(wndDC);

	RECT rc;
	GetClientRect(hWnd_, &rc);
	hBitmap_ = CreateCompatibleBitmap(wndDC, rc.right, rc.bottom);
	if (!hBitmap_) {
		dprintf(L"CreateCompatibleBitmap failed");
	}
	hPrevBitmap_ = (HBITMAP)SelectObject(memDC_, hBitmap_);
	
	ReleaseDC(hWnd_, wndDC);
}

void Cjk::ClearObjects() {
	if (memDC_ == NULL) {
		return ;
	}
	SelectObject(memDC_, hPrevBitmap_);
	DeleteObject(hBitmap_);
	DeleteDC(memDC_);
	memDC_ = NULL;
}

// token�� ms= �̂悤�� = ������B
bool GetToken(const char *in, const char *token, char *out, int len) {
	const char *start = strstr(in, token);
	if (start) {
		start += strlen(token);
		const char *end = strchr(start, '&');
		if (end) {
			strncpy_s(out, len, start, min(end - start, len - 1));
			out[min(end - start, len - 1)] = '\0';
			return true;
		}
	}
	return false;
}

void Cjk::Open(int jkCh) {
	// �E�B���h�E����U��
	DestroySocket();
	manager.Clear();
	Destroy();
	Create(m_pApp->GetAppWindow());

	msPositionBase_ = 0;
	jkCh_ = jkCh;

	WSAAsyncGetHostByName(hSocket_, WMS_JKHOST_EVENT, "jk.nicovideo.jp", szHostbuf_, MAXGETHOSTSTRUCT);

	this->SetLiveMode();
	this->Start();
}

void Cjk::Start() {
	msPositionBase_ = msPosition_;
	ShowWindow(GetDlgItem(hWnd_, IDB_START), SW_HIDE);
}

void Cjk::SetLiveMode() {
	msSystime_ = 0;
	msPosition_ = timeGetTime();
}

void Cjk::SetPosition(int posms) {
	msSystime_ = timeGetTime();
	msPosition_ = posms;
}

void Cjk::DrawComments(HWND hWnd, HDC hDC) {
	if (!msPositionBase_) {
		return;
	}
	if (memDC_) {
		SetBkMode(memDC_, TRANSPARENT);
		RECT rc = {0, 0, 1920, 1080};
		HBRUSH transBrush = CreateSolidBrush(COLOR_TRANSPARENT);
		FillRect(memDC_, &rc, transBrush);
		DeleteObject(transBrush);

		RECT rcWnd;
		GetClientRect(hWnd, &rcWnd);
		int width = rcWnd.right;
		int yPitch = manager.GetYPitch();

		int basevpos = (msPosition_ - msPositionBase_ + (timeGetTime() - msSystime_)) / 10;
		SetBkMode(memDC_, TRANSPARENT);
		HGDIOBJ hPrevFont = SelectObject(memDC_, manager.GetFont());
		
		std::list<Chat>::const_iterator i = manager.chat_.begin();
		for (; i!=manager.chat_.cend(); ++i) {
			// skip head
			if (i->vpos < basevpos - VPOS_LEN) {
				continue;
			}
			// end
			if (i->vpos > basevpos) {
				break;
			}
			if (i->position == CHAT_POS_SHITA) {
				RECT rc;
				rc.top = rcWnd.bottom - 30 - static_cast<LONG>(i->line * yPitch);
				rc.bottom = rc.top + 30;
				rc.left = 0;
				rc.right = rcWnd.right;
				const std::wstring &text = i->text;
				SetTextColor(memDC_, RGB(0, 0, 0));
				DrawTextW(memDC_, text.c_str(), text.length(), &rc, DT_CENTER | DT_NOPREFIX);
				OffsetRect(&rc, -2, -2);
				SetTextColor(memDC_, i->color);
				DrawTextW(memDC_, text.c_str(), text.length(), &rc, DT_CENTER | DT_NOPREFIX);				
			} else {
				int x = static_cast<int>(width - (float)(i->width + width) * (basevpos - i->vpos) / VPOS_LEN);
				int y = static_cast<int>(10 + yPitch * i->line);
				const std::wstring &text = i->text;
				SetTextColor(memDC_, RGB(0, 0, 0));
				TextOutW(memDC_, x+2, y+2, text.c_str(), text.length());
				SetTextColor(memDC_, i->color);
				TextOutW(memDC_, x, y, text.c_str(), text.length());
			}
		}
		SelectObject(memDC_, hPrevFont);

		BitBlt(hDC, 0, 0, rcWnd.right - rcWnd.left, rc.bottom - rc.top, memDC_, 0, 0, SRCCOPY);
	}
}

LRESULT CALLBACK Cjk::WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
	HDC hdc;
	PAINTSTRUCT ps;

	static Cjk *pThis = NULL;

	switch (msg) {
	case WM_CREATE:
		SetLayeredWindowAttributes(hWnd, COLOR_TRANSPARENT, 0, LWA_COLORKEY);
		pThis = (Cjk*)((CREATESTRUCT*)lp)->lpCreateParams;
		SetTimer(hWnd, jkTimerID, 33, NULL);
		break;
	case WM_COMMAND:
		switch(LOWORD(wp)) {
		case IDB_START:
			pThis->Start();
			break;
		}
		break;
	case WM_SIZE:
		manager.PrepareChats(hWnd);
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		pThis->DrawComments(hWnd, hdc);
		EndPaint(hWnd, &ps);
		break;
	case WM_TIMER:
		InvalidateRect(hWnd, NULL, FALSE);
		UpdateWindow(hWnd);
		break;
	case WM_CLOSE:
		KillTimer(hWnd, jkTimerID);
		PostMessage(GetParent(hWnd), WM_CLOSE, 0, 0);
		DestroyWindow(hWnd);
		break;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MBUTTONDBLCLK:
	case WM_MOUSEMOVE:
		{
			POINT pt;
			RECT rc;

			pt.x = GET_X_LPARAM(lp);
			pt.y = GET_Y_LPARAM(lp);
			HWND hTarget = pThis->m_pApp->GetAppWindow();
			if (pThis->m_pApp->GetFullscreen()) {
				hTarget = pThis->GetFullscreenWindow();
			} 
			MapWindowPoints(hWnd, hTarget, &pt, 1);
			GetClientRect(hTarget,&rc);
			if (PtInRect(&rc,pt)) {
				return SendMessage(hTarget, msg, wp, MAKELPARAM(pt.x,pt.y));
			}
		}
		return 0;
	default:
		return (DefWindowProc(hWnd, msg, wp, lp));
	}
	return 0;
}

LRESULT CALLBACK Cjk::SocketProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
	static Cjk *pThis = NULL;

	switch (msg) {
	case WM_CREATE:
		pThis = (Cjk*)((CREATESTRUCT*)lp)->lpCreateParams;
		pThis->socGetflv_ = INVALID_SOCKET;
		pThis->socComment_ = INVALID_SOCKET;
		manager.SetNotifyWindow(hWnd);
		break;
	case WM_CLOSE:
		PostMessage(GetParent(hWnd), WM_CLOSE, 0, 0);
		DestroyWindow(hWnd);
		break;
	case WM_NEWCOMMENT:
		SendMessage(pThis->hForce_, WM_NEWCOMMENT, 0L, lp);
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
			
			struct sockaddr_in serversockaddr;
			serversockaddr.sin_family = AF_INET;
			serversockaddr.sin_addr.s_addr = *(UINT*)(hostent->h_addr);
			serversockaddr.sin_port = htons(80);
			memset(serversockaddr.sin_zero, 0, sizeof(serversockaddr.sin_zero));
			if (connect(pThis->socGetflv_, (struct sockaddr*)&serversockaddr, sizeof(serversockaddr)) == SOCKET_ERROR) {
				if (WSAGetLastError() != WSAEWOULDBLOCK) {
					MessageBox(hWnd, _T("�j�R�j�R�T�[�o�ւ̐ڑ��Ɏ��s���܂����B"), _T("Error"), MB_OK | MB_ICONERROR);
				}
			}
		}
		return TRUE;
	case WMS_GETFLV_EVENT:
		{
			SOCKET soc = (SOCKET)wp;
			char *szGetTemplate = "GET /api/getflv?v=jk%d HTTP/1.0\r\nHost: jk.nicovideo.jp\r\n\r\n";
			switch(WSAGETSELECTEVENT(lp)) {
			case FD_CONNECT:		/* �T�[�o�ڑ������C�x���g�̏ꍇ */
				OutputDebugString(_T("getflv Connected"));
				break;
			case FD_WRITE:
				OutputDebugString(_T("getflv Write"));
				char szGet[1024];
				wsprintfA(szGet, szGetTemplate, pThis->jkCh_);
				send(soc, szGet, strlen(szGet)+1, 0);
				break;
			case FD_READ:
				char buf[10240];
				int read;
				read = recv(soc, buf, sizeof(buf)-1, 0);
				shutdown(soc, SD_BOTH);
				if (read == SOCKET_ERROR) {
					break;
				}
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
						memset(serversockaddr.sin_zero, 0, sizeof(serversockaddr.sin_zero));

						// �R�����g�T�[�o�ɐڑ�
						if (connect(pThis->socComment_, (struct sockaddr*)&serversockaddr, sizeof(serversockaddr)) == SOCKET_ERROR) {
							if (WSAGetLastError() != WSAEWOULDBLOCK) {
								MessageBox(hWnd, _T("�R�����g�T�[�o�ւ̐ڑ��Ɏ��s���܂����B"), _T("Error"), MB_OK | MB_ICONERROR);
							}
						}
					}
				}
				OutputDebugStringA(buf);
				break;
			case FD_CLOSE:
				OutputDebugString(_T("getflv Closed"));
				closesocket(soc);
				pThis->socGetflv_ = INVALID_SOCKET;
				break;
			}
		}
		return TRUE;
	case WMS_COMMENT_EVENT:
		{
			SOCKET soc = (SOCKET)wp;
			const char *szRequestTemplate = "<thread res_from=\"-10\" version=\"20061206\" thread=\"%s\" />";
			switch(WSAGETSELECTEVENT(lp)) {
			case FD_CONNECT:		/* �T�[�o�ڑ������C�x���g�̏ꍇ */
				OutputDebugString(_T("Comment Connected"));
				break;
			case FD_WRITE:
				OutputDebugString(_T("Comment Write"));
				char szRequest[1024];
				wsprintfA(szRequest, szRequestTemplate, pThis->szThread_);
				send(soc, szRequest, strlen(szRequest) + 1, 0); // '\0'�܂ő���
				break;
			case FD_READ:
				char buf[10240];
				int read;
				read = recv(soc, buf, sizeof(buf)-1, 0);
				if (read == SOCKET_ERROR) {
					break;
				}
				wchar_t wcsBuf[10240];
				int wcsLen;
				wcsLen = MultiByteToWideChar(CP_UTF8, 0, buf, read, wcsBuf, 10240);
				int pos;
				pos = 0;
				do {
					wchar_t *line = &wcsBuf[pos];
					int len = wcslen(line);
					manager.insert(line, timeGetTime() / 10);
					pos += len + 1;
				} while(pos < wcsLen);
				// �Â��̂͏���
				manager.trim(timeGetTime() / 10 - VPOS_LEN * 3);
				break;
			case FD_CLOSE:
				OutputDebugString(_T("Comment Closed"));
				closesocket(soc);
				pThis->socComment_ = INVALID_SOCKET;
				break;
			}
		}
		return TRUE;
	}
	return DefWindowProc(hWnd, msg, wp, lp);
}

BOOL Cjk::WindowMsgCallback(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam,LRESULT *pResult) {
	if (!hWnd_) {
		return FALSE;
	}

    switch (uMsg) {
	case WM_ACTIVATE:
		if (WA_INACTIVE == LOWORD(wParam)) {
			return FALSE;
		}
	case WM_MOVE:
    case WM_SIZE:
		ResizeToVideoWindow();
		break;
	}
	return FALSE;
}

LRESULT Cjk::EventCallback(UINT Event, LPARAM lParam1, LPARAM lParam2) {
	if (!hWnd_) {
		return FALSE;
	}

	switch (Event) {
    case TVTest::EVENT_PLUGINENABLE:
        // �v���O�C���̗L����Ԃ��ω�����
        //return pThis->EnablePlugin(lParam1 != 0);
		break;
    case TVTest::EVENT_FULLSCREENCHANGE:
        // �S��ʕ\����Ԃ��ω�����
        if (m_pApp->IsPluginEnabled()) {
			Destroy();
			Create(m_pApp->GetAppWindow());
		}
        break;
	}
	return 0;
}
