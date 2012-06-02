
#pragma once

#include "stdafx.h"
#include "TVTestPlugin.h"

#include <string>
#include <vector>
#include <list>
#include <cstdio>
#include <algorithm>
#include <functional>

#define jkTimerID 1021
#define jkTimerResizeID 1022
#define IDB_START 1

const int VPOS_LEN = 400;
#define COLOR_TRANSPARENT RGB(12, 12, 12)

#define WM_NEWCOMMENT (WM_APP+200)

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
	Chat()
		: vpos(-1000),
		  text(L""),
		  line(0),
		  width(0),
		  color(0),
		  position(CHAT_POS_DEFAULT)
	{ }
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

class Cjk {
	TVTest::CTVTestApp *m_pApp;

	HWND hWnd_;
	HWND hSocket_;
	HWND hForce_;

	DWORD msPosition_;
	DWORD msSystime_;
	DWORD msPositionBase_;

	// �ʐM�p
	int jkCh_;
	char szHostbuf_[MAXGETHOSTSTRUCT];
	char szThread_[1024];
	SOCKET socGetflv_;
	SOCKET socComment_;

	static bool doResize;
	static Cjk *pSelf;

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);
	static LRESULT CALLBACK SocketProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);

public:
	Cjk(TVTest::CTVTestApp *pApp, HWND hForce, bool disableDWrite);
	~Cjk();
	void Create(HWND hParent);
	void Destroy();
	void DestroySocket();
	void ResizeToVideoWindow();
	HWND GetFullscreenWindow();

	void Open(int jkCh);
	void SetPosition(int posms);
	void SetLiveMode();
	void Start();

	void DrawComments(HWND hWnd);

	// TVTest�̃��b�Z�[�W
	BOOL WindowMsgCallback(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam,LRESULT *pResult);
	LRESULT EventCallback(UINT Event, LPARAM lParam1, LPARAM lParam2);
};
