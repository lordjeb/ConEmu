
/*
Copyright (c) 2013-present Maximus5
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ''AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "Header.h"
#include "HtmlCopy.h"

using ConEmu::PaletteColors;


CFormatCopy::CFormatCopy()
{
	mn_AllItemsLen = 0;
	bCopyRawCodes = false;
	szUTF8[0] = 0;  // mitigate V730
}

CFormatCopy::~CFormatCopy()
{
	for (int i = 0; i < m_Items.size(); i++)
		free(m_Items[i].ptr);
	m_Items.clear();
}

void CFormatCopy::LineAdd(LPCWSTR asText, const CharAttr* apAttr, INT_PTR nLen)
{
	CharAttr a = *apAttr;
	LPCWSTR ps = asText;
	const CharAttr* pa = apAttr;
	ParBegin();
	while ((nLen--) > 0)
	{
		pa++; ps++;
		if (!nLen || (pa->All != a.All))
		{
			TextAdd(asText, ps - asText, a.crForeColor, a.crBackColor, (a.nFontIndex & 1) == 1, (a.nFontIndex & 2) == 2, (a.nFontIndex & 4) == 4);
			asText = ps; apAttr = pa;
			a = *apAttr;
		}
	}
	ParEnd();
}

void CFormatCopy::LineAdd(LPCWSTR asText, const WORD* apAttr, const PaletteColors& pPal, INT_PTR nLen)
{
	WORD a = *apAttr;
	LPCWSTR ps = asText;
	const WORD* pa = apAttr;
	ParBegin();
	while ((nLen--) > 0)
	{
		pa++; ps++;
		if (!nLen || (*pa != a))
		{
			TextAdd(asText, ps - asText, pPal[CONFORECOLOR(a)], pPal[CONBACKCOLOR(a)]);
			asText = ps; apAttr = pa;
			a = *apAttr;
		}
	}
	ParEnd();
}

void CFormatCopy::RawAdd(LPCWSTR asText, INT_PTR cchLen)
{
	if (bCopyRawCodes)
	{
		// We store data "as is" to copy RAW sequences/html to clipboard
		txt t = { cchLen };
		t.pwsz = (wchar_t*)malloc((cchLen + 1) * sizeof(wchar_t));
		wmemmove(t.pwsz, asText, cchLen);
		t.pwsz[cchLen] = 0;
		m_Items.push_back(t);
		mn_AllItemsLen += cchLen;
	}
	else
	{
		// We need to convert data to UTF-8 (HTML clipboard format)
		int nUtfLen = WideCharToMultiByte(CP_UTF8, 0, asText, cchLen, szUTF8, countof(szUTF8) - 1, 0, 0);
		if (nUtfLen > 0)
		{
			txt t = { nUtfLen };
			t.pasz = (char*)malloc(nUtfLen + 1);
			memmove(t.pasz, szUTF8, nUtfLen);
			t.pasz[nUtfLen] = 0;
			m_Items.push_back(t);
			mn_AllItemsLen += nUtfLen;
		}
	}
}

HGLOBAL CFormatCopy::CreateResultInternal(const char* pszHdr /*= nullptr*/, INT_PTR nHdrLen /*= 0*/)
{
	INT_PTR nCharCount = (nHdrLen/*Header*/ + mn_AllItemsLen + 3);
	HGLOBAL hCopy = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, (nCharCount + 1) * (bCopyRawCodes ? sizeof(wchar_t) : sizeof(char)));

	if (!hCopy)
	{
		Assert(hCopy != nullptr);
	}
	else if (bCopyRawCodes)
	{
		wchar_t* pch = (wchar_t*)GlobalLock(hCopy);
		if (!pch)
		{
			Assert(pch != nullptr);
			GlobalFree(hCopy);
			hCopy = nullptr;
		}
		else
		{
			for (int i = 0; i < m_Items.size(); i++)
			{
				txt t = m_Items[i];
				wmemmove(pch, t.pwsz, t.nLen);
				pch += t.nLen;
			}

			*pch = 0;

			GlobalUnlock(hCopy);
		}
	}
	else
	{
		char* pch = (char*)GlobalLock(hCopy);
		if (!pch)
		{
			Assert(pch != nullptr);
			GlobalFree(hCopy);
			hCopy = nullptr;
		}
		else
		{
			if (pszHdr && (nHdrLen > 0))
			{
				memmove(pch, pszHdr, nHdrLen);
				pch += nHdrLen;
			}

			for (int i = 0; i < m_Items.size(); i++)
			{
				txt t = m_Items[i];
				memmove(pch, t.pasz, t.nLen);
				pch += t.nLen;
			}

			*pch = 0;

			GlobalUnlock(hCopy);
		}
	}

	return hCopy;
}

