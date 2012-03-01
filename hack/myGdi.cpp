#include "stdafx.h"
#include "myGdi.h"
#include <string>
#include <hash_map>
#include <unordered_set>

std::hash_map<std::string, int> kernings;
std::unordered_set<std::string> calculated;

unsigned char gdi_buffer[2048*2048*4];
RECT invalidateRects[1024];
int invalidateRectsCount = 0;

void gdi_write_string(HDC hdc, int nXStart, int nYStart, LPCTSTR lpString, int cchString)
{
	LOGFONT lf;
	GetObject(GetCurrentObject(hdc, OBJ_FONT), sizeof(lf), &lf);
	int height = lf.lfHeight < 0 ? -lf.lfHeight : lf.lfHeight;
	COLORREF c = GetTextColor(hdc);
	MAT2 m2;
	m2.eM11.fract = 0;
	m2.eM11.value = 1;
	m2.eM12.fract = 0;
	m2.eM12.value = 0;
	m2.eM21.fract = 0;
	m2.eM21.value = 0;
	m2.eM22.fract = 0;
	m2.eM22.value = 1;
	char lc = 0;
	unsigned char **buffers = new unsigned char*[cchString];
	GLYPHMETRICS *gms = new GLYPHMETRICS[cchString];
	int *kerns = new int[cchString];
	int i;
	std::string kerningstring = std::string(lf.lfFaceName);
	kerningstring += (char) lf.lfWidth + 1;
	kerningstring += (char) lf.lfHeight + 1;
	kerningstring += (char) lf.lfWeight / 100 + 1;
	kerningstring += lf.lfItalic + 1;

	// get kerning pairs
	if (calculated.find(kerningstring) == calculated.end())
	{
		int numpairs = GetKerningPairs(hdc, 0, 0);

		if (numpairs)
		{
			KERNINGPAIR *pairs = new KERNINGPAIR[numpairs];
			GetKerningPairs(hdc, numpairs, pairs);

			for (int i = 0; i < numpairs; i++)
			{
				if (pairs[i].wFirst > 255 || pairs[i].wSecond > 255)
					continue;
				std::string ks = kerningstring;
				ks += (char) pairs[i].wFirst;
				ks += (char) pairs[i].wSecond;
				kernings.insert(std::pair<std::string, int>(ks, pairs[i].iKernAmount));
			}

			delete [] pairs;
		}

		calculated.insert(kerningstring);
	}

	int totalwidth = 0;

	for (i = 0; i < cchString; i++)
	{
		std::string ks;
		if(lc)
		{
			ks = kerningstring;
			ks += lc;
			ks += lpString[i];
		}
		int size = GetGlyphOutline(hdc, lpString[i], GGO_BITMAP, &gms[i], 0, 0, &m2);
		buffers[i] = new unsigned char[size];
		GetGlyphOutline(hdc, lpString[i], GGO_BITMAP, &gms[i], size, buffers[i], &m2);
		if (lc && kernings.find(ks) != kernings.end())
		{
			kerns[i] = kernings[ks];
			totalwidth += kerns[i];
		}
		else
		{
			kerns[i] = 0;
		}
		totalwidth += gms[i].gmCellIncX;
		lc = lpString[i];
	}

	UINT align = GetTextAlign(hdc);

	if (align & TA_RIGHT)
		nXStart -= totalwidth;
	else if (align & TA_CENTER)
		nXStart -= totalwidth / 2;

	for (i = 0; i < cchString; i++)
	{
		nXStart += kerns[i];
		DWORD pitch = ((gms[i].gmBlackBoxX / 8) + 3) & ~3;
		if (!pitch) pitch = 4;
		for (DWORD j = 0; j < gms[i].gmBlackBoxX; j++)
			for (DWORD k = 0; k < gms[i].gmBlackBoxY; k++)
			{
				unsigned char pixel = ((buffers[i][(k * pitch) + (j / 8)] >> ((7 - (j + 8)) & 7)) & 1) * 0xFF;
				if (!pixel) continue;
				int gdi_offset = (tex_w * (k + nYStart - gms[i].gmptGlyphOrigin.y + height) + (j + nXStart)) * 4;
				gdi_buffer[gdi_offset] = (unsigned char) (pixel * ((c & 0x000000FF) / 255.f));
				gdi_buffer[gdi_offset + 1] = (unsigned char) (pixel * (((c & 0x0000FF00) >> 8) / 255.f));
				gdi_buffer[gdi_offset + 2] = (unsigned char) (pixel * (((c & 0x00FF0000) >> 16) / 255.f));
				gdi_buffer[gdi_offset + 3] = pixel;
			}

		delete [] buffers[i];
		nXStart += gms[i].gmCellIncX;
	}
	delete [] buffers;
	delete [] gms;
	delete [] kerns;
}

void gdi_clear(const RECT *lpRect)
{
	if (tex_w == 0 || tex_h == 0)
		return;
	if (lpRect == 0)
		return;
	for (int i = lpRect->top; i <= lpRect->bottom; i++)
		memset(&gdi_buffer[(i * tex_w + lpRect->left) * 4], 0, (lpRect->right - lpRect->left) * 4);
}

void gdi_clear_all()
{
	invalidateRectsCount = 0;
	memset(gdi_buffer, 0, 2048*2048*4);
}

unsigned char *gdi_get_buffer()
{
	return gdi_buffer;
}

void gdi_invalidate(const RECT *lpRect)
{
	if (lpRect == 0)
		return;
	if (invalidateRectsCount >= 1024)
		return;
	memcpy(&invalidateRects[invalidateRectsCount++], lpRect, sizeof(RECT));
}

void gdi_run_invalidations()
{
	if (invalidateRectsCount)
		for (;invalidateRectsCount >= 0; --invalidateRectsCount)
			gdi_clear(&invalidateRects[invalidateRectsCount]);

	gdi_clear_invalidations();
}

void gdi_clear_invalidations()
{
	invalidateRectsCount = 0;
}