/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "Precompiled/BasePrecomp.h"
#include "WWLib/always.h"
#include "Lib/Debug.h"
#include "WW3D2/dx8wrapper.h"
#include "ParticleAtlas.h"
#include <d3dx8.h>
#include <algorithm>
#include <string.h>

namespace {
unsigned Power_Of_Two(unsigned n) { unsigned v = 1; while (v < n) v *= 2; return v; }
int Address(int x, int size, bool wrap) {
	return wrap ? (x % size + size) % size : std::max(0, std::min(x, size - 1));
}
// All channels are averaged together, AFTER conversion to premultiplied RGB.
std::vector<unsigned> Downsample(const std::vector<unsigned>& input, unsigned width, unsigned height) {
	const unsigned w = std::max(1u, width / 2), h = std::max(1u, height / 2);
	std::vector<unsigned> output(w * h);
	for (unsigned y = 0; y < h; ++y) for (unsigned x = 0; x < w; ++x) {
		unsigned result = 0;
		for (unsigned shift = 0; shift < 32; shift += 8) {
			unsigned sum = 0;
			for (unsigned dy = 0; dy < 2; ++dy) for (unsigned dx = 0; dx < 2; ++dx)
				sum += (input[std::min(y * 2 + dy, height - 1) * width + std::min(x * 2 + dx, width - 1)] >> shift) & 255;
			result |= ((sum + 2) / 4) << shift;
		}
		output[y * w + x] = result;
	}
	return output;
}
}

