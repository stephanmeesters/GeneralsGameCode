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

#include "WW3D2/Renderer.h"
#include "WW3D2/camera.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/scene.h"
#include "WW3D2/surfaceclass.h"
#include "WW3D2/texture.h"
#include "WW3D2/ww3d.h"
#include "WWMath/wwmath.h"

CameraClass *RenderingTest::s_camera = nullptr;
HWND RenderingTest::s_window = nullptr;

// Runs once before the first test of the suite. When it fails, Google Test skips the tests of the suite.
// WW3D starts up as W3DDisplay::init does, with a windowed device whose back buffer the tests render into.
void RenderingTest::SetUpTestSuite()
{
	// Without a border, the window is as large as the back buffer, which backends that size their back buffer from
	// the window need.
	s_window = CreateWindowA("STATIC", "RenderingTest", WS_POPUP, 0, 0, Width, Height, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
	ASSERT_NE(s_window, nullptr);
	WWMath::Init();
	ASSERT_EQ(WW3D::Init(s_window), WW3D_ERROR_OK);
	WW3D::Set_Screen_UV_Bias(TRUE);
	// Thumbnails need an asset manager, which the tests do without.
	WW3D::Set_Thumbnail_Enabled(false);
	ASSERT_EQ(WW3D::Set_Render_Device(0, Width, Height, 32, TRUE), WW3D_ERROR_OK);

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
	WW3D::Shutdown();
	WWMath::Shutdown();
	if (s_window != nullptr)
	{
		DestroyWindow(s_window);
		s_window = nullptr;
	}
}

void RenderingTest::SetUp()
{
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
	const Vector3 rgb((color >> 16 & 0xFF) / 255.0f, (color >> 8 & 0xFF) / 255.0f, (color & 0xFF) / 255.0f);
	Renderer::Clear(true, false, rgb);
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

// The one DX8 specific part. Renderer has no way to read the back buffer yet.
void RenderingTest::capture(const char *imageName)
{
	ASSERT_TRUE(m_rendering);
	// The back buffer keeps the frame until it is presented.
	WW3D::End_Render(false);
	m_rendering = false;
	SurfaceClass *backBuffer = DX8Wrapper::_Get_DX8_Back_Buffer();
	ASSERT_NE(backBuffer, nullptr);
	// CopyRects needs the format of the back buffer, which WW3D picks from the desktop: X8R8G8B8 or A8R8G8B8.
	SurfaceClass *copy = NEW_REF(SurfaceClass, (Width, Height, Renderer::Get_Back_Buffer_Format()));
	DX8Wrapper::_Copy_DX8_Rects(backBuffer->Peek_D3D_Surface(), nullptr, 0, copy->Peek_D3D_Surface(), nullptr);
	REF_PTR_RELEASE(backBuffer);
	int pitch = 0;
	const BYTE *bits = static_cast<const BYTE *>(copy->Lock(&pitch));
	std::vector<unsigned char> rgb;
	rgb.reserve(Width * Height * 3);
	for (UINT y = 0; y < Height; ++y)
	{
		const DWORD *row = reinterpret_cast<const DWORD *>(bits + y * pitch);
		for (UINT x = 0; x < Width; ++x)
		{
			const DWORD color = row[x] & 0x00ffffff;
			m_pixels[y * Width + x] = color;
			rgb.push_back(static_cast<unsigned char>(color >> 16));
			rgb.push_back(static_cast<unsigned char>(color >> 8));
			rgb.push_back(static_cast<unsigned char>(color));
		}
	}
	copy->Unlock();
	REF_PTR_RELEASE(copy);
	EXPECT_NE(stbi_write_png(imageName, Width, Height, 3, rgb.data(), Width * 3), 0) << imageName;
}
