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
#include <string>
#include <stb_image_write.h>
// Reads the BMP that bgfx saves. The game links no stb_image, so it is compiled here.
#define STB_IMAGE_STATIC
#define STBI_ONLY_BMP
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#ifdef GGC_RENDER_BACKEND_BGFX
#include <bgfx/bgfx.h>
#endif

#include "WW3D2/IRenderBackend.h"
#include "WW3D2/camera.h"
#include "WW3D2/render2d.h"
#include "WW3D2/scene.h"
#include "WW3D2/surfaceclass.h"
#include "WW3D2/texture.h"
#include "WW3D2/ww3d.h"
#include "WWLib/GgcRuntimeFlags.h"
#include "WWMath/wwmath.h"

CameraClass *RenderingTest::s_camera = nullptr;
HWND RenderingTest::s_window = nullptr;

// Runs once before the first test of the suite. When it fails, Google Test skips the tests of the suite.
// WW3D starts up as W3DDisplay::init does, with a windowed device whose back buffer the tests render into.
void RenderingTest::SetUpTestSuite()
{
	// Without a border, the window is as large as the back buffer that bgfx takes from it.
	s_window = CreateWindowA("STATIC", "RenderingTest", WS_POPUP, 0, 0, Width, Height, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
	ASSERT_NE(s_window, nullptr);
	// bgfx then renders each frame when it ends instead of on a thread of its own, so capture() gets the frame.
	GgcFlags::SetOverride(GgcFlag_BgfxNoRenderThread, "1");
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
	WW3D::Get_Render_Backend()->Clear(true, false, rgb);
}

// Render2DClass draws in screen space through the backend, and its colors can differ per vertex.
void RenderingTest::drawTriangles(std::initializer_list<Vertex> vertices)
{
	Render2DClass renderer;
	renderer.Set_Coordinate_Range(RectClass(0.0f, 0.0f, Width, Height));
	renderer.Enable_Texturing(false);
	// Draws the triangles whichever way round their vertices go.
	renderer.Get_Shader()->Set_Cull_Mode(ShaderClass::CULL_MODE_DISABLE);
	const Vector2 uv(0.0f, 0.0f);
	for (const Vertex *vertex = vertices.begin(); vertex + 2 < vertices.end(); vertex += 3)
	{
		renderer.Add_Tri(Vector2(vertex[0].x, vertex[0].y), Vector2(vertex[1].x, vertex[1].y), Vector2(vertex[2].x, vertex[2].y), uv, uv, uv);
	}
	DynamicVectorClass<unsigned long> &colors = renderer.Get_Color_Array();
	int index = 0;
	for (const Vertex &vertex : vertices)
	{
		colors[index++] = 0xFF000000 | vertex.color;
	}
	renderer.Render();
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
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	std::vector<unsigned char> rgb;
	rgb.reserve(Width * Height * 3);
	if (backend->Supports_Native_Screen_Shot())
	{
		// bgfx reads the back buffer back while it renders the frame, which ends the frame, and saves it as a BMP.
		// On Direct3D 12 it reads the back buffer of the frame before, so it saves the image during the next frame.
		const std::string bmp = std::string(imageName) + ".bmp";
		bool nextFrame = false;
#ifdef GGC_RENDER_BACKEND_BGFX
		nextFrame = bgfx::getRendererType() == bgfx::RendererType::Direct3D12;
#endif
		if (nextFrame)
		{
			WW3D::End_Render(false);
			m_rendering = false;
			ASSERT_EQ(WW3D::Begin_Render(false, false, Vector3(0.0f, 0.0f, 0.0f)), WW3D_ERROR_OK);
			m_rendering = true;
		}
		ASSERT_TRUE(backend->Request_Native_Screen_Shot(bmp.c_str()));
		WW3D::End_Render(false);
		m_rendering = false;
		int width = 0;
		int height = 0;
		int channels = 0;
		unsigned char *bits = stbi_load(bmp.c_str(), &width, &height, &channels, 3);
		ASSERT_NE(bits, nullptr) << bmp;
		rgb.assign(bits, bits + width * height * 3);
		stbi_image_free(bits);
		DeleteFileA(bmp.c_str());
		ASSERT_EQ(width, static_cast<int>(Width));
		ASSERT_EQ(height, static_cast<int>(Height));
	}
	else
	{
		// The DX8 back buffer keeps the frame until it is presented.
		WW3D::End_Render(false);
		m_rendering = false;
		RenderBackendImage image;
		ASSERT_TRUE(backend->Capture_Back_Buffer_Image(0, image));
		ASSERT_EQ(image.Width, Width);
		ASSERT_EQ(image.Height, Height);
		for (UINT y = 0; y < Height; ++y)
		{
			const DWORD *row = reinterpret_cast<const DWORD *>(image.Bytes.data() + y * image.Pitch);
			for (UINT x = 0; x < Width; ++x)
			{
				rgb.push_back(static_cast<unsigned char>(row[x] >> 16));
				rgb.push_back(static_cast<unsigned char>(row[x] >> 8));
				rgb.push_back(static_cast<unsigned char>(row[x]));
			}
		}
	}
	for (UINT i = 0; i < Width * Height; ++i)
	{
		m_pixels[i] = rgb[i * 3] << 16 | rgb[i * 3 + 1] << 8 | rgb[i * 3 + 2];
	}
	EXPECT_NE(stbi_write_png(imageName, Width, Height, 3, rgb.data(), Width * 3), 0) << imageName;
}
