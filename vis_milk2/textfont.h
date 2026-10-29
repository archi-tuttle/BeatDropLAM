#ifndef BEATDROP_TEXT_FONT_H
#define BEATDROP_TEXT_FONT_H

#include <windows.h>
#include <d3d9.h>

class CTextFont
{
public:
    CTextFont(IDirect3DDevice9* device, INT height, UINT width, UINT weight,
        BOOL italic, BYTE charset, BYTE outputPrecision, BYTE quality, BYTE pitch,
        const wchar_t* faceName);
    ~CTextFont();

    ULONG AddRef();
    ULONG Release();
    int DrawTextA(HDC dc, const char* text, int length, RECT* rect, DWORD flags, DWORD color);
    int DrawTextW(HDC dc, const wchar_t* text, int length, RECT* rect, DWORD flags, DWORD color);
    bool IsValid() const { return m_font != NULL; }

private:
    int DrawBitmapText(const wchar_t* text, int length, int drawnHeight, RECT* rect, DWORD flags, DWORD color);

    LONG m_refCount;
    IDirect3DDevice9* m_device;
    HFONT m_font;
};

HRESULT CreateTextFont(IDirect3DDevice9* device, INT height, UINT width, UINT weight,
    UINT mipLevels, BOOL italic, BYTE charset, BYTE outputPrecision, BYTE quality, BYTE pitch,
    const wchar_t* faceName, CTextFont** font);

#endif