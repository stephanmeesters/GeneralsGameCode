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

#include "WW3D2/camera.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/scene.h"
#include "WW3D2/surfaceclass.h"
#include "WW3D2/texture.h"
#include "WW3D2/ww3d.h"
#include "WWMath/wwmath.h"

IDirect3DDevice8 *RenderingTest::s_device = nullptr;
CameraClass *RenderingTest::s_camera = nullptr;
HWND RenderingTest::s_window = nullptr;
IDirect3DSurface8 *RenderingTest::s_readback = nullptr;
DWORD RenderingTest::s_defaultState = 0;

// Runs once before the first test of the suite. When it fails, Google Test skips the tests of the suite.
// WW3D starts up as W3DDisplay::init does, with a windowed device whose back buffer the tests render into.
void RenderingTest::SetUpTestSuite()
{
	s_window = CreateWindowA("STATIC", "RenderingTest", WS_OVERLAPPEDWINDOW, 0, 0, Width, Height, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
	ASSERT_NE(s_window, nullptr);
	WWMath::Init();
	ASSERT_EQ(WW3D::Init(s_window), WW3D_ERROR_OK);
	WW3D::Set_Screen_UV_Bias(TRUE);
	// Thumbnails need an asset manager, which the tests do without.
	WW3D::Set_Thumbnail_Enabled(false);
	ASSERT_EQ(WW3D::Set_Render_Device(0, Width, Height, 32, TRUE), WW3D_ERROR_OK);
	s_device = DX8Wrapper::_Get_D3D_Device8();
	// CopyRects needs the format of the back buffer, which WW3D picks from the desktop: X8R8G8B8 or A8R8G8B8.
	IDirect3DSurface8 *backBuffer = nullptr;
	ASSERT_HRESULT_SUCCEEDED(s_device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backBuffer));
	D3DSURFACE_DESC desc = {};
	backBuffer->GetDesc(&desc);
	backBuffer->Release();
	ASSERT_HRESULT_SUCCEEDED(s_device->CreateImageSurface(Width, Height, desc.Format, &s_readback));
	// No test has run yet, so this captures the state that WW3D leaves the device in.
	ASSERT_HRESULT_SUCCEEDED(s_device->CreateStateBlock(D3DSBT_ALL, &s_defaultState));

	s_camera = NEW_REF(CameraClass, ());
	Matrix3D transform;
	transform.Look_At(Vector3(0.0f, -12.0f, 6.0f), Vector3(0.0f, 0.0f, 0.0f), 0.0f);
	s_camera->Set_Transform(transform);
	s_camera->Set_View_Plane(DEG_TO_RADF(50.0f), DEG_TO_RADF(50.0f));
	s_camera->Set_Clip_Planes(1.0f, 100.0f);
}

void RenderingTest::TearDownTestSuite()
{
	REF_PTR_RELEASE(s_camera);
	if (s_defaultState != 0)
	{
		s_device->DeleteStateBlock(s_defaultState);
		s_defaultState = 0;
	}
	if (s_readback != nullptr)
	{
		s_readback->Release();
		s_readback = nullptr;
	}
	s_device = nullptr;
	WW3D::Shutdown();
	WWMath::Shutdown();
	if (s_window != nullptr)
	{
		DestroyWindow(s_window);
		s_window = nullptr;
	}
}

// Undoes the state that the previous test changed on the device directly, before WW3D clears the back buffer.
void RenderingTest::SetUp()
{
	ASSERT_HRESULT_SUCCEEDED(s_device->ApplyStateBlock(s_defaultState));
	ASSERT_EQ(WW3D::Begin_Render(true, true, Vector3(0.0f, 0.0f, 0.0f)), WW3D_ERROR_OK);
	m_rendering = true;
}

void RenderingTest::TearDown()
{
	if (m_rendering)
	{
		WW3D::End_Render(false);
	}
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
	EXPECT_HRESULT_SUCCEEDED(s_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, transformed.size() / 3, transformed.data(), sizeof(TransformedVertex)));
}

void RenderingTest::render(std::initializer_list<RenderObjClass *> objects)
{
	SimpleSceneClass *scene = NEW_REF(SimpleSceneClass, ());
	scene->Set_Ambient_Light(Vector3(1.0f, 1.0f, 1.0f));
	for (RenderObjClass *object : objects)
	{
		scene->Add_Render_Object(object);
	}
	EXPECT_EQ(WW3D::Render(scene, s_camera), WW3D_ERROR_OK);
	REF_PTR_RELEASE(scene);
}

TextureClass *RenderingTest::createTexture(UINT width, UINT height, const std::function<DWORD(UINT x, UINT y)> &color)
{
	SurfaceClass *surface = NEW_REF(SurfaceClass, (width, height, WW3D_FORMAT_A8R8G8B8));
	int pitch = 0;
	BYTE *bits = static_cast<BYTE *>(surface->Lock(&pitch));
	for (UINT y = 0; y < height; ++y)
	{
		for (UINT x = 0; x < width; ++x)
		{
			reinterpret_cast<DWORD *>(bits + y * pitch)[x] = color(x, y);
		}
	}
	surface->Unlock();
	TextureClass *texture = NEW_REF(TextureClass, (surface, MIP_LEVELS_1));
	REF_PTR_RELEASE(surface);
	return texture;
}

void RenderingTest::capture(const char *imageName)
{
	ASSERT_TRUE(m_rendering);
	WW3D::End_Render(false);
	m_rendering = false;
	IDirect3DSurface8 *backBuffer = nullptr;
	ASSERT_HRESULT_SUCCEEDED(s_device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backBuffer));
	const HRESULT copied = s_device->CopyRects(backBuffer, nullptr, 0, s_readback, nullptr);
	backBuffer->Release();
	ASSERT_HRESULT_SUCCEEDED(copied);
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
