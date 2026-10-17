/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once
#include "WW3D2/texture.h"
#include "WW3D2/render2d.h"
#include "WWLib/ref_ptr.h"
#include <string>

// Tool-only half-size particle target. Default-pool texture tracking handles reset.
class ParticleResolutionPass
{
public:
	ParticleResolutionPass(int outputWidth, int outputHeight);
	bool Begin(bool transparent, float background, std::string& error);
	void Composite(bool transparent);
private:
	bool Init(std::string& error);
	int outputWidth, outputHeight;
	RefCountPtr<TextureClass> target;
	Render2DClass composite;
};
