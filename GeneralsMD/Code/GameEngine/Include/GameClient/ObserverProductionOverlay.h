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

#include "Common/STLTypedefs.h"
#include "GameClient/Color.h"
#include "GameNetwork/NetworkDefs.h"

class DisplayString;
class GameFont;
class Image;
class Object;

class ObserverProductionOverlay
{
public:
	~ObserverProductionOverlay();

	void reset();
	void toggle() { m_hidden = !m_hidden; }
	Int draw(); ///< Returns the bottom edge of the rows, or 0 when nothing was drawn

private:
	enum Category { CATEGORY_STRUCTURE, CATEGORY_UNIT, CATEGORY_UPGRADE };

	struct Tile
	{
		const Image* image; ///< the button image; things showing the same image share a tile
		Category category;
		Int count;
		Real progress;
	};

	struct Row
	{
		Bool present = false;
		Color color = 0;
		std::vector<Tile> tiles;

		void add(const Image* image, Category category, Int count, Real percent);
	};

	static void addObject(Object* obj, void* row);
	void collect();
	DisplayString* countString(Int count, GameFont* font);

	Row m_rows[MAX_SLOTS]; ///< Indexed by game slot, refreshed once per logic frame
	UnsignedInt m_frame = 0;
	Bool m_hidden = false;
	std::map<Int, DisplayString*> m_countStrings; ///< Negative keys hold "+N" overflow labels
};