LPCWSTR CHtmlCopy::FormatColor(COLORREF clr, wchar_t* rsBuf)
{
	swprintf_c(rsBuf, 8/*#SECURELEN*/, L"#%02X%02X%02X",
		(UINT)(clr & 0xFF), (UINT)((clr & 0xFF00) >> 8), (UINT)((clr & 0xFF0000) >> 16));
	return rsBuf;
}

CHtmlCopy::CHtmlCopy(bool abPlainHtml, LPCWSTR asBuild, LPCWSTR asFont, int anFontHeight, COLORREF crFore, COLORREF crBack) : CFormatCopy()
{
	mb_Finalized = mb_ParOpened = false;
	mn_TextStart = 0; mn_TextEnd = 0;
	szBack[0] = szFore[0] = szId[0] = szBold[0] = szItalic[0] = szUnderline[0] = 0;  // mitigate V730

	bCopyRawCodes = abPlainHtml;

	LPCWSTR pszHtml = L"<!DOCTYPE>\r\n"
		L"<HTML>\r\n"
		L"<HEAD>\r\n"
		L"<META http-equiv=\"content-type\" content=\"text/html; charset=utf-8\">\r\n"
		L"<META name=\"GENERATOR\" content=\"ConEmu %s[%u]\">\n"
		L"</HEAD>\r\n"
		L"<BODY>\r\n";
	swprintf_c(szTemp, pszHtml, asBuild, WIN3264TEST(32, 64));
	RawAdd(szTemp, _tcslen(szTemp));

	mn_TextStart = mn_AllItemsLen; mn_TextEnd = 0;
	msprintf(szTemp, countof(szTemp),
		L"<DIV class=\"%s\" style=\"font-family: '%s'; font-size: %upx; text-align: start; text-indent: 0px; margin: 0;\">\r\n",
		asBuild, asFont, anFontHeight);
	RawAdd(szTemp, _tcslen(szTemp));
}

CHtmlCopy::~CHtmlCopy()
{

}

void CHtmlCopy::ParBegin()
{
	if (mb_ParOpened)
		ParEnd();
	mb_ParOpened = true;
}

void CHtmlCopy::ParEnd()
{
	if (!mb_ParOpened)
		return;
	mb_ParOpened = false;
	RawAdd(L"<br>\r\n", 6);
}

void CHtmlCopy::TextAdd(LPCWSTR asText, INT_PTR cchLen, COLORREF crFore, COLORREF crBack, bool Bold /*= false*/, bool Italic /*= false*/, bool Underline /*= false*/)
{
	// Open (special colors, fonts, outline?)
	swprintf_c(szFore, L"color: %s; ", FormatColor(crFore, szId));
	swprintf_c(szBack, L"background-color: %s; ", FormatColor(crBack, szId));
	wcscpy_c(szBold, Bold ? L"font-weight: bold; " : L"");
	wcscpy_c(szItalic, Italic ? L"font-style: italic; " : L"");
	wcscpy_c(szUnderline, Underline ? L"text-decoration: underline; " : L"");
	swprintf_c(szTemp, L"<span style=\"%s%s%s%s%s\">", szFore, szBack, szBold, szItalic, szUnderline);
	RawAdd(szTemp, _tcslen(szTemp));

	// Text
	LPCWSTR pszEnd = asText + cchLen;
	wchar_t* pszDst = szTemp;
	wchar_t* pszDstEnd = szTemp + countof(szTemp);
	const INT_PTR minTagLen = 10; // ~~
	bool bSpace = true;
	while (asText < pszEnd)
	{
		if ((pszDst + minTagLen) >= pszDstEnd)
		{
			RawAdd(szTemp, pszDst - szTemp);
			pszDst = szTemp;
		}

		switch (*asText)
		{
		case 0:
		case L' ':
		case 0xA0:
			if (bSpace || (*asText == 0xA0))
			{
				_wcscpy_c(pszDst, minTagLen, L"&nbsp;");
				pszDst += 6;
			}
			else
			{
				*(pszDst++) = L' ';
				bSpace = true;
			}
			break;
		case L'&':
			_wcscpy_c(pszDst, minTagLen, L"&amp;");
			pszDst += 5;
			break;
		case L'"':
			_wcscpy_c(pszDst, minTagLen, L"&quot;");
			pszDst += 6;
			break;
		case L'<':
			_wcscpy_c(pszDst, minTagLen, L"&lt;");
			pszDst += 4;
			break;
		case L'>':
			_wcscpy_c(pszDst, minTagLen, L"&gt;");
			pszDst += 4;
			break;
		default:
			*(pszDst++) = *asText;
			bSpace = false;
		}

		asText++;
	}

	if (pszDst > szTemp)
	{
		RawAdd(szTemp, pszDst - szTemp);
	}

	// Fin
	RawAdd(L"</span>", 7);
}

