/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <gtest/gtest.h>
#include <windows.h>
#include <d3d8.h>
#include <algorithm>
#include <cstdio>
#include <vector>

// Each test renders with a real D3D8 device into an offscreen render target. CTest skips these tests;
// CI runs them under Wine, DXVK and Mesa lavapipe and compares the saved bitmaps with the base commit.
class RenderingTest : public ::testing::Test
{
protected:
	static constexpr UINT Width = 256;
	static constexpr UINT Height = 256;

	virtual void SetUp() override
	{
		m_window = CreateWindowA("STATIC", "RenderingTest", WS_OVERLAPPEDWINDOW, 0, 0, Width, Height, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
		ASSERT_NE(m_window, nullptr);
		m_d3d = Direct3DCreate8(D3D_SDK_VERSION);
		ASSERT_NE(m_d3d, nullptr);

		D3DPRESENT_PARAMETERS params = {};
		params.BackBufferWidth = Width;
		params.BackBufferHeight = Height;
		params.BackBufferFormat = D3DFMT_X8R8G8B8;
		params.BackBufferCount = 1;
		params.SwapEffect = D3DSWAPEFFECT_DISCARD;
		params.hDeviceWindow = m_window;
		params.Windowed = TRUE;
		ASSERT_HRESULT_SUCCEEDED(m_d3d->CreateDevice(0, D3DDEVTYPE_HAL, m_window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &params, &m_device));
		ASSERT_HRESULT_SUCCEEDED(m_device->CreateRenderTarget(Width, Height, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, FALSE, &m_target));
		ASSERT_HRESULT_SUCCEEDED(m_device->SetRenderTarget(m_target, nullptr));
		ASSERT_HRESULT_SUCCEEDED(m_device->CreateImageSurface(Width, Height, D3DFMT_X8R8G8B8, &m_readback));
	}

	virtual void TearDown() override
	{
		release(m_readback);
		release(m_target);
		release(m_device);
		release(m_d3d);
		if (m_window != nullptr)
		{
			DestroyWindow(m_window);
		}
	}

	// Copies the render target into pixels as 0xRRGGBB values and saves it as a bitmap for the CI report.
	void readBack(const char *bitmapName, std::vector<DWORD> &pixels)
	{
		ASSERT_HRESULT_SUCCEEDED(m_device->CopyRects(m_target, nullptr, 0, m_readback, nullptr));
		D3DLOCKED_RECT locked = {};
		ASSERT_HRESULT_SUCCEEDED(m_readback->LockRect(&locked, nullptr, D3DLOCK_READONLY));
		pixels.resize(Width * Height);
		for (UINT y = 0; y < Height; ++y)
		{
			const DWORD *row = reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(locked.pBits) + y * locked.Pitch);
			for (UINT x = 0; x < Width; ++x)
			{
				pixels[y * Width + x] = row[x] & 0x00ffffff;
			}
		}
		ASSERT_HRESULT_SUCCEEDED(m_readback->UnlockRect());
		saveBitmap(bitmapName, pixels);
	}

	IDirect3DDevice8 *m_device = nullptr;

private:
	template <typename T>
	static void release(T *object)
	{
		if (object != nullptr)
		{
			object->Release();
		}
	}

	// Writes a top-down 32 bit BMP.
	static void saveBitmap(const char *name, const std::vector<DWORD> &pixels)
	{
		BITMAPFILEHEADER header = {};
		BITMAPINFOHEADER info = {};
		header.bfType = 0x4d42;
		header.bfOffBits = sizeof(header) + sizeof(info);
		header.bfSize = header.bfOffBits + Width * Height * 4;
		info.biSize = sizeof(info);
		info.biWidth = Width;
		info.biHeight = -static_cast<LONG>(Height);
		info.biPlanes = 1;
		info.biBitCount = 32;
		info.biCompression = BI_RGB;

		FILE *file = std::fopen(name, "wb");
		ASSERT_NE(file, nullptr) << name;
		bool written = std::fwrite(&header, sizeof(header), 1, file) == 1;
		written = std::fwrite(&info, sizeof(info), 1, file) == 1 && written;
		written = std::fwrite(pixels.data(), pixels.size() * sizeof(DWORD), 1, file) == 1 && written;
		EXPECT_EQ(std::fclose(file), 0) << name;
		EXPECT_TRUE(written) << name;
	}

	HWND m_window = nullptr;
	IDirect3D8 *m_d3d = nullptr;
	IDirect3DSurface8 *m_target = nullptr;
	IDirect3DSurface8 *m_readback = nullptr;
};

// Clearing the render target fills every pixel with the clear color.
TEST_F(RenderingTest, Clear)
{
	const DWORD color = 0x102030;
	ASSERT_HRESULT_SUCCEEDED(m_device->Clear(0, nullptr, D3DCLEAR_TARGET, color, 1.0f, 0));

	std::vector<DWORD> pixels;
	readBack("clear.bmp", pixels);
	ASSERT_EQ(pixels.size(), Width * Height);
	EXPECT_EQ(std::count(pixels.begin(), pixels.end(), color), static_cast<std::ptrdiff_t>(pixels.size()));
}
