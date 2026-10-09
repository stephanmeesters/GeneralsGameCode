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

#include "PreRTS.h"

#include "GameClient/ObserverProductionOverlay.h"

#include <algorithm>
#include <functional>

#include "Common/GlobalData.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/ProductionUpdate.h"

// Reference dimensions at 1080p; screen width does not affect tile scaling.
static const Int ICON_WIDTH = 60;
static const Int ICON_HEIGHT = 48;
static const Int BAR_HEIGHT = 5;
static const Int BORDER = 2;
static const Int STRIPE_WIDTH = 5;
static const Int TILE_GAP = 6;
static const Int ROW_GAP = 8;
static const Int FONT_SIZE = 12;
static const Int COUNT_OUTLINE = 2;

struct TileLayout
{
	Int iconW, iconH, barH, border;
	Bool flipped;
};

// Somewhere below chat messages, and high enough to leave room for observer notifications.
static Int areaTop()
{
	return Int(TheDisplay->getHeight() * 0.18f);
}

static void drawTile(const TileLayout& layout, Int x, Int y, Color color, const Image* image, Real progress,
	DisplayString* count)
{
	const Int right = x + layout.iconW;
	const Int bottom = y + layout.iconH;
	const Color white = GameMakeColor(255, 255, 255, 255);
	const Color black = GameMakeColor(0, 0, 0, 255);

	// Icon framed in the player color. Filled strips share the image bounds and avoid the
	// outline renderer's pixel offsets.
	TheWindowManager->winFillRect(GameMakeColor(0, 0, 0, 160), 1, x, y, right, bottom);
	if (image)
		TheWindowManager->winDrawImage(image, x, y, right, bottom);
	TheWindowManager->winFillRect(color, 1, x, y, right, y + layout.border);
	TheWindowManager->winFillRect(color, 1, x, bottom - layout.border, right, bottom);
	TheWindowManager->winFillRect(color, 1, x, y, x + layout.border, bottom);
	TheWindowManager->winFillRect(color, 1, right - layout.border, y, right, bottom);

	// Progress bar under the icon, filling from the stripe side.
	TheWindowManager->winFillRect(GameDarkenColor(color, 67), 1, x, bottom, right, bottom + layout.barH);
	Int progressW = Int(layout.iconW * progress);
	Int progressX = layout.flipped ? right - progressW : x;
	TheWindowManager->winFillRect(white, 1, progressX, bottom, progressX + progressW, bottom + layout.barH);

	// Keep the outlined count inside the frame, directly beside the colored border.
	IRegion2D visible = count->getVisibleBounds();
	Int inset = layout.border + COUNT_OUTLINE;
	Int countX = layout.flipped ? x + inset - visible.lo.x : right - inset - visible.hi.x;
	Int countY = bottom - inset - visible.hi.y;
	for (Int dy = -COUNT_OUTLINE; dy <= COUNT_OUTLINE; ++dy)
		for (Int dx = -COUNT_OUTLINE; dx <= COUNT_OUTLINE; ++dx)
			if (dx || dy)
				count->draw(countX + dx, countY + dy, black, black, 0, 0);
	count->draw(countX, countY, white, black, 0, 0);
}

ObserverProductionOverlay::~ObserverProductionOverlay()
{
	if (!TheDisplayStringManager)
		return;

	for (const auto& entry : m_countStrings)
		TheDisplayStringManager->freeDisplayString(entry.second);
}

void ObserverProductionOverlay::reset()
{
	m_frame = 0;
	m_hidden = false;
}

void ObserverProductionOverlay::Row::add(const Image* image, Category category, Int count, Real percent)
{
	if (count <= 0)
		return;

	Real progress = clamp(0.0f, percent / 100.0f, 1.0f);
	for (Tile& tile : tiles)
	{
		if (tile.image == image)
		{
			tile.count += count;
			tile.progress = max(tile.progress, progress);
			return;
		}
	}
	tiles.push_back({image, category, count, progress});
}