HGLOBAL CHtmlCopy::CreateResult()
{
	if (!mb_Finalized)
	{
		if (mb_ParOpened)
			ParEnd();
		mb_Finalized = true;
		RawAdd(L"</DIV>", 6);
		mn_TextEnd = mn_AllItemsLen;

		LPCWSTR pszFin = L"\r\n</BODY>\r\n</HTML>\r\n";
		RawAdd(pszFin, _tcslen(pszFin));
	}

	char szHdr[255];
	INT_PTR nHdrLen = 0;
	if (!bCopyRawCodes)
	{
		// First, we need to calculate header len
		LPCSTR pszURL = CEHOMEPAGE_A;
		LPCSTR pszHdrFmt = "Version:1.0\r\n"
			"StartHTML:%09u\r\n"
			"EndHTML:%09u\r\n"
			"StartFragment:%09u\r\n"
			"EndFragment:%09u\r\n"
			"StartSelection:%09u\r\n"
			"EndSelection:%09u\r\n"
			"SourceURL:%s\r\n";
		sprintf_c(szHdr, pszHdrFmt, 0, 0, 0, 0, 0, 0, pszURL);
		nHdrLen = strlen(szHdr);
		// Calculate positions
		sprintf_c(szHdr, pszHdrFmt,
			(UINT)(nHdrLen), (UINT)(nHdrLen + mn_AllItemsLen),
			(UINT)(nHdrLen + mn_TextStart), (UINT)(nHdrLen + mn_TextEnd),
			(UINT)(nHdrLen + mn_TextStart), (UINT)(nHdrLen + mn_TextEnd),
			pszURL);
		_ASSERTE(nHdrLen == strlen(szHdr));
	}

	// Just compile all string to one block
	HGLOBAL hCopy = CreateResultInternal(bCopyRawCodes ? nullptr : szHdr, nHdrLen);
	return hCopy;
}

