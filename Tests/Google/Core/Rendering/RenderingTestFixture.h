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
#include <d3d8.h>
#include <initializer_list>
#include <vector>

// Each test renders with a real D3D8 device into an offscreen render target. The CTest step of CI skips these tests;
// its rendering check runs them on Windows under DXVK and Mesa lavapipe and compares the saved images with the base commit.
// Write each test as TEST_F(RenderingTest, Name) in a file of its own. The tests share one device, which is created
// once for the suite, and each test starts with the default device state and a black render target.
class RenderingTest : public ::testing::Test
{
protected:
	static constexpr UINT Width = 256;
	static constexpr UINT Height = 256;

	// A position on the render target in pixels, and its color as 0xRRGGBB.
	struct Vertex
	{
		float x, y;
		DWORD color;
	};

	static void SetUpTestSuite();
	static void TearDownTestSuite();
	virtual void SetUp() override;

	static void clear(DWORD color);
	// Draws a triangle list and blends the vertex colors across each triangle.
	static void drawTriangles(std::initializer_list<Vertex> vertices);
	// Saves the render target as a PNG for the CI report and keeps its pixels for pixel().
	void capture(const char *imageName);
	// The color of a captured pixel as 0xRRGGBB.
	DWORD pixel(UINT x, UINT y) const { return m_pixels[y * Width + x]; }

	static IDirect3DDevice8 *s_device;

private:
	std::vector<DWORD> m_pixels = std::vector<DWORD>(Width * Height);
	static HWND s_window;
	static IDirect3D8 *s_d3d;
	static IDirect3DSurface8 *s_target;
	static IDirect3DSurface8 *s_readback;
	static DWORD s_defaultState;
};
