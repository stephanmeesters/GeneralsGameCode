/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once
#include "WW3D2/texture.h"
#include "WWLib/ref_ptr.h"
#include "WWMath/vector4.h"
#include <string>
#include <vector>

// Startup-built experiment: two entries per source, never rebuilt per frame.
class ParticleAtlas
{
public:
	bool Init(TextureClass* const* sources, unsigned count, std::string& error);
	TextureClass* Get_Texture() const { return texture.Peek(); }
	TextureClass* Get_Isolated(unsigned source, bool additive) const { return entries[source * 2 + additive].isolated.Peek(); }
	const Vector4& Get_Rect(unsigned source, bool additive) const { return entries[source * 2 + additive].uv; }
private:
	struct Entry {
		unsigned x, y, width, height;
		bool wrapU, wrapV;
		Vector4 uv;
		std::vector<std::vector<unsigned> > mips;
		RefCountPtr<TextureClass> isolated;
	};
	std::vector<Entry> entries;
	RefCountPtr<TextureClass> texture;
};
