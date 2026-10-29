/*
  LICENSE
  -------
Copyright 2005-2013 Nullsoft, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer. 

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution. 

  * Neither the name of Nullsoft nor the names of its contributors may be used to 
    endorse or promote products derived from this software without specific prior written permission. 
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR 
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND 
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR 
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT 
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "texmgr.h"
#include "../ns-eel2/ns-eel.h"
#include "support.h"
#include "plugin.h"
#include "utility.h"
#include <wincodec.h>
#include <wrl/client.h>
#include <vector>

namespace
{
HRESULT CreateTgaBitmap(IWICImagingFactory* factory, const wchar_t* filename,
	Microsoft::WRL::ComPtr<IWICBitmap>& bitmap, UINT* width, UINT* height)
{
	if (!factory)
		return E_POINTER;
	HANDLE file = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return HRESULT_FROM_WIN32(GetLastError());

	LARGE_INTEGER fileSize;
	if (!GetFileSizeEx(file, &fileSize) || fileSize.QuadPart < 18 || fileSize.QuadPart > 0x40000000)
	{
		CloseHandle(file);
		return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
	}

	HRESULT result = S_OK;
	try
	{
		std::vector<BYTE> fileData(static_cast<size_t>(fileSize.QuadPart));
		size_t bytesRead = 0;
		while (bytesRead < fileData.size())
		{
			DWORD chunk = static_cast<DWORD>(min(fileData.size() - bytesRead, static_cast<size_t>(MAXDWORD)));
			DWORD read = 0;
			if (!ReadFile(file, fileData.data() + bytesRead, chunk, &read, NULL) || read == 0)
			{
				result = HRESULT_FROM_WIN32(GetLastError());
				break;
			}
			bytesRead += read;
		}
		CloseHandle(file);
		file = INVALID_HANDLE_VALUE;
		if (FAILED(result) && bytesRead != fileData.size())
			return result;

		const BYTE colorMapType = fileData[1];
		const BYTE imageType = fileData[2];
		const UINT imageWidth = fileData[12] | (static_cast<UINT>(fileData[13]) << 8);
		const UINT imageHeight = fileData[14] | (static_cast<UINT>(fileData[15]) << 8);
		const BYTE bitsPerPixel = fileData[16];
		const BYTE descriptor = fileData[17];
		const bool gray = imageType == 3 || imageType == 11;
		const bool rle = imageType == 10 || imageType == 11;
		if (colorMapType != 0 || !(imageType == 2 || imageType == 3 || imageType == 10 || imageType == 11) ||
			!imageWidth || !imageHeight || imageWidth > 16384 || imageHeight > 16384 ||
			(descriptor & 0xc0) != 0 || (gray ? bitsPerPixel != 8 : !(bitsPerPixel == 24 || bitsPerPixel == 32)))
			return WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT;

		const UINT sourcePixelBytes = bitsPerPixel / 8;
		const size_t pixelCount = static_cast<size_t>(imageWidth) * imageHeight;
		std::vector<BYTE> pixels(pixelCount * 4);
		size_t sourceOffset = 18 + fileData[0];
		if (sourceOffset > fileData.size())
			return WINCODEC_ERR_BADIMAGE;
		const bool topOrigin = (descriptor & 0x20) != 0;
		const bool rightOrigin = (descriptor & 0x10) != 0;
		size_t outputPixel = 0;
		while (outputPixel < pixelCount)
		{
			UINT runLength = 1;
			bool repeated = false;
			if (rle)
			{
				if (sourceOffset >= fileData.size())
					return WINCODEC_ERR_BADIMAGE;
				const BYTE packet = fileData[sourceOffset++];
				runLength = (packet & 0x7f) + 1;
				repeated = (packet & 0x80) != 0;
				if (runLength > pixelCount - outputPixel)
					return WINCODEC_ERR_BADIMAGE;
			}

			BYTE pixel[4] = { 0, 0, 0, 255 };
			for (UINT runPixel = 0; runPixel < runLength; ++runPixel)
			{
				if (repeated || !rle || runPixel == 0)
				{
					if (sourcePixelBytes > fileData.size() - sourceOffset)
						return WINCODEC_ERR_BADIMAGE;
					if (gray)
						pixel[0] = pixel[1] = pixel[2] = fileData[sourceOffset];
					else
					{
						pixel[0] = fileData[sourceOffset];
						pixel[1] = fileData[sourceOffset + 1];
						pixel[2] = fileData[sourceOffset + 2];
						if (sourcePixelBytes == 4) pixel[3] = fileData[sourceOffset + 3];
					}
					sourceOffset += sourcePixelBytes;
				}

				const size_t fileX = outputPixel % imageWidth;
				const size_t fileY = outputPixel / imageWidth;
				const size_t x = rightOrigin ? imageWidth - 1 - fileX : fileX;
				const size_t y = topOrigin ? fileY : imageHeight - 1 - fileY;
				memcpy(pixels.data() + (y * imageWidth + x) * 4, pixel, 4);
				++outputPixel;
			}
		}

		result = factory->CreateBitmapFromMemory(imageWidth, imageHeight,
			GUID_WICPixelFormat32bppBGRA, imageWidth * 4,
			static_cast<UINT>(pixels.size()), pixels.data(), &bitmap);
		if (SUCCEEDED(result))
		{
			if (width) *width = imageWidth;
			if (height) *height = imageHeight;
		}
	}
	catch (const std::bad_alloc&)
	{
		result = E_OUTOFMEMORY;
	}
	if (file != INVALID_HANDLE_VALUE)
		CloseHandle(file);
	return result;
}

#pragma pack(push, 1)
struct DdsPixelFormat
{
	DWORD size;
	DWORD flags;
	DWORD fourCC;
	DWORD rgbBitCount;
	DWORD redMask;
	DWORD greenMask;
	DWORD blueMask;
	DWORD alphaMask;
};

struct DdsHeader
{
	DWORD size;
	DWORD flags;
	DWORD height;
	DWORD width;
	DWORD pitchOrLinearSize;
	DWORD depth;
	DWORD mipMapCount;
	DWORD reserved[11];
	DdsPixelFormat pixelFormat;
	DWORD caps;
	DWORD caps2;
	DWORD caps3;
	DWORD caps4;
	DWORD reserved2;
};
#pragma pack(pop)

HRESULT LoadDdsTexture(IDirect3DDevice9* device, const wchar_t* filename, DWORD colorKey,
	IDirect3DTexture9** texture, UINT* width, UINT* height)
{
	HANDLE file = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return HRESULT_FROM_WIN32(GetLastError());

	LARGE_INTEGER fileSize;
	if (!GetFileSizeEx(file, &fileSize) || fileSize.QuadPart < 128 || fileSize.QuadPart > 0x40000000)
	{
		CloseHandle(file);
		return WINCODEC_ERR_BADIMAGE;
	}

	HRESULT result = E_FAIL;
	try
	{
		std::vector<BYTE> fileData(static_cast<size_t>(fileSize.QuadPart));
		size_t bytesRead = 0;
		while (bytesRead < fileData.size())
		{
			DWORD chunk = static_cast<DWORD>(min(fileData.size() - bytesRead, static_cast<size_t>(MAXDWORD)));
			DWORD read = 0;
			if (!ReadFile(file, fileData.data() + bytesRead, chunk, &read, NULL) || read == 0)
			{
				result = HRESULT_FROM_WIN32(GetLastError());
				break;
			}
			bytesRead += read;
		}
		CloseHandle(file);
		file = INVALID_HANDLE_VALUE;
		if (FAILED(result) && bytesRead != fileData.size())
			return result;

		if (memcmp(fileData.data(), "DDS ", 4) != 0)
			return WINCODEC_ERR_BADIMAGE;
		const DdsHeader* header = reinterpret_cast<const DdsHeader*>(fileData.data() + 4);
		if (header->size != 124 || header->pixelFormat.size != 32 || !header->width || !header->height ||
			header->width > 16384 || header->height > 16384)
			return WINCODEC_ERR_BADIMAGE;

		D3DFORMAT format = D3DFMT_UNKNOWN;
		UINT blockBytes = 0;
		bool compressed = false;
		if (header->pixelFormat.flags & 0x4)
		{
			switch (header->pixelFormat.fourCC)
			{
			case MAKEFOURCC('D','X','T','1'): format = D3DFMT_DXT1; blockBytes = 8; break;
			case MAKEFOURCC('D','X','T','2'): format = D3DFMT_DXT2; blockBytes = 16; break;
			case MAKEFOURCC('D','X','T','3'): format = D3DFMT_DXT3; blockBytes = 16; break;
			case MAKEFOURCC('D','X','T','4'): format = D3DFMT_DXT4; blockBytes = 16; break;
			case MAKEFOURCC('D','X','T','5'): format = D3DFMT_DXT5; blockBytes = 16; break;
			default: return WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT;
			}
			compressed = true;
		}
		else if ((header->pixelFormat.flags & 0x40) && header->pixelFormat.rgbBitCount == 32 &&
			header->pixelFormat.redMask == 0x00ff0000 && header->pixelFormat.greenMask == 0x0000ff00 &&
			header->pixelFormat.blueMask == 0x000000ff &&
			(header->pixelFormat.alphaMask == 0xff000000 || header->pixelFormat.alphaMask == 0))
		{
			format = (header->pixelFormat.alphaMask || colorKey != 0xffffffff) ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8;
			blockBytes = 4;
		}
		if (format == D3DFMT_UNKNOWN || (compressed && colorKey != 0xffffffff))
			return WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT;

		DWORD maxMipLevels = 1;
		for (UINT dimension = max(header->width, header->height); dimension > 1; dimension >>= 1)
			++maxMipLevels;
		const UINT mipLevels = (header->flags & 0x20000) ? header->mipMapCount : 1;
		if (!mipLevels || mipLevels > maxMipLevels)
			return WINCODEC_ERR_BADIMAGE;

		result = device->CreateTexture(header->width, header->height, mipLevels, 0,
			format, D3DPOOL_MANAGED, texture, NULL);
		if (FAILED(result))
			return result;

		size_t sourceOffset = 128;
		UINT mipWidth = header->width;
		UINT mipHeight = header->height;
		for (UINT level = 0; level < mipLevels; ++level)
		{
			const UINT rows = compressed ? max(1U, (mipHeight + 3) / 4) : mipHeight;
			const UINT rowBytes = compressed ? max(1U, (mipWidth + 3) / 4) * blockBytes : mipWidth * 4;
			const size_t levelBytes = static_cast<size_t>(rows) * rowBytes;
			if (levelBytes > fileData.size() - sourceOffset)
			{
				(*texture)->Release();
				*texture = NULL;
				return WINCODEC_ERR_BADIMAGE;
			}

			D3DLOCKED_RECT locked;
			result = (*texture)->LockRect(level, &locked, NULL, 0);
			if (FAILED(result))
			{
				(*texture)->Release();
				*texture = NULL;
				return result;
			}
			for (UINT row = 0; row < rows; ++row)
				memcpy(static_cast<BYTE*>(locked.pBits) + row * locked.Pitch,
					fileData.data() + sourceOffset + static_cast<size_t>(row) * rowBytes, rowBytes);

			if (!compressed && (!header->pixelFormat.alphaMask || colorKey != 0xffffffff))
			{
				const BYTE keyRed = static_cast<BYTE>((colorKey >> 16) & 0xff);
				const BYTE keyGreen = static_cast<BYTE>((colorKey >> 8) & 0xff);
				const BYTE keyBlue = static_cast<BYTE>(colorKey & 0xff);
				for (UINT y = 0; y < mipHeight; ++y)
				{
					BYTE* row = static_cast<BYTE*>(locked.pBits) + y * locked.Pitch;
					for (UINT x = 0; x < mipWidth; ++x)
					{
						if (colorKey != 0xffffffff && row[x * 4 + 2] == keyRed && row[x * 4 + 1] == keyGreen && row[x * 4] == keyBlue)
							row[x * 4 + 3] = 0;
						else if (!header->pixelFormat.alphaMask)
							row[x * 4 + 3] = 255;
					}
				}
			}
			(*texture)->UnlockRect(level);

			sourceOffset += levelBytes;
			mipWidth = max(1U, mipWidth / 2);
			mipHeight = max(1U, mipHeight / 2);
		}
		if (width) *width = header->width;
		if (height) *height = header->height;
		return S_OK;
	}
	catch (const std::bad_alloc&)
	{
		result = E_OUTOFMEMORY;
	}
	if (file != INVALID_HANDLE_VALUE)
		CloseHandle(file);
	return result;
}
}

HRESULT LoadTextureFromFileW(
	IDirect3DDevice9* device,
	const wchar_t* filename,
	DWORD colorKey,
	IDirect3DTexture9** texture,
	UINT* width,
	UINT* height)
{
	if (!device || !filename || !texture)
		return E_INVALIDARG;

	*texture = NULL;
	if (width) *width = 0;
	if (height) *height = 0;
	const wchar_t* extension = wcsrchr(filename, L'.');
	if (extension && _wcsicmp(extension, L".dds") == 0)
		return LoadDdsTexture(device, filename, colorKey, texture, width, height);

	HRESULT initResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	const bool uninitializeCom = SUCCEEDED(initResult);
	if (FAILED(initResult) && initResult != RPC_E_CHANGED_MODE)
		return initResult;

	HRESULT result = E_FAIL;
	Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
	Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
	Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
	Microsoft::WRL::ComPtr<IWICBitmap> tgaBitmap;
	Microsoft::WRL::ComPtr<IWICFormatConverter> converter;

	result = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&factory));
	if (SUCCEEDED(result))
		result = factory->CreateDecoderFromFilename(filename, NULL, GENERIC_READ,
			WICDecodeMetadataCacheOnLoad, &decoder);
	if (SUCCEEDED(result))
		result = decoder->GetFrame(0, &frame);
	IWICBitmapSource* bitmapSource = frame.Get();
	if (FAILED(result))
	{
		result = CreateTgaBitmap(factory.Get(), filename, tgaBitmap, NULL, NULL);
		bitmapSource = tgaBitmap.Get();
	}
	if (SUCCEEDED(result))
		result = factory->CreateFormatConverter(&converter);
	if (SUCCEEDED(result))
		result = converter->Initialize(bitmapSource, GUID_WICPixelFormat32bppBGRA,
			WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);

	UINT imageWidth = 0;
	UINT imageHeight = 0;
	if (SUCCEEDED(result))
		result = converter->GetSize(&imageWidth, &imageHeight);
	if (FAILED(result))
		goto cleanup;
	if (!imageWidth || !imageHeight || imageWidth > 16384 || imageHeight > 16384)
	{
		result = WINCODEC_ERR_BADIMAGE;
		goto cleanup;
	}

	try
	{
		std::vector<BYTE> pixels(static_cast<size_t>(imageWidth) * imageHeight * 4);
		result = converter->CopyPixels(NULL, imageWidth * 4, static_cast<UINT>(pixels.size()), pixels.data());
		if (FAILED(result))
			goto cleanup;

		if (colorKey != 0xffffffff)
		{
			const BYTE keyRed = static_cast<BYTE>((colorKey >> 16) & 0xff);
			const BYTE keyGreen = static_cast<BYTE>((colorKey >> 8) & 0xff);
			const BYTE keyBlue = static_cast<BYTE>(colorKey & 0xff);
			for (size_t offset = 0; offset < pixels.size(); offset += 4)
				if (pixels[offset + 2] == keyRed && pixels[offset + 1] == keyGreen && pixels[offset] == keyBlue)
					pixels[offset + 3] = 0;
		}

		UINT mipLevels = 1;
		for (UINT dimension = max(imageWidth, imageHeight); dimension > 1; dimension >>= 1)
			++mipLevels;

		result = device->CreateTexture(imageWidth, imageHeight, mipLevels, 0,
			D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, texture, NULL);
		if (FAILED(result))
			goto cleanup;

		UINT mipWidth = imageWidth;
		UINT mipHeight = imageHeight;
		for (UINT level = 0; level < mipLevels; ++level)
		{
			D3DLOCKED_RECT locked;
			result = (*texture)->LockRect(level, &locked, NULL, 0);
			if (FAILED(result))
			{
				(*texture)->Release();
				*texture = NULL;
				goto cleanup;
			}

			for (UINT y = 0; y < mipHeight; ++y)
				memcpy(static_cast<BYTE*>(locked.pBits) + y * locked.Pitch,
					pixels.data() + static_cast<size_t>(y) * mipWidth * 4, mipWidth * 4);
			(*texture)->UnlockRect(level);

			if (level + 1 < mipLevels)
			{
				const UINT nextWidth = max(1U, mipWidth / 2);
				const UINT nextHeight = max(1U, mipHeight / 2);
				std::vector<BYTE> nextPixels(static_cast<size_t>(nextWidth) * nextHeight * 4);
				for (UINT y = 0; y < nextHeight; ++y)
					for (UINT x = 0; x < nextWidth; ++x)
						for (UINT channel = 0; channel < 4; ++channel)
						{
							const UINT x0 = min(x * 2, mipWidth - 1);
							const UINT x1 = min(x * 2 + 1, mipWidth - 1);
							const UINT y0 = min(y * 2, mipHeight - 1);
							const UINT y1 = min(y * 2 + 1, mipHeight - 1);
							const size_t i00 = (static_cast<size_t>(y0) * mipWidth + x0) * 4 + channel;
							const size_t i10 = (static_cast<size_t>(y0) * mipWidth + x1) * 4 + channel;
							const size_t i01 = (static_cast<size_t>(y1) * mipWidth + x0) * 4 + channel;
							const size_t i11 = (static_cast<size_t>(y1) * mipWidth + x1) * 4 + channel;
							const size_t output = (static_cast<size_t>(y) * nextWidth + x) * 4 + channel;
							nextPixels[output] = static_cast<BYTE>((pixels[i00] + pixels[i10] + pixels[i01] + pixels[i11]) / 4);
						}
				pixels.swap(nextPixels);
				mipWidth = nextWidth;
				mipHeight = nextHeight;
			}
		}

		if (width) *width = imageWidth;
		if (height) *height = imageHeight;
		result = S_OK;
	}
	catch (const std::bad_alloc&)
	{
		result = E_OUTOFMEMORY;
		if (*texture)
		{
			(*texture)->Release();
			*texture = NULL;
		}
	}

cleanup:
	converter.Reset();
	frame.Reset();
	tgaBitmap.Reset();
	decoder.Reset();
	factory.Reset();
	if (uninitializeCom)
		CoUninitialize();
	return result;
}

texmgr::texmgr()
{
}

texmgr::~texmgr()
{
	// DO NOT RELEASE OR DELETE m_lpDD; CLIENT SHOULD DO THIS!
}

void texmgr::Finish()
{
	for (int i=0; i<NUM_TEX; i++)
	{
		KillTex(i);
		NSEEL_VM_free(m_tex[i].tex_eel_ctx);
	}

	// DO NOT RELEASE OR DELETE m_lpDD; CLIENT SHOULD DO THIS!
}

void texmgr::Init(LPDIRECT3DDEVICE9 lpDD)
{
	m_lpDD = lpDD;

	for (int i=0; i<NUM_TEX; i++)
	{
		m_tex[i].pSurface = NULL;
		m_tex[i].szFileName[0] = 0;
		m_tex[i].m_codehandle = NULL;
		m_tex[i].m_szExpr[0] = 0;
		m_tex[i].tex_eel_ctx = NSEEL_VM_alloc();
	}
}

int texmgr::LoadTex(wchar_t *szFilename, int iSlot, char *szInitCode, char *szCode, float time, int frame, unsigned int ck)
{
	if (iSlot < 0) return TEXMGR_ERR_BAD_INDEX;
	if (iSlot >= NUM_TEX) return TEXMGR_ERR_BAD_INDEX;
	
	// first, if this texture is already loaded, just add another instance.
	bool bTextureInstanced = false;
	{
		for (int x=0; x<NUM_TEX; x++)
			if (m_tex[x].pSurface && _wcsicmp(m_tex[x].szFileName, szFilename)==0)
			{
				memcpy(&m_tex[iSlot], &m_tex[x], sizeof(td_tex));
				m_tex[iSlot].m_szExpr[0] = 0;
				m_tex[iSlot].m_codehandle  = 0;

				bTextureInstanced = true;
				break;
			}
	}

	if (!bTextureInstanced)
	{
		// Free old resources:
		KillTex(iSlot);

		wcscpy(m_tex[iSlot].szFileName, szFilename);

				UINT width = 0;
				UINT height = 0;
				HRESULT hr = LoadTextureFromFileW(m_lpDD, szFilename, 0xFF000000 | ck,
						&m_tex[iSlot].pSurface, &width, &height);
        
        if (hr != D3D_OK)
        {
            switch(hr)
            {
            case E_OUTOFMEMORY:
            case D3DERR_OUTOFVIDEOMEMORY:
                return TEXMGR_ERR_OUTOFMEM;
            default:
			    return TEXMGR_ERR_BADFILE;
            }
        }

        m_tex[iSlot].img_w = width;
		m_tex[iSlot].img_h = height;
	}
	
	m_tex[iSlot].fStartTime = time;
	m_tex[iSlot].nStartFrame = frame;
	
	int ret = TEXMGR_ERR_SUCCESS;

	// compile & run init. code:	
	if (!RunInitCode(iSlot, szInitCode))
		ret |= TEXMGR_WARN_ERROR_IN_INIT_CODE;
	
	// compile & save per-frame code:
	strcpy(m_tex[iSlot].m_szExpr, szCode);
	FreeCode(iSlot);
	if (!RecompileExpressions(iSlot))
		ret |= TEXMGR_WARN_ERROR_IN_REG_CODE;
	
	//g_dumpmsg("texmgr: success");
    
	return ret;
}

void texmgr::KillTex(int iSlot)
{
	if (iSlot < 0) return;
	if (iSlot >= NUM_TEX) return;
	
	// Free old resources:
	if (m_tex[iSlot].pSurface)
	{
		// first, make sure no other sprites reference this texture!
		int refcount = 0;
		for (int x=0; x<NUM_TEX; x++)
			if (m_tex[x].pSurface == m_tex[iSlot].pSurface)
				refcount++;

		if (refcount==1)
			m_tex[iSlot].pSurface->Release();
		m_tex[iSlot].pSurface = NULL;
	}
	m_tex[iSlot].szFileName[0] = 0;

	FreeCode(iSlot);
}

void texmgr::StripLinefeedCharsAndComments(char *src, char *dest)
{
	// replaces all LINEFEED_CONTROL_CHAR characters in src with a space in dest;
	// also strips out all comments (beginning with '//' and going til end of line).
	// Restriction: sizeof(dest) must be >= sizeof(src).

	int i2 = 0;
	int len = strlen(src);
	int bComment = false;
	for (int i=0; i<len; i++)		
	{
		if (bComment)
		{
			if (src[i] == LINEFEED_CONTROL_CHAR)	
				bComment = false;
		}
		else
		{
			if ((src[i] =='\\' && src[i+1] =='\\') || (src[i] =='/' && src[i+1] =='/'))
				bComment = true;
			else if (src[i] != LINEFEED_CONTROL_CHAR)
				dest[i2++] = src[i];
		}
	}
	dest[i2] = 0;
}

bool texmgr::RunInitCode(int iSlot, char *szInitCode)
{
	// warning: destroys contents of m_tex[iSlot].m_szExpr,
	//   so be sure to call RunInitCode before writing or
	//   compiling that string!

	FreeCode(iSlot);
	FreeVars(iSlot);
	RegisterBuiltInVariables(iSlot);

	strcpy(m_tex[iSlot].m_szExpr, szInitCode);
	bool ret = RecompileExpressions(iSlot);

	// set default values of output variables:
	// (by not setting these every frame, we allow the values to persist from frame-to-frame.)
	*(m_tex[iSlot].var_x)        = 0.5;
	*(m_tex[iSlot].var_y)        = 0.5;
	*(m_tex[iSlot].var_sx)       = 1.0;
	*(m_tex[iSlot].var_sy)       = 1.0;
	*(m_tex[iSlot].var_repeatx)  = 1.0;
	*(m_tex[iSlot].var_repeaty)  = 1.0;
	*(m_tex[iSlot].var_rot)      = 0.0;
	*(m_tex[iSlot].var_flipx)    = 0.0;
	*(m_tex[iSlot].var_flipy)    = 0.0;
	*(m_tex[iSlot].var_r)        = 1.0;
	*(m_tex[iSlot].var_g)        = 1.0;
	*(m_tex[iSlot].var_b)        = 1.0;
	*(m_tex[iSlot].var_a)        = 1.0;
	*(m_tex[iSlot].var_blendmode)= 0.0;
	*(m_tex[iSlot].var_done)     = 0.0;
	*(m_tex[iSlot].var_burn)     = 1.0;

	#ifndef _NO_EXPR_
		if (m_tex[iSlot].m_codehandle)
			NSEEL_code_execute(m_tex[iSlot].m_codehandle);
	#endif

	return ret;
}

bool texmgr::RecompileExpressions(int iSlot)
{
	char *expr = m_tex[iSlot].m_szExpr;

	// QUICK FIX: if the string ONLY has spaces and linefeeds, erase it, 
	// because for some strange reason this would cause an error in compileCode().
	{
		char *p = expr;
		while (*p==' ' || *p==LINEFEED_CONTROL_CHAR) p++;
		if (*p == 0) expr[0] = 0;
	}

	// replace linefeed control characters with spaces, so they don't mess up the code compiler,
	// and strip out any comments ('//') before sending to CompileCode().
	char buf[sizeof(m_tex[iSlot].m_szExpr)];
	StripLinefeedCharsAndComments(expr, buf);

	return true;
}

void texmgr::FreeVars(int iSlot)
{
	// free the built-in variables AND any user variables
}

void texmgr::FreeCode(int iSlot)
{
	// free the compiled expressions
	if (m_tex[iSlot].m_codehandle)
	{
		NSEEL_code_free(m_tex[iSlot].m_codehandle);
		m_tex[iSlot].m_codehandle = NULL;
	}
}

void texmgr::RegisterBuiltInVariables(int iSlot)
{
	NSEEL_VMCTX eel_ctx = m_tex[iSlot].tex_eel_ctx;
	NSEEL_VM_resetvars(eel_ctx);
	
	// input variables
    m_tex[iSlot].var_time        = NSEEL_VM_regvar(eel_ctx, "time");     
	m_tex[iSlot].var_frame       = NSEEL_VM_regvar(eel_ctx, "frame");    
	m_tex[iSlot].var_fps         = NSEEL_VM_regvar(eel_ctx, "fps");      
	m_tex[iSlot].var_progress    = NSEEL_VM_regvar(eel_ctx, "progress"); 
	m_tex[iSlot].var_bass        = NSEEL_VM_regvar(eel_ctx, "bass");     
	m_tex[iSlot].var_bass_att    = NSEEL_VM_regvar(eel_ctx, "bass_att"); 
	m_tex[iSlot].var_mid         = NSEEL_VM_regvar(eel_ctx, "mid");      
	m_tex[iSlot].var_mid_att     = NSEEL_VM_regvar(eel_ctx, "mid_att");  
	m_tex[iSlot].var_treb        = NSEEL_VM_regvar(eel_ctx, "treb");     
	m_tex[iSlot].var_treb_att    = NSEEL_VM_regvar(eel_ctx, "treb_att"); 
	
	// output variables
	m_tex[iSlot].var_x           = NSEEL_VM_regvar(eel_ctx, "x");        
	m_tex[iSlot].var_y           = NSEEL_VM_regvar(eel_ctx, "y");        
	m_tex[iSlot].var_sx          = NSEEL_VM_regvar(eel_ctx, "sx");       
	m_tex[iSlot].var_sy          = NSEEL_VM_regvar(eel_ctx, "sy");       
	m_tex[iSlot].var_repeatx     = NSEEL_VM_regvar(eel_ctx, "repeatx");       
	m_tex[iSlot].var_repeaty     = NSEEL_VM_regvar(eel_ctx, "repeaty");       
	m_tex[iSlot].var_rot         = NSEEL_VM_regvar(eel_ctx, "rot");      
	m_tex[iSlot].var_flipx       = NSEEL_VM_regvar(eel_ctx, "flipx");    
	m_tex[iSlot].var_flipy       = NSEEL_VM_regvar(eel_ctx, "flipy");    
	m_tex[iSlot].var_r           = NSEEL_VM_regvar(eel_ctx, "r");        
	m_tex[iSlot].var_g           = NSEEL_VM_regvar(eel_ctx, "g");        
	m_tex[iSlot].var_b           = NSEEL_VM_regvar(eel_ctx, "b");        
	m_tex[iSlot].var_a           = NSEEL_VM_regvar(eel_ctx, "a");        
	m_tex[iSlot].var_blendmode   = NSEEL_VM_regvar(eel_ctx, "blendmode");
	m_tex[iSlot].var_done        = NSEEL_VM_regvar(eel_ctx, "done"); 
	m_tex[iSlot].var_burn        = NSEEL_VM_regvar(eel_ctx, "burn"); 

//	resetVars(NULL);
}