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

#include "RenderingTestFixture.h"
#include <stb_image_write.h>

IDirect3DDevice8 *RenderingTest::s_device = nullptr;
HWND RenderingTest::s_window = nullptr;
IDirect3D8 *RenderingTest::s_d3d = nullptr;
IDirect3DSurface8 *RenderingTest::s_target = nullptr;
IDirect3DSurface8 *RenderingTest::s_readback = nullptr;
DWORD RenderingTest::s_defaultState = 0;

namespace
{
template <typename T>
void release(T *&object)
{
	if (object != nullptr)
	{
		object->Release();
		object = nullptr;
	}
}
} // namespace

// Runs once before the first test of the suite. When it fails, Google Test skips the tests of the suite.
void RenderingTest::SetUpTestSuite()
{
	s_window = CreateWindowA("STATIC", "RenderingTest", WS_OVERLAPPEDWINDOW, 0, 0, Width, Height, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
	ASSERT_NE(s_window, nullptr);
	s_d3d = Direct3DCreate8(D3D_SDK_VERSION);
	ASSERT_NE(s_d3d, nullptr);

	D3DPRESENT_PARAMETERS params = {};
	params.BackBufferWidth = Width;
	params.BackBufferHeight = Height;
	params.BackBufferFormat = D3DFMT_X8R8G8B8;
	params.BackBufferCount = 1;
	params.SwapEffect = D3DSWAPEFFECT_DISCARD;
	params.hDeviceWindow = s_window;
	params.Windowed = TRUE;
	ASSERT_HRESULT_SUCCEEDED(s_d3d->CreateDevice(0, D3DDEVTYPE_HAL, s_window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &params, &s_device));
	ASSERT_HRESULT_SUCCEEDED(s_device->CreateRenderTarget(Width, Height, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, FALSE, &s_target));
	ASSERT_HRESULT_SUCCEEDED(s_device->SetRenderTarget(s_target, nullptr));
	ASSERT_HRESULT_SUCCEEDED(s_device->CreateImageSurface(Width, Height, D3DFMT_X8R8G8B8, &s_readback));
	// No test has run yet, so this captures the default state of the device.
	ASSERT_HRESULT_SUCCEEDED(s_device->CreateStateBlock(D3DSBT_ALL, &s_defaultState));
}

void RenderingTest::TearDownTestSuite()
{
	if (s_defaultState != 0)
	{
		s_device->DeleteStateBlock(s_defaultState);
		s_defaultState = 0;
	}
	release(s_readback);
	release(s_target);
	release(s_device);
	release(s_d3d);
	if (s_window != nullptr)
	{
		DestroyWindow(s_window);
		s_window = nullptr;
	}
}

// Undoes what the previous test changed. The state comes first, because it sets the viewport that Clear fills.
void RenderingTest::SetUp()
{
	ASSERT_HRESULT_SUCCEEDED(s_device->SetRenderTarget(s_target, nullptr));
	ASSERT_HRESULT_SUCCEEDED(s_device->ApplyStateBlock(s_defaultState));
	clear(0x000000);
}

void RenderingTest::clear(DWORD color)
{
	EXPECT_HRESULT_SUCCEEDED(s_device->Clear(0, nullptr, D3DCLEAR_TARGET, color, 1.0f, 0));
}

void RenderingTest::drawTriangles(std::initializer_list<Vertex> vertices)
{
	struct TransformedVertex
	{
		float x, y, z, rhw;
		DWORD color;
	};
	std::vector<TransformedVertex> transformed;
	for (const Vertex &vertex : vertices)
	{
		transformed.push_back({vertex.x, vertex.y, 0.0f, 1.0f, vertex.color});
	}
	ASSERT_HRESULT_SUCCEEDED(s_device->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE));
	ASSERT_HRESULT_SUCCEEDED(s_device->BeginScene());
	EXPECT_HRESULT_SUCCEEDED(s_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, transformed.size() / 3, transformed.data(), sizeof(TransformedVertex)));
	ASSERT_HRESULT_SUCCEEDED(s_device->EndScene());
}

void RenderingTest::capture(const char *imageName)
{
	ASSERT_HRESULT_SUCCEEDED(s_device->CopyRects(s_target, nullptr, 0, s_readback, nullptr));
	D3DLOCKED_RECT locked = {};
	ASSERT_HRESULT_SUCCEEDED(s_readback->LockRect(&locked, nullptr, D3DLOCK_READONLY));
	std::vector<unsigned char> rgb;
	rgb.reserve(Width * Height * 3);
	for (UINT y = 0; y < Height; ++y)
	{
		const DWORD *row = reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(locked.pBits) + y * locked.Pitch);
		for (UINT x = 0; x < Width; ++x)
		{
			const DWORD color = row[x] & 0x00ffffff;
			m_pixels[y * Width + x] = color;
			rgb.push_back(static_cast<unsigned char>(color >> 16));
			rgb.push_back(static_cast<unsigned char>(color >> 8));
			rgb.push_back(static_cast<unsigned char>(color));
		}
	}
	ASSERT_HRESULT_SUCCEEDED(s_readback->UnlockRect());
	EXPECT_NE(stbi_write_png(imageName, Width, Height, 3, rgb.data(), Width * 3), 0) << imageName;
}
