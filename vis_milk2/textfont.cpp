#include "textfont.h"
#include <new>
#include <vector>

namespace
{
struct TextVertex
{
    float x, y, z, rhw;
    float u, v;
};
}

CTextFont::CTextFont(IDirect3DDevice9* device, INT height, UINT width, UINT weight,
    BOOL italic, BYTE charset, BYTE outputPrecision, BYTE quality, BYTE pitch,
    const wchar_t* faceName)
    : m_refCount(1), m_device(device), m_font(NULL)
{
    if (m_device)
        m_device->AddRef();
    m_font = CreateFontW(height, width, 0, 0, weight, italic, FALSE, FALSE,
        charset, outputPrecision, CLIP_DEFAULT_PRECIS, quality, pitch, faceName);
}

CTextFont::~CTextFont()
{
    if (m_font)
        DeleteObject(m_font);
    if (m_device)
        m_device->Release();
}

ULONG CTextFont::AddRef()
{
    return static_cast<ULONG>(InterlockedIncrement(&m_refCount));
}

ULONG CTextFont::Release()
{
    const ULONG refs = static_cast<ULONG>(InterlockedDecrement(&m_refCount));
    if (!refs)
        delete this;
    return refs;
}

int CTextFont::DrawTextA(HDC, const char* text, int length, RECT* rect, DWORD flags, DWORD color)
{
    if (!text)
        return 0;
    const int sourceLength = (length < 0) ? static_cast<int>(strlen(text)) : length;
    const int wideLength = MultiByteToWideChar(CP_ACP, 0, text, sourceLength, NULL, 0);
    if (wideLength <= 0)
        return 0;

    std::vector<wchar_t> wideText(wideLength + 1);
    MultiByteToWideChar(CP_ACP, 0, text, sourceLength, wideText.data(), wideLength);
    wideText[wideLength] = L'\0';
    return DrawTextW(NULL, wideText.data(), wideLength, rect, flags, color);
}

int CTextFont::DrawTextW(HDC, const wchar_t* text, int length, RECT* rect, DWORD flags, DWORD color)
{
    if (!m_font || !text || !rect)
        return 0;

    const int count = (length < 0) ? static_cast<int>(wcslen(text)) : length;
    HDC dc = CreateCompatibleDC(NULL);
    if (!dc)
        return 0;

    HGDIOBJ oldFont = SelectObject(dc, m_font);
    RECT measured = *rect;
    const int height = ::DrawTextW(dc, text, count, &measured, flags);
    SelectObject(dc, oldFont);
    DeleteDC(dc);

    if (flags & DT_CALCRECT)
    {
        *rect = measured;
        return measured.bottom - measured.top;
    }

    if (!m_device || height <= 0)
        return height;
    return DrawBitmapText(text, count, height, rect, flags, color);
}

