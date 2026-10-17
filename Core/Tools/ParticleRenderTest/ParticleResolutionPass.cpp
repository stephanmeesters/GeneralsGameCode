/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "Precompiled/BasePrecomp.h"
#include "WWLib/always.h"
#include "Lib/Debug.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/sortingrenderer.h"
#include "ParticleResolutionPass.h"

ParticleResolutionPass::ParticleResolutionPass(int width, int height) : outputWidth(width), outputHeight(height)
{
}

bool ParticleResolutionPass::Init(std::string& error)
{
	if (target) return true;
	// Avoid Create_Render_Target(), which forces a square power-of-two target.
	// This constructor registers a default-pool resource with the engine tracker.
	target.Assign_No_Add_Ref(new TextureClass(outputWidth / 2, outputHeight / 2,
		WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1, TextureClass::POOL_DEFAULT, true, false));
	if (!target->Peek_D3D_Texture()) {
		error = "Could not allocate the half-resolution ARGB8 render target.";
		target = nullptr;
		return false;
	}
	D3DSURFACE_DESC desc;
	if (FAILED(target->Peek_D3D_Texture()->GetLevelDesc(0, &desc)) ||
		desc.Width != outputWidth / 2 || desc.Height != outputHeight / 2 || desc.Format != D3DFMT_A8R8G8B8) {
		error = "This device did not provide an exact half-resolution ARGB8 render target.";
		target = nullptr;
		return false;
	}
	target->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
	target->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
	target->Get_Filter().Set_Min_Filter(TextureFilterClass::FILTER_TYPE_FAST);
	target->Get_Filter().Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_FAST);
	target->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);
	composite.Set_Texture(target.Peek());
	composite.Set_Coordinate_Range(RectClass(0, 0, float(outputWidth), float(outputHeight)));
	composite.Add_Quad(RectClass(0, 0, float(outputWidth), float(outputHeight)), RectClass(0, 0, 1, 1));
	composite.Get_Shader()->Set_Src_Blend_Func(ShaderClass::SRCBLEND_ONE);
	DEBUG_LOG(("Particle half-resolution target: %ux%u ARGB8; output %dx%d", desc.Width, desc.Height, outputWidth, outputHeight));
	return true;
}

bool ParticleResolutionPass::Begin(bool transparent, float background, std::string& error)
{
	if (!Init(error)) return false;
	// Unbind last frame's sampled target before binding it for rendering.
	DX8Wrapper::Set_Texture(0, nullptr);
	DX8Wrapper::Apply_Render_State_Changes();
	IDirect3DSurface8* surface = target->Get_D3D_Surface_Level();
	if (!surface) { error = "Half-resolution target is not available."; return false; }
	DX8Wrapper::Set_Render_Target(surface, true);
	surface->Release();
	D3DVIEWPORT8 viewport = { 0, 0, DWORD(outputWidth / 2), DWORD(outputHeight / 2), 0.0f, 1.0f };
	DX8Wrapper::Set_Viewport(&viewport);
	// The prototype has no opaque geometry and uses a freshly cleared depth
	// buffer. Reusing that larger buffer is valid here, not general scene occlusion.
	const float color = transparent ? 0.0f : background;
	DX8Wrapper::Clear(true, false, Vector3(color, color, color), transparent ? 0.0f : 1.0f);
	return true;
}

void ParticleResolutionPass::Composite(bool transparent)
{
	// Deferred particles must be consumed while the small target is still bound.
	SortingRendererClass::Flush();
	DX8Wrapper::Set_Pixel_Shader(0);
	DX8Wrapper::Set_Render_Target(static_cast<IDirect3DSurface8*>(nullptr));
	// The particle mip inspection control must not affect the single-level target.
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAXMIPLEVEL, 0);
	composite.Get_Shader()->Set_Dst_Blend_Func(transparent ?
		ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA : ShaderClass::DSTBLEND_ZERO);
	// Render2D applies the full output viewport and its standard DX8 pixel bias.
	// It uses ordinary indexed drawing, so the fullscreen pass is counted in stats.
	composite.Render();
}