void ObserverProductionOverlay::addObject(Object* obj, void* row)
{
	if (obj->isEffectivelyDead())
		return;

	Row& target = *static_cast<Row*>(row);
	if (obj->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
		target.add(obj->getTemplate()->getButtonImage(), CATEGORY_STRUCTURE, 1, obj->getConstructionPercent());

	ProductionUpdateInterface* production = obj->getProductionUpdateInterface();
	const ProductionEntry* entry = production ? production->firstProduction() : nullptr;
	if (!entry)
		return;

	// ProductionEntry stores unit and upgrade pointers in a union. Check the type before reading it.
	if (entry->getProductionType() == PRODUCTION_UNIT)
		target.add(entry->getProductionObject()->getButtonImage(), CATEGORY_UNIT, entry->getProductionQuantityRemaining(), entry->getPercentComplete());
	else if (entry->getProductionType() == PRODUCTION_UPGRADE)
		target.add(entry->getProductionUpgrade()->getButtonImage(), CATEGORY_UPGRADE, 1, entry->getPercentComplete());
}

void ObserverProductionOverlay::collect()
{
	for (Row& row : m_rows)
	{
		row.present = false;
		row.tiles.clear();
	}

	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		Player* player = ThePlayerList->getNthPlayer(i);
		Int slot = ThePlayerList->getSlotIndex(player->getPlayerIndex());
		if (slot < 0 || player->isPlayerObserver())
			continue;

		Row& row = m_rows[slot];
		row.present = true;
		row.color = player->getPlayerColor();
		player->iterateObjects(addObject, &row);
		// Pointer order keeps tiles in place from frame to frame.
		std::sort(row.tiles.begin(), row.tiles.end(), [](const Tile& a, const Tile& b) {
			return a.category != b.category ? a.category < b.category : std::less<const Image*>()(a.image, b.image);
		});
	}
}

DisplayString* ObserverProductionOverlay::countString(Int count, GameFont* font)
{
	DisplayString*& text = m_countStrings[count];
	if (!text)
	{
		text = TheDisplayStringManager->newDisplayString();
		UnicodeString value;
		value.format(count < 0 ? L"+%d" : L"%d", abs(count));
		text->setText(value);
	}
	text->setFont(font);
	return text;
}

Int ObserverProductionOverlay::draw()
{
	if (m_hidden || TheGlobalData->m_observerProductionScale == 0)
		return 0;

	if (TheGameLogic->getFrame() != m_frame)
	{
		m_frame = TheGameLogic->getFrame();
		collect();
	}

	Real scale = TheDisplay->getHeight() / 1080.0f * TheGlobalData->m_observerProductionScale / 100.0f;
	auto px = [scale](Int value) { return max(1, Int(value * scale + 0.5f)); };
	GameFont* font = TheFontLibrary->getFont("Tahoma", px(FONT_SIZE), true);
	TileLayout layout = {px(ICON_WIDTH), px(ICON_HEIGHT), px(BAR_HEIGHT), px(BORDER), IsGameTextRightToLeft()};
	Int stripeW = px(STRIPE_WIDTH), gap = px(TILE_GAP), rowGap = px(ROW_GAP);

	// Right-to-left languages mirror the rows: the stripe sits at the right edge and tiles walk left.
	Int screenW = TheDisplay->getWidth();
	Int margin = Int(screenW * 0.01f);
	Int stripeX = layout.flipped ? screenW - margin - stripeW : margin;
	Int firstX = layout.flipped ? stripeX - gap - layout.iconW : stripeX + stripeW + gap;
	Int step = layout.flipped ? -(layout.iconW + gap) : layout.iconW + gap;
	Int maxTiles = max(1, (Int(screenW * 0.35f) - stripeW - gap) / (layout.iconW + gap));

	const Int top = areaTop();
	Int y = top;
	for (const Row& row : m_rows)
	{
		if (!row.present)
			continue;

		TheWindowManager->winFillRect(row.color, 1, stripeX, y, stripeX + stripeW, y + layout.iconH + layout.barH);
		Int total = Int(row.tiles.size());
		Int shown = total > maxTiles ? maxTiles - 1 : total;
		Int x = firstX;
		for (Int i = 0; i < shown; ++i, x += step)
			drawTile(layout, x, y, row.color, row.tiles[i].image, row.tiles[i].progress,
				countString(row.tiles[i].count, font));
		if (shown < total)
			drawTile(layout, x, y, row.color, nullptr, 0.0f, countString(shown - total, font));
		y += layout.iconH + layout.barH + rowGap;
	}
	return y > top ? y : 0;
}
