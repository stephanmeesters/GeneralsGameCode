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

#pragma once

#include <gtest/gtest.h>
#include <windows.h>
#include <functional>
#include <initializer_list>
#include <vector>

class CameraClass;
class RenderObjClass;
class TextureClass;

// Each test renders one frame with WW3D. The CTest step of CI skips these tests; its rendering check runs them on
// Windows under DXVK and Mesa lavapipe and compares the saved images with the base commit.
// The tests draw through WW3D and Renderer only, so that they run unchanged on other render backends. Only capture()
// reads the back buffer the DX8 way.
// Write each test as TEST_F(RenderingTest, Name) in a file of its own. The tests share the device, which WW3D creates
// once for the suite, and each test starts its frame with a black back buffer.
class RenderingTest : public ::testing::Test
{
protected:
	static constexpr UINT Width = 256;
	static constexpr UINT Height = 256;

	// A position on the back buffer in pixels, and its color as 0xRRGGBB.
	struct Vertex
	{
		float x, y;
		DWORD color;
	};

	static void SetUpTestSuite();
	static void TearDownTestSuite();
	virtual void SetUp() override;
	virtual void TearDown() override;

	static void clear(DWORD color);
	// Draws a triangle list and blends the vertex colors across each triangle.
	static void drawTriangles(std::initializer_list<Vertex> vertices);
	// Renders the objects as a scene seen through s_camera.
	static void render(std::initializer_list<RenderObjClass *> objects);
	// Creates a texture whose texels get the 0xAARRGGBB colors that color(x, y) returns.
	static TextureClass *createTexture(UINT width, UINT height, const std::function<DWORD(UINT x, UINT y)> &color);
	// Ends the frame without presenting it, saves the back buffer as a PNG for the CI report and keeps its pixels
	// for pixel().
	void capture(const char *imageName);
	// The color of a captured pixel as 0xRRGGBB.
	DWORD pixel(UINT x, UINT y) const { return m_pixels[y * Width + x]; }

	// Looks down at the origin from the south, with x pointing east, y north and z up.
	static CameraClass *s_camera;

private:
	std::vector<DWORD> m_pixels = std::vector<DWORD>(Width * Height);
	bool m_rendering = false;
	static HWND s_window;
};
