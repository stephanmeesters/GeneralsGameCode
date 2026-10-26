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
	static constexpr int Width = 256;
	static constexpr int Height = 256;

	static void SetUpTestSuite();
	static void TearDownTestSuite();
	virtual void SetUp() override;
	virtual void TearDown() override;

	static void clear(unsigned int color);
	// Renders the objects as a scene seen through s_camera.
	static void render(std::initializer_list<RenderObjClass *> objects);
	// Creates a texture whose texels get the 0xAARRGGBB colors that color(x, y) returns.
	static TextureClass *createTexture(int width, int height, const std::function<unsigned int(int x, int y)> &color);
	// Ends the frame without presenting it, saves the back buffer for the CI report as <test name>_<n>.png, where n
	// counts the captures of the test, and keeps its pixels for pixel(). Then starts the next frame, black again.
	// The images go to z_RenderTest for Zero Hour and g_RenderTest for Generals, next to the test executable.
	void capture();
	// The color of a captured pixel as 0xRRGGBB.
	unsigned int pixel(int x, int y) const { return m_pixels[y * Width + x]; }

	// Looks down at the origin from the south, with x pointing east, y north and z up.
	static CameraClass *s_camera;

private:
	std::vector<unsigned int> m_pixels = std::vector<unsigned int>(Width * Height);
	int m_captures = 0;
	bool m_rendering = false;
	static HWND s_window;
};
