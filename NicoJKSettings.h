#pragma once
#include <string>
#include "stdafx.h"

/* �j�R�j�R������{�ݒ� */
struct TNicoJKSettings
{
	/* �s�Ԕ{�� */
	int				timerInterval;
	float			commentLineMargin;
	float			commentFontOutline;
	float			commentSize;
	int				commentSizeMin;
	int				commentSizeMax;
	std::wstring	commentFontName;
	int				commentFontBold;
};

class CNJIni
{
	static TNicoJKSettings		settings;

public:
	static void LoadFromIni(TCHAR* iniFile);
	static TNicoJKSettings* GetSettings() { return &settings; }
};