LPCWSTR CAnsiCopy::FormatColor(COLORREF crFore, COLORREF crBack, bool Bold, bool Italic, bool Underline, wchar_t(&rsBuf)[80])
{
	wchar_t szBk[80], szBIU[16] = L"";
	UINT Ansi[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };

	// Prepare styles first
	if (Bold != mb_Bold)
	{
		mb_Bold = Bold;
		wcscat_c(szBIU, Bold ? L";1" : L";22");
	}
	if (Italic != mb_Italic)
	{
		mb_Italic = Italic;
		wcscat_c(szBIU, Italic ? L";3" : L";23");
	}
	if (Underline != mb_Underline)
	{
		mb_Underline = Underline;
		wcscat_c(szBIU, Underline ? L";3" : L";24");
	}

	// 0..15 indexed colors
	for (int i = 0; i < 8; i++)
	{
		if (clrPalette[i] == crFore)
		{
			swprintf_c(rsBuf, L"\x1B[%um", 30 + Ansi[i]);
			goto do_bk;
		}
	}
	for (int i = 8; i < 16; i++)
	{
		if (clrPalette[i] == crFore)
		{
			swprintf_c(rsBuf, L"\x1B[%um", 90 + Ansi[i - 8]);
			goto do_bk;
		}
	}

	//TODO: xterm-256 palette?

	// xterm 24-bit colors
	swprintf_c(rsBuf, L"\x1B[38;2;%u;%u;%um",
		(UINT)(crFore & 0xFF), (UINT)((crFore & 0xFF00) >> 8), (UINT)((crFore & 0xFF0000) >> 16));

do_bk:
	// 0..15 indexed colors
	for (int i = 0; i < 8; i++)
	{
		if (clrPalette[i] == crBack)
		{
			swprintf_c(szBk, L"\x1B[%um", 40 + Ansi[i]);
			goto add_bk;
		}
	}
	for (int i = 8; i < 16; i++)
	{
		if (clrPalette[i] == crBack)
		{
			swprintf_c(szBk, L"\x1B[%um", 100 + Ansi[i - 8]);
			goto add_bk;
		}
	}

	//TODO: xterm-256 palette?

	// xterm 24-bit colors
	swprintf_c(szBk, L"\x1B[48;2;%u;%u;%um",
		(UINT)(crBack & 0xFF), (UINT)((crBack & 0xFF00) >> 8), (UINT)((crBack & 0xFF0000) >> 16));
add_bk:
	wcscat_c(rsBuf, szBk);
	if (szBIU[0])
	{
		wcscat_c(szBIU, L"m");
		wcscat_c(rsBuf, L"\x1B[");
		wcscat_c(rsBuf, szBIU + 1);
	}
	return rsBuf;
}

CAnsiCopy::CAnsiCopy(const PaletteColors& pclrPalette, COLORREF crFore, COLORREF crBack) : CFormatCopy()
, clrPalette(pclrPalette)
{
	mb_ParOpened = false;
	mb_Bold = mb_Italic = mb_Underline = false;
	memset(szSeq, 0, sizeof(szSeq));
	memset(szTemp, 0, sizeof(szTemp));

	bCopyRawCodes = true;

#if 0
	// Init format
	FormatColor(crFore, crBack, szSeq);
	// Unset Bold/Italic/Underline
	RawAdd(szTemp, _tcslen(szTemp));
#endif
}

CAnsiCopy::~CAnsiCopy()
{

}

void CAnsiCopy::ParBegin()
{
	if (mb_ParOpened)
		ParEnd();
	mb_ParOpened = true;
}

void CAnsiCopy::ParEnd()
{
	if (!mb_ParOpened)
		return;
	mb_ParOpened = false;
	RawAdd(L"\x1B[m\r\n", 5);
}

void CAnsiCopy::TextAdd(LPCWSTR asText, INT_PTR cchLen, COLORREF crFore, COLORREF crBack, bool Bold /*= false*/, bool Italic /*= false*/, bool Underline /*= false*/)
{
	// Set colors
	wcscpy_c(szTemp, FormatColor(crFore, crBack, Bold, Italic, Underline, szSeq));
	// Set/Unset Bold/Italic/Underline
	RawAdd(szTemp, _tcslen(szTemp));

	// Text
	LPCWSTR pszEnd = asText + cchLen;
	wchar_t* pszDst = szTemp;
	wchar_t* pszDstEnd = szTemp + countof(szTemp);
	const INT_PTR minTagLen = 10; // ~~

	while (asText < pszEnd)
	{
		if ((pszDst + minTagLen) >= pszDstEnd)
		{
			RawAdd(szTemp, pszDst - szTemp);
			pszDst = szTemp;
		}

		switch (*asText)
		{
		case 0:
			*(pszDst++) = L' ';
			break;
			//case L'\\':
			//	_wcscpy_c(pszDst, minTagLen, L"\\\\");
			//	pszDst += 2;
			//	break;
		default:
			*(pszDst++) = *asText;
		}

		asText++;
	}

	if (pszDst > szTemp)
	{
		RawAdd(szTemp, pszDst - szTemp);
	}

	// Fin
}

HGLOBAL CAnsiCopy::CreateResult()
{
	// Reset coloring
	if (mb_ParOpened)
	{
		wcscpy_c(szTemp, L"\x1B[m");
		RawAdd(szTemp, _tcslen(szTemp));
	}

	// Just compile all string to one block
	HGLOBAL hCopy = CreateResultInternal(nullptr, 0);
	return hCopy;
}