bool ParticleAtlas::Init(TextureClass* const* sources, unsigned count, std::string& error)
{
	IDirect3DDevice8* device = DX8Wrapper::_Get_D3D_Device8();
	D3DCAPS8 caps;
	if (FAILED(device->GetDeviceCaps(&caps))) { error = "Could not read atlas device caps."; return false; }
	unsigned levels = 7; // L0..L6; at least one gutter texel survives at L6.
	for (unsigned i = 0; i < count; ++i) {
		unsigned w = sources[i]->Get_Width(), h = sources[i]->Get_Height();
		if (!w || !h || Power_Of_Two(w) != w || Power_Of_Two(h) != h) {
			error = "Atlas experiment requires power-of-two source dimensions."; return false;
		}
		unsigned safe = 1;
		while (w > 1 && h > 1) { ++safe; w /= 2; h /= 2; }
		levels = std::min(levels, safe);
	}
	const unsigned gutter = 1u << (levels - 1);
	const unsigned atlasWidth = 2048;
	unsigned cursorX = 0, cursorY = 0, rowHeight = 0;
	entries.resize(count * 2);
	for (unsigned i = 0; i < count; ++i) {
		const unsigned w = sources[i]->Get_Width(), h = sources[i]->Get_Height();
		// Preserve the DDS mip images, including their authored sampling phase.
		// Regenerating a centered box chain from L0 shifts these legacy sprites.
		std::vector<std::vector<unsigned> > sourceMips;
		const unsigned sourceLevels = std::min(levels, static_cast<unsigned>(sources[i]->Peek_D3D_Texture()->GetLevelCount()));
		for (unsigned level = 0; level < sourceLevels; ++level) {
			const unsigned mipWidth = w >> level, mipHeight = h >> level;
			IDirect3DSurface8* source = sources[i]->Get_D3D_Surface_Level(level);
			IDirect3DSurface8* decoded = nullptr;
			HRESULT result = device->CreateImageSurface(mipWidth, mipHeight, D3DFMT_A8R8G8B8, &decoded);
			if (SUCCEEDED(result) && source) result = D3DXLoadSurfaceFromSurface(decoded, nullptr, nullptr, source, nullptr, nullptr, D3DX_FILTER_NONE, 0);
			else result = E_FAIL;
			if (source) source->Release();
			if (FAILED(result)) { if (decoded) decoded->Release(); error = "Could not decode atlas source mip."; return false; }
			D3DLOCKED_RECT lock;
			result = decoded->LockRect(&lock, nullptr, D3DLOCK_READONLY);
			if (FAILED(result)) { decoded->Release(); error = "Could not read atlas source mip."; return false; }
			sourceMips.push_back(std::vector<unsigned>(mipWidth * mipHeight));
			for (unsigned y = 0; y < mipHeight; ++y)
				memcpy(&sourceMips.back()[y * mipWidth], static_cast<const unsigned char*>(lock.pBits) + y * lock.Pitch, mipWidth * 4);
			decoded->UnlockRect(); decoded->Release();
		}
		for (unsigned variant = 0; variant < 2; ++variant) {
			Entry& entry = entries[i * 2 + variant];
			entry.width = w; entry.height = h;
			entry.wrapU = sources[i]->Get_Filter().Get_U_Addr_Mode() == TextureFilterClass::TEXTURE_ADDRESS_REPEAT;
			entry.wrapV = sources[i]->Get_Filter().Get_V_Addr_Mode() == TextureFilterClass::TEXTURE_ADDRESS_REPEAT;
			const unsigned cellWidth = w + gutter * 2, cellHeight = h + gutter * 2;
			if (cellWidth > atlasWidth) { error = "Atlas entry is too wide."; return false; }
			if (cursorX + cellWidth > atlasWidth) { cursorX = 0; cursorY += rowHeight; rowHeight = 0; }
			entry.x = cursorX + gutter; entry.y = cursorY + gutter;
			cursorX += cellWidth; rowHeight = std::max(rowHeight, cellHeight);
			for (unsigned level = 0; level < levels; ++level) {
				if (level >= sourceMips.size()) {
					// Some DDS chains stop at 4x4. Generate only the absent tail,
					// independently, from this variant's last converted level.
					entry.mips.push_back(Downsample(entry.mips.back(), w >> (level - 1), h >> (level - 1)));
					continue;
				}
				entry.mips.push_back(sourceMips[level]);
				for (unsigned& pixel : entry.mips.back()) {
					if (variant) pixel &= 0x00FFFFFFu; // ADD keeps RGB, alpha is always zero.
					else {
						const unsigned alpha = pixel >> 24;
						unsigned converted = pixel & 0xFF000000u;
						for (unsigned shift = 0; shift < 24; shift += 8)
							converted |= ((((pixel >> shift) & 255) * alpha + 127) / 255) << shift;
						pixel = converted;
					}
				}
			}
		}
	}
	const unsigned atlasHeight = Power_Of_Two(cursorY + rowHeight);
	if (atlasWidth > caps.MaxTextureWidth || atlasHeight > caps.MaxTextureHeight) { error = "Atlas exceeds the device texture-size limit."; return false; }
	IDirect3DTexture8* d3dAtlas = nullptr;
	if (FAILED(device->CreateTexture(atlasWidth, atlasHeight, levels, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &d3dAtlas))) {
		error = "Could not allocate particle atlas."; return false;
	}
	for (unsigned level = 0; level < levels; ++level) {
		D3DLOCKED_RECT lock;
		if (FAILED(d3dAtlas->LockRect(level, &lock, nullptr, 0))) { d3dAtlas->Release(); error = "Could not upload particle atlas."; return false; }
		for (unsigned y = 0; y < (atlasHeight >> level); ++y) memset(static_cast<unsigned char*>(lock.pBits) + y * lock.Pitch, 0, (atlasWidth >> level) * 4);
		const int border = gutter >> level;
		for (Entry& entry : entries) {
			const int w = entry.width >> level, h = entry.height >> level;
			const int ox = entry.x >> level, oy = entry.y >> level;
			for (int y = -border; y < h + border; ++y) {
				unsigned* row = reinterpret_cast<unsigned*>(static_cast<unsigned char*>(lock.pBits) + (oy + y) * lock.Pitch);
				for (int x = -border; x < w + border; ++x)
					row[ox + x] = entry.mips[level][Address(y, h, entry.wrapV) * w + Address(x, w, entry.wrapU)];
			}
		}
		d3dAtlas->UnlockRect(level);
	}
	texture.Assign_No_Add_Ref(new TextureClass(d3dAtlas)); d3dAtlas->Release();
	texture->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
	texture->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
	// Limit the footprint to bilinear/trilinear: one guard texel at the last mip.
	texture->Get_Filter().Set_Min_Filter(TextureFilterClass::FILTER_TYPE_FAST);
	texture->Get_Filter().Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_FAST);
	for (Entry& entry : entries) {
		entry.uv.Set(float(entry.x) / atlasWidth, float(entry.y) / atlasHeight,
			float(entry.x + entry.width) / atlasWidth, float(entry.y + entry.height) / atlasHeight);
		IDirect3DTexture8* isolated = nullptr;
		if (FAILED(device->CreateTexture(entry.width, entry.height, levels, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &isolated))) {
			error = "Could not allocate isolated atlas control texture."; return false;
		}
		for (unsigned level = 0; level < levels; ++level) {
			D3DLOCKED_RECT lock;
			if (FAILED(isolated->LockRect(level, &lock, nullptr, 0))) { isolated->Release(); error = "Could not upload isolated atlas control."; return false; }
			for (unsigned y = 0; y < (entry.height >> level); ++y)
				memcpy(static_cast<unsigned char*>(lock.pBits) + y * lock.Pitch, &entry.mips[level][y * (entry.width >> level)], (entry.width >> level) * 4);
			isolated->UnlockRect(level);
		}
		entry.isolated.Assign_No_Add_Ref(new TextureClass(isolated)); isolated->Release();
		entry.isolated->Get_Filter() = texture->Get_Filter();
		entry.isolated->Get_Filter().Set_U_Addr_Mode(entry.wrapU ? TextureFilterClass::TEXTURE_ADDRESS_REPEAT : TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		entry.isolated->Get_Filter().Set_V_Addr_Mode(entry.wrapV ? TextureFilterClass::TEXTURE_ADDRESS_REPEAT : TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		entry.mips.clear(); // Converted pixels live in managed D3D textures now.
	}
	DEBUG_LOG(("Particle atlas: %ux%u, %u variants, %u mips, base gutter %u, last gutter 1, ARGB8", atlasWidth, atlasHeight, count * 2, levels, gutter));
	return true;
}