int CTextFont::DrawBitmapText(const wchar_t* text, int length, int drawnHeight, RECT* rect, DWORD flags, DWORD color)
{
    const int width = rect->right - rect->left;
    const int height = rect->bottom - rect->top;
    if (width <= 0 || height <= 0 || width > 8192 || height > 8192)
        return 0;

    BITMAPINFO bitmapInfo = {};
    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    HDC screenDc = GetDC(NULL);
    void* bitmapPixels = NULL;
    HBITMAP bitmap = CreateDIBSection(screenDc, &bitmapInfo, DIB_RGB_COLORS, &bitmapPixels, NULL, 0);
    if (screenDc)
        ReleaseDC(NULL, screenDc);
    if (!bitmap || !bitmapPixels)
    {
        if (bitmap) DeleteObject(bitmap);
        return 0;
    }
    ZeroMemory(bitmapPixels, static_cast<size_t>(width) * height * 4);

    HDC memoryDc = CreateCompatibleDC(NULL);
    if (!memoryDc)
    {
        DeleteObject(bitmap);
        return 0;
    }
    HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);
    HGDIOBJ oldFont = SelectObject(memoryDc, m_font);
    SetBkMode(memoryDc, TRANSPARENT);
    SetTextColor(memoryDc, RGB(255, 255, 255));
    RECT drawRect = { 0, 0, width, height };
    ::DrawTextW(memoryDc, text, length, &drawRect, flags & ~DT_CALCRECT);
    SelectObject(memoryDc, oldFont);
    SelectObject(memoryDc, oldBitmap);
    DeleteDC(memoryDc);

    IDirect3DTexture9* texture = NULL;
    HRESULT result = m_device->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8,
        D3DPOOL_MANAGED, &texture, NULL);
    if (FAILED(result))
    {
        DeleteObject(bitmap);
        return 0;
    }

    D3DLOCKED_RECT locked;
    result = texture->LockRect(0, &locked, NULL, 0);
    if (FAILED(result))
    {
        texture->Release();
        DeleteObject(bitmap);
        return 0;
    }

    const BYTE* source = static_cast<const BYTE*>(bitmapPixels);
    const BYTE red = static_cast<BYTE>((color >> 16) & 0xff);
    const BYTE green = static_cast<BYTE>((color >> 8) & 0xff);
    const BYTE blue = static_cast<BYTE>(color & 0xff);
    const BYTE opacity = static_cast<BYTE>((color >> 24) & 0xff);
    for (int y = 0; y < height; ++y)
    {
        BYTE* destination = static_cast<BYTE*>(locked.pBits) + y * locked.Pitch;
        for (int x = 0; x < width; ++x)
        {
            const size_t offset = (static_cast<size_t>(y) * width + x) * 4;
            const BYTE coverage = max(source[offset], max(source[offset + 1], source[offset + 2]));
            destination[x * 4 + 0] = blue;
            destination[x * 4 + 1] = green;
            destination[x * 4 + 2] = red;
            destination[x * 4 + 3] = static_cast<BYTE>(coverage * opacity / 255);
        }
    }
    texture->UnlockRect(0);
    DeleteObject(bitmap);

    D3DVIEWPORT9 viewport;
    m_device->GetViewport(&viewport);
    const float left = static_cast<float>(viewport.X + rect->left) - 0.5f;
    const float top = static_cast<float>(viewport.Y + rect->top) - 0.5f;
    const float right = left + width;
    const float bottom = top + height;
    TextVertex vertices[4] = {
        { left, top, 0.0f, 1.0f, 0.0f, 0.0f },
        { right, top, 0.0f, 1.0f, 1.0f, 0.0f },
        { left, bottom, 0.0f, 1.0f, 0.0f, 1.0f },
        { right, bottom, 0.0f, 1.0f, 1.0f, 1.0f }
    };

    IDirect3DStateBlock9* state = NULL;
    if (SUCCEEDED(m_device->CreateStateBlock(D3DSBT_ALL, &state)))
        state->Capture();
    m_device->SetTexture(0, texture);
    m_device->SetVertexShader(NULL);
    m_device->SetPixelShader(NULL);
    m_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
    m_device->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    m_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    m_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    m_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    m_device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    m_device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    m_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(TextVertex));
    if (state)
    {
        state->Apply();
        state->Release();
    }

    texture->Release();
    return drawnHeight;
}

HRESULT CreateTextFont(IDirect3DDevice9* device, INT height, UINT width, UINT weight,
    UINT mipLevels, BOOL italic, BYTE charset, BYTE outputPrecision, BYTE quality, BYTE pitch,
    const wchar_t* faceName, CTextFont** font)
{
    UNREFERENCED_PARAMETER(mipLevels);
    if (!font)
        return E_POINTER;
    *font = new(std::nothrow) CTextFont(device, height, width, weight, italic,
        charset, outputPrecision, quality, pitch, faceName);
    if (!*font)
        return E_OUTOFMEMORY;
    if (!(*font)->IsValid())
    {
        (*font)->Release();
        *font = NULL;
        return HRESULT_FROM_WIN32(GetLastError());
    }
    return S_OK;
}