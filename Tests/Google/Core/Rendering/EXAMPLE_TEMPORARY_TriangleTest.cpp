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

// A triangle with a red, a green and a white corner blends the corner colors across its area.
TEST_F(RenderingTest, Triangle)
{
	const DWORD background = 0x102030;
	clear(background);
	drawTriangles({
		{128.0f, 32.0f, 0xff0000},
		{224.0f, 224.0f, 0x00ff00},
		{32.0f, 224.0f, 0xffffff},
	});
	capture("triangle.png");
	// GPUs round the blended colors differently, so the exact colors are left to the image comparison of CI.
	EXPECT_EQ(pixel(8, 8), background);
	EXPECT_NE(pixel(128, 160), background);
}
