/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "Precompiled/BasePrecomp.h"
#include "WWLib/always.h"
#include "WWLib/win.h"
#include "Lib/Debug.h"
#include "WW3D2/assetmgr.h"
#include "WW3D2/camera.h"
#include "WW3D2/ddsfile.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/statistics.h"
#include "WW3D2/sortingrenderer.h"
#include "WW3D2/pointgr.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/texture.h"
#include "WW3D2/ww3d.h"
#include "WWMath/wwmath.h"
#include "BigTextureFileFactory.h"
#include "ParticleAtlas.h"
#include "ParticleResolutionPass.h"
#include <d3d8.h>
#include <d3dx8.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>

namespace
{
// Match the other W3D tools: debug utilities must outlive the renderer,
// archive adapter and all their cleanup paths, including startup failures.
class DebugSession
{
public:
	DebugSession()
	{
		DEBUG_INIT(DEBUG_FLAGS_DEFAULT);
#ifdef DEBUG_LOGGING
		DebugOpenLogFile("ParticleRenderTest-", "");
#endif
	}
	~DebugSession() { DEBUG_SHUTDOWN(); }
};

const int WINDOW_WIDTH = 2560;
const int WINDOW_HEIGHT = 1440;
const int LABEL_HEIGHT = 24;
int previewWidth = 1280;
int previewHeight = 720;
HWND referenceWindow = nullptr;
HWND liveLabel = nullptr;
HWND referenceLabel = nullptr;
std::vector<unsigned int> referencePixels;
enum ComparisonState { COMPARISON_PENDING, COMPARISON_MATCH, COMPARISON_DIFFERENT, COMPARISON_ERROR, COMPARISON_PAUSED };
ComparisonState comparisonState = COMPARISON_PENDING;
bool comparisonEnabled = true;
bool comparisonRequested = true;
HBITMAP referenceBitmap = nullptr;
std::string referencePath;
std::string referenceDirectory;
int selectedScene = 0;
const unsigned STRESS_PARTICLE_COUNT = 4000;
const unsigned STRESS_SEED = 0x5EED1234u;
const unsigned OVERDRAW_PARTICLE_COUNT = 100;
const unsigned OVERDRAW_SEED = 0x0D12A100u;
const unsigned BATCH_CAPACITY = 512;
bool captureReference = false;
const char WINDOW_CLASS[] = "GeneralsParticleRenderTest";

bool halfResolution = false;
std::string halfResolutionError;
bool atlasEnabled = false;
bool isolatedEnabled = false;
bool mipStress = false;
unsigned forcedMip = 0;
bool trilinear = false;
std::string atlasError;
bool premultiplied = false;
std::string premultipliedError;
bool sorting = true;
bool reverseSubmission = false;
bool lightBackground = false;
double displayedFps = 0.0;
double displayedFrameMs = 0.0;
unsigned displayedDraws = 0;
int displayedTriangles = 0;

struct TestParticle
{
	float x, y, z;
	float red, green, blue, alpha;
	bool additive;
	const char* texture;
	unsigned textureIndex;
	float size;
};

// Distinct depths and alternating modes make additive/alpha ordering visible.
// The deliberately scrambled submission order exercises the real triangle sorter.
const TestParticle PARTICLES[] = {
	{ -0.9f,  0.4f, -8.0f, 0.90f, 0.15f, 0.10f, 0.65f, false, "Art/Textures/EXSmokNew1.dds", 0, 1.4f },
	{  0.7f,  0.2f, -6.0f, 0.10f, 0.35f, 0.80f, 1.00f, true, "Art/Textures/EXexplo03.dds", 1, 1.4f },
	{  0.2f, -0.5f, -9.0f, 0.10f, 0.75f, 0.30f, 0.65f, false, "Art/Textures/EXSmokePuf07.dds", 2, 1.4f },
	{ -0.5f, -0.2f, -5.0f, 0.65f, 0.20f, 0.05f, 1.00f, true, "Art/Textures/EXFire01.dds", 3, 1.4f },
	{  0.8f,  0.5f, -7.0f, 0.50f, 0.15f, 0.75f, 0.65f, false, "Art/Textures/EXCloud01.dds", 4, 1.4f },
	{ -0.1f,  0.6f, -9.5f, 0.10f, 0.40f, 0.65f, 1.00f, true, "Art/Textures/EXLnzFlar2.dds", 5, 1.4f },
	{ -0.6f, -0.6f, -5.5f, 0.10f, 0.55f, 0.65f, 0.65f, false, "Art/Textures/EXCloud02.dds", 6, 1.4f },
	{  0.4f, -0.3f, -7.5f, 0.55f, 0.35f, 0.05f, 1.00f, true, "Art/Textures/EXFireball03.dds", 7, 1.4f },
	{  0.0f,  0.0f, -4.5f, 0.75f, 0.20f, 0.40f, 0.55f, false, "Art/Textures/EXWater04.dds", 8, 1.4f },
	{ -0.3f,  0.0f, -6.5f, 0.15f, 0.55f, 0.10f, 1.00f, true, "Art/Textures/EXSpark01.dds", 9, 1.4f },
};
const int PARTICLE_COUNT = sizeof(PARTICLES) / sizeof(PARTICLES[0]);

// Explicit xorshift32 and 24-bit conversion keep the scene reproducible
// independently of the C library's rand() or STL distribution implementation.
class SceneRandom
{
public:
	explicit SceneRandom(unsigned seed = STRESS_SEED) : state(seed) {}
	unsigned Next()
	{
		state ^= state << 13;
		state ^= state >> 17;
		state ^= state << 5;
		return state;
	}
	float Unit() { return static_cast<float>(Next() >> 8) / 16777216.0f; }
private:
	unsigned state;
};

void Sort_Submission_Order(const std::vector<TestParticle>& particles, std::vector<unsigned>& order)
{
	// Bound the sorting node count. The real triangle sorter restores global
	// depth order across these texture/mode batches before issuing GPU draws.
	std::stable_sort(order.begin(), order.end(), [&particles](unsigned a, unsigned b) {
		const TestParticle& left = particles[a];
		const TestParticle& right = particles[b];
		const unsigned leftKey = left.textureIndex * 2 + (left.additive ? 1 : 0);
		const unsigned rightKey = right.textureIndex * 2 + (right.additive ? 1 : 0);
		return leftKey < rightKey;
	});
}

void Build_Stress_Scene(std::vector<TestParticle>& particles, std::vector<unsigned>& order)
{
	SceneRandom random;
	particles.reserve(STRESS_PARTICLE_COUNT);
	order.reserve(STRESS_PARTICLE_COUNT);
	for (unsigned i = 0; i < STRESS_PARTICLE_COUNT; ++i) {
		TestParticle particle = PARTICLES[random.Next() % PARTICLE_COUNT];
		// Stratify depth to guarantee distinct values, with seeded jitter.
		const float depth = 6.0f + 18.0f * (i + random.Unit() * 0.5f) / STRESS_PARTICLE_COUNT;
		particle.x = (random.Unit() * 2.0f - 1.0f) * depth * 0.42f;
		particle.y = (random.Unit() * 2.0f - 1.0f) * depth * 0.30f;
		particle.z = -depth;
		particle.red = 0.15f + random.Unit() * 0.60f;
		particle.green = 0.15f + random.Unit() * 0.60f;
		particle.blue = 0.15f + random.Unit() * 0.60f;
		particle.alpha = 0.15f + random.Unit() * 0.50f;
		particle.additive = (random.Next() & 1) != 0;
		particle.size = depth * (0.008f + random.Unit() * 0.018f);
		particles.push_back(particle);
		order.push_back(i);
	}
	Sort_Submission_Order(particles, order);
	DEBUG_LOG(("Particle test: generated %u particles with seed 0x%08X", STRESS_PARTICLE_COUNT, STRESS_SEED));
}

void Build_Overdraw_Scene(std::vector<TestParticle>& particles, std::vector<unsigned>& order)
{
	SceneRandom random(OVERDRAW_SEED);
	for (unsigned i = 0; i < OVERDRAW_PARTICLE_COUNT; ++i) {
		TestParticle particle = PARTICLES[i % PARTICLE_COUNT];
		const float depth = 8.0f + 12.0f * (i + random.Unit() * 0.5f) / OVERDRAW_PARTICLE_COUNT;
		particle.x = (random.Unit() * 2.0f - 1.0f) * depth * 0.08f;
		particle.y = (random.Unit() * 2.0f - 1.0f) * depth * 0.05f;
		particle.z = -depth;
		particle.additive = ((i + i / PARTICLE_COUNT) & 1) != 0;
		// Keep the blended image visible instead of saturating after 100 layers.
		const float tintScale = particle.additive ? 0.1f : 1.0f;
		particle.red = (0.08f + random.Unit() * 0.22f) * tintScale;
		particle.green = (0.08f + random.Unit() * 0.22f) * tintScale;
		particle.blue = (0.08f + random.Unit() * 0.22f) * tintScale;
		particle.alpha = 0.035f + random.Unit() * 0.045f;
		// At horizontal FOV 0.9, full view width is about 0.97 * depth.
		// These quads cover the viewport even with their small center offsets.
		particle.size = depth * (1.2f + random.Unit() * 0.4f);
		particles.push_back(particle);
		order.push_back(i);
	}
	Sort_Submission_Order(particles, order);
	DEBUG_LOG(("Particle test: generated %u screen-filling particles with seed 0x%08X", OVERDRAW_PARTICLE_COUNT, OVERDRAW_SEED));
}

void Build_Atlas_Order(const std::vector<TestParticle>& particles, const std::vector<unsigned>& order, std::vector<unsigned>& atlasOrder)
{
	atlasOrder = order;
	std::stable_sort(atlasOrder.begin(), atlasOrder.end(), [&particles](unsigned a, unsigned b) {
		return particles[a].z < particles[b].z;
	});
}

unsigned Scene_Particle_Count()
{
	return selectedScene == 0 ? static_cast<unsigned>(PARTICLE_COUNT) :
		selectedScene == 1 ? STRESS_PARTICLE_COUNT : OVERDRAW_PARTICLE_COUNT;
}

const char* Scene_Description()
{
	return selectedScene == 0 ? "overlap" : selectedScene == 1 ? "small particles" : "screen-filling overdraw";
}

const char* Comparison_Name()
{
	switch (comparisonState) {
	case COMPARISON_MATCH: return "MATCH";
	case COMPARISON_DIFFERENT: return "DIFFERENT";
	case COMPARISON_ERROR: return "CHECK FAILED";
	case COMPARISON_PAUSED: return "CHECK PAUSED";
	default: return "CHECK PENDING";
	}
}

void Update_Comparison_Label(const char* detail = nullptr)
{
	if (!liveLabel) return;
	char label[256];
	snprintf(label, sizeof(label), "LIVE: %s%s%s", Comparison_Name(), detail ? " | " : "", detail ? detail : "");
	SetWindowTextA(liveLabel, label);
	InvalidateRect(liveLabel, nullptr, TRUE);
}

void Update_Title(HWND window)
{
	char title[512];
	snprintf(title, sizeof(title),
		"Scene %d: %u particles (%s) | 2560x1440 | %s | %.1f FPS | %.2f ms | %u draws | %d triangles | Q: %s | T: atlas %s | I: converted %s | M: distant %s | L: mip >=%u | H: %s | P: premul %s | A: add batching %s | S: sorting %s | R: %s | B: bg | C: capture | V: compare | Esc",
		selectedScene + 1, Scene_Particle_Count(), Scene_Description(),
		Comparison_Name(), displayedFps, displayedFrameMs, displayedDraws, displayedTriangles,
		halfResolution ? "half 1280x720" : "full 2560x1440",
		atlasEnabled ? "ON" : "OFF", isolatedEnabled ? "ON" : "OFF", mipStress ? "ON" : "OFF", forcedMip, trilinear ? "trilinear" : "bilinear",
		premultiplied ? "ON" : "OFF",
		SortingRendererClass::Is_Additive_Batching_Enabled() ? "ON" : "OFF",
		sorting ? "ON" : "OFF", reverseSubmission ? "reversed" : "original");
	SetWindowTextA(GetAncestor(window, GA_ROOT), title);
}

LRESULT CALLBACK Window_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message) {
	case WM_KEYDOWN:
		// Ignore key repeat, so one press changes one setting.
		if (lparam & (1L << 30)) return 0;
		switch (wparam) {
		case VK_ESCAPE: PostMessage(window, WM_CLOSE, 0, 0); break;
		case 'S': sorting = !sorting; break;
		case 'R': reverseSubmission = !reverseSubmission; break;
		case 'B': lightBackground = !lightBackground; break;
		case 'Q':
			if (halfResolutionError.empty()) halfResolution = !halfResolution;
			else MessageBoxA(GetAncestor(window, GA_ROOT), halfResolutionError.c_str(), "Half-resolution rendering unavailable", MB_OK | MB_ICONERROR);
			break;
		case 'H':
			trilinear = !trilinear;
			WW3D::Set_Texture_Filter(trilinear ? TextureFilterClass::TEXTURE_FILTER_TRILINEAR : TextureFilterClass::TEXTURE_FILTER_BILINEAR);
			break;
		case 'L': forcedMip = (forcedMip + 1) % 7; break;
		case 'M': mipStress = !mipStress; break;
		case 'T':
		case 'I':
			if (atlasError.empty()) {
				if (wparam == 'T') { atlasEnabled = !atlasEnabled; isolatedEnabled = false; }
				else { isolatedEnabled = !isolatedEnabled; atlasEnabled = false; }
				premultiplied = false;
			} else MessageBoxA(GetAncestor(window, GA_ROOT), atlasError.c_str(), "Atlas unavailable", MB_OK | MB_ICONERROR);
			break;
		case 'P':
			if (premultipliedError.empty()) { premultiplied = !premultiplied; atlasEnabled = isolatedEnabled = false; }
			else MessageBoxA(GetAncestor(window, GA_ROOT), premultipliedError.c_str(), "Premultiplied blending unavailable", MB_OK | MB_ICONERROR);
			break;
		case 'A':
			SortingRendererClass::Enable_Additive_Batching(!SortingRendererClass::Is_Additive_Batching_Enabled());
			break;
		case 'C': captureReference = true; break;
		case 'V': comparisonEnabled = !comparisonEnabled; break;
		case '1': selectedScene = 0; break;
		case '2': selectedScene = 1; break;
		case '3': selectedScene = 2; break;
		}
		if (wparam == 'S' || wparam == 'R' || wparam == 'B' || wparam == 'C' || wparam == 'V' || wparam == 'A' || wparam == 'P' || wparam == 'T' || wparam == 'I' || wparam == 'M' || wparam == 'L' || wparam == 'H' || wparam == 'Q' || wparam == '1' || wparam == '2' || wparam == '3') {
			comparisonRequested = true;
			comparisonState = comparisonEnabled ? COMPARISON_PENDING : COMPARISON_PAUSED;
			Update_Comparison_Label();
		}
		Update_Title(window);
		return 0;
	case WM_CTLCOLORSTATIC:
		if (reinterpret_cast<HWND>(lparam) == liveLabel) {
			HDC dc = reinterpret_cast<HDC>(wparam);
			SetTextColor(dc, comparisonState == COMPARISON_MATCH ? RGB(0, 120, 0) :
				comparisonState == COMPARISON_DIFFERENT || comparisonState == COMPARISON_ERROR ? RGB(180, 0, 0) : RGB(0, 0, 0));
			SetBkColor(dc, GetSysColor(COLOR_BTNFACE));
			return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
		}
		break;
	case WM_PAINT:
		if (window == referenceWindow) {
			PAINTSTRUCT paint;
			HDC dc = BeginPaint(window, &paint);
			FillRect(dc, &paint.rcPaint, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
			if (referenceBitmap) {
				HDC memory = CreateCompatibleDC(dc);
				HGDIOBJ old = SelectObject(memory, referenceBitmap);
				RECT client;
				GetClientRect(window, &client);
				SetStretchBltMode(dc, HALFTONE);
				SetBrushOrgEx(dc, 0, 0, nullptr);
				StretchBlt(dc, 0, 0, client.right, client.bottom, memory, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, SRCCOPY);
				SelectObject(memory, old);
				DeleteDC(memory);
			}
			EndPaint(window, &paint);
			return 0;
		}
		break;
	case WM_CLOSE:
		// Keep the renderer's HWND alive until graphics resources are released.
		PostQuitMessage(0);
		return 0;
	case WM_ERASEBKGND:
		return 1;
	}
	return DefWindowProcA(window, message, wparam, lparam);
}

bool Load_Reference(const char* path, HBITMAP& bitmap, std::vector<unsigned int>& pixels, std::string& error)
{
	bitmap = static_cast<HBITMAP>(LoadImageA(nullptr, path, IMAGE_BITMAP, 0, 0,
		LR_LOADFROMFILE | LR_CREATEDIBSECTION));
	BITMAP description = {};
	if (!bitmap || !GetObject(bitmap, sizeof(description), &description) ||
		description.bmWidth != WINDOW_WIDTH || description.bmHeight != WINDOW_HEIGHT) {
		if (bitmap) DeleteObject(bitmap);
		bitmap = nullptr;
		error = std::string("Could not load a 2560x1440 reference bitmap: ") + path;
		return false;
	}
	BITMAPINFO info = {};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = WINDOW_WIDTH;
	info.bmiHeader.biHeight = -WINDOW_HEIGHT;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;
	pixels.resize(WINDOW_WIDTH * WINDOW_HEIGHT);
	HDC dc = CreateCompatibleDC(nullptr);
	const int rows = dc ? GetDIBits(dc, bitmap, 0, WINDOW_HEIGHT, &pixels[0], &info, DIB_RGB_COLORS) : 0;
	if (dc) DeleteDC(dc);
	if (rows != WINDOW_HEIGHT) {
		DeleteObject(bitmap);
		bitmap = nullptr;
		pixels.clear();
		error = "Could not read RGB pixels from the reference bitmap.";
		return false;
	}
	return true;
}

bool Select_Scene_Reference(std::string& error)
{
	const char* sceneIds[] = { "10", "4000-5EED1234", "overdraw-100-0D12A100" };
	char filename[128];
	snprintf(filename, sizeof(filename), "ParticleRenderTest-reference-%s-%dx%d.bmp", sceneIds[selectedScene], WINDOW_WIDTH, WINDOW_HEIGHT);
	referencePath = referenceDirectory + filename;
	HBITMAP replacement = nullptr;
	std::vector<unsigned int> pixels;
	const DWORD attributes = GetFileAttributesA(referencePath.c_str());
	if (attributes != INVALID_FILE_ATTRIBUTES) {
		if (!Load_Reference(referencePath.c_str(), replacement, pixels, error)) return false;
	} else if (GetLastError() != ERROR_FILE_NOT_FOUND) {
		error = std::string("Could not access reference image: ") + referencePath;
		return false;
	}
	if (referenceBitmap) DeleteObject(referenceBitmap);
	referenceBitmap = replacement;
	referencePixels.swap(pixels);
	captureReference = referenceBitmap == nullptr;
	comparisonRequested = true;
	comparisonState = comparisonEnabled ? COMPARISON_PENDING : COMPARISON_PAUSED;
	Update_Comparison_Label();
	char label[256];
	snprintf(label, sizeof(label), "REFERENCE: scene %d, %u particles (%s), %dx%d (C replaces it)",
		selectedScene + 1, Scene_Particle_Count(), Scene_Description(), WINDOW_WIDTH, WINDOW_HEIGHT);
	SetWindowTextA(referenceLabel, label);
	InvalidateRect(referenceWindow, nullptr, FALSE);
	return true;
}

bool Save_Reference(std::string& error)
{
	// Capture only this application's rendered backbuffer, before presentation.
	IDirect3DSurface8* backbuffer = nullptr;
	HRESULT result = DX8Wrapper::_Get_D3D_Device8()->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
	const std::string temporary = referencePath + ".tmp.bmp";
	if (SUCCEEDED(result)) {
		result = D3DXSaveSurfaceToFileA(temporary.c_str(), D3DXIFF_BMP, backbuffer, nullptr, nullptr);
		backbuffer->Release();
	}
	if (FAILED(result)) {
		error = "Could not save the particle reference image.";
		return false;
	}
	HBITMAP replacement = nullptr;
	std::vector<unsigned int> pixels;
	if (!Load_Reference(temporary.c_str(), replacement, pixels, error)) {
		DeleteFileA(temporary.c_str());
		return false;
	}
	if (!MoveFileExA(temporary.c_str(), referencePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		DeleteObject(replacement);
		DeleteFileA(temporary.c_str());
		error = std::string("Could not replace reference image: ") + referencePath;
		return false;
	}
	if (referenceBitmap) DeleteObject(referenceBitmap);
	referenceBitmap = replacement;
	referencePixels.swap(pixels);
	comparisonRequested = true;
	InvalidateRect(referenceWindow, nullptr, FALSE);
	DEBUG_LOG(("Particle test: reference saved to %s", referencePath.c_str()));
	return true;
}

bool Compare_Reference(IDirect3DSurface8*& readback)
{
	if (referencePixels.size() != WINDOW_WIDTH * WINDOW_HEIGHT) return false;
	IDirect3DDevice8* device = DX8Wrapper::_Get_D3D_Device8();
	if (!readback && FAILED(device->CreateImageSurface(WINDOW_WIDTH, WINDOW_HEIGHT, D3DFMT_A8R8G8B8, &readback))) return false;
	IDirect3DSurface8* backbuffer = nullptr;
	if (FAILED(device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backbuffer))) return false;
	const HRESULT copied = D3DXLoadSurfaceFromSurface(readback, nullptr, nullptr, backbuffer,
		nullptr, nullptr, D3DX_FILTER_NONE, 0);
	backbuffer->Release();
	if (FAILED(copied)) return false;
	D3DLOCKED_RECT locked;
	if (FAILED(readback->LockRect(&locked, nullptr, D3DLOCK_READONLY))) return false;
	unsigned differentPixels = 0;
	unsigned maximumDifference = 0;
	for (int y = 0; y < WINDOW_HEIGHT; ++y) {
		const unsigned int* live = reinterpret_cast<const unsigned int*>(
			static_cast<const unsigned char*>(locked.pBits) + y * locked.Pitch);
		const unsigned int* reference = &referencePixels[y * WINDOW_WIDTH];
		for (int x = 0; x < WINDOW_WIDTH; ++x) {
			// Ignore backbuffer alpha: this fixture compares the visible RGB image.
			if ((live[x] & 0x00FFFFFFu) == (reference[x] & 0x00FFFFFFu)) continue;
			++differentPixels;
			for (int shift = 0; shift < 24; shift += 8) {
				const int difference = static_cast<int>((live[x] >> shift) & 255) - static_cast<int>((reference[x] >> shift) & 255);
				const unsigned magnitude = static_cast<unsigned>(difference < 0 ? -difference : difference);
				if (magnitude > maximumDifference) maximumDifference = magnitude;
			}
		}
	}
	if (FAILED(readback->UnlockRect())) return false;
	comparisonState = differentPixels == 0 ? COMPARISON_MATCH : COMPARISON_DIFFERENT;
	char detail[128];
	snprintf(detail, sizeof(detail), "%u / %u pixels differ, max RGB delta %u / 255",
		differentPixels, static_cast<unsigned>(WINDOW_WIDTH * WINDOW_HEIGHT), maximumDifference);
	Update_Comparison_Label(detail);
	return true;
}

// The readback is a system-memory surface, retained between comparisons.
struct ComparisonReadback
{
	ComparisonReadback() : surface(nullptr) {}
	~ComparisonReadback() { if (surface) surface->Release(); }
	IDirect3DSurface8* surface;
};

// Tool-only PS 1.1 experiment. Keep original textures (and their mip/filter
// behavior); premultiply the sampled color, rather than rewriting the assets.
// v0.a = particle alpha for OVER, zero for ADD. A zero-alpha OVER particle
// also has zero vertex RGB so that the ADD branch still emits zero.
class UnifiedParticleShader
{
public:
	UnifiedParticleShader() : handle(0) {}
	~UnifiedParticleShader()
	{
		DX8Wrapper::Set_Pixel_Shader(0);
		if (handle) DX8Wrapper::_Get_D3D_Device8()->DeletePixelShader(handle);
	}
	bool Init(std::string& error)
	{
		const char source[] =
			"ps.1.1\n"
			"def c0, 0, 0, 0, 0.5\n"
			"tex t0\n"
			"mul r1, t0, v0\n"             // original modulated RGB/alpha
			"mul r0.rgb, r1, r1.a\n"       // premultiply OVER
			"add r0.a, v0.a, c0.a\n"       // CND tests r0.a > 0.5
			"cnd r0.rgb, r0.a, r0, r1\n"   // OVER or unattenuated ADD RGB
			"mul r0.a, t0.a, v0.a\n";      // ADD outputs zero alpha
		ID3DXBuffer* bytecode = nullptr;
		ID3DXBuffer* diagnostics = nullptr;
		HRESULT result = D3DXAssembleShader(source, strlen(source), 0, nullptr, &bytecode, &diagnostics);
		if (FAILED(result)) {
			error = diagnostics ? static_cast<const char*>(diagnostics->GetBufferPointer()) : "Could not assemble particle PS 1.1 shader.";
		} else {
			result = DX8Wrapper::_Get_D3D_Device8()->CreatePixelShader(static_cast<const DWORD*>(bytecode->GetBufferPointer()), &handle);
			if (FAILED(result)) error = "The premultiplied experiment requires DirectX 8 pixel shader 1.1 support.";
		}
		if (diagnostics) diagnostics->Release();
		if (bytecode) bytecode->Release();
		return SUCCEEDED(result);
	}
	DWORD Get_Handle() const { return handle; }
private:
	DWORD handle;
};

bool Run_Scene(HWND window, const BigTextureFileFactory& archives, std::string& error)
{
	RefCountPtr<TextureClass> textures[PARTICLE_COUNT];
	WW3D::Set_Thumbnail_Enabled(false);
	for (int i = 0; i < PARTICLE_COUNT; ++i) {
		const char* name = PARTICLES[i].texture;
		if (!archives.Contains(name)) {
			error = std::string("Required texture not found in the supplied BIG archives: ") + name;
			return false;
		}
		// Check the DDS header before the general loader can fall back to TGA.
		DDSFileClass dds(name, 0);
		if (!dds.Is_Available()) {
			error = std::string("Invalid DDS texture in BIG archive: ") + name;
			return false;
		}
		DEBUG_LOG(("Particle test source: %s size=%ux%u mips=%u", name,
			dds.Get_Width(0), dds.Get_Height(0), dds.Get_Mip_Level_Count()));
		textures[i].Assign_No_Add_Ref(WW3DAssetManager::Get_Instance()->Get_Texture(name));
		if (textures[i]) textures[i]->Init();
		if (!textures[i] || !textures[i]->Is_Initialized() || textures[i]->Is_Missing_Texture()) {
			error = std::string("Could not load particle texture from BIG: ") + name;
			return false;
		}
	}

	DEBUG_LOG(("Particle test: all %d archive textures loaded", PARTICLE_COUNT));
	ParticleAtlas atlas;
	TextureClass* atlasSources[PARTICLE_COUNT];
	for (int i = 0; i < PARTICLE_COUNT; ++i) atlasSources[i] = textures[i].Peek();
	if (!atlas.Init(atlasSources, PARTICLE_COUNT, atlasError)) {
		DEBUG_LOG(("Particle atlas unavailable: %s", atlasError.c_str()));
	}
	UnifiedParticleShader unifiedShader;
	if (!unifiedShader.Init(premultipliedError)) {
		DEBUG_LOG(("Particle test: premultiplied experiment unavailable: %s", premultipliedError.c_str()));
	}
	CameraClass camera;
	camera.Set_Clip_Planes(0.1f, 100.0f);
	camera.Set_Aspect_Ratio(static_cast<float>(WINDOW_WIDTH) / WINDOW_HEIGHT);
	camera.Set_View_Plane(0.9f);
	RenderInfoClass renderInfo(camera);

	RefCountPtr<ShareBufferClass<Vector3> > positions;
	positions.Assign_No_Add_Ref(new ShareBufferClass<Vector3>(BATCH_CAPACITY, "ParticleRenderTest positions"));
	RefCountPtr<ShareBufferClass<Vector4> > colors;
	colors.Assign_No_Add_Ref(new ShareBufferClass<Vector4>(BATCH_CAPACITY, "ParticleRenderTest colors"));
	RefCountPtr<ShareBufferClass<float> > sizes;
	sizes.Assign_No_Add_Ref(new ShareBufferClass<float>(BATCH_CAPACITY, "ParticleRenderTest sizes"));
	RefCountPtr<ShareBufferClass<Vector4> > uvRects;
	uvRects.Assign_No_Add_Ref(new ShareBufferClass<Vector4>(BATCH_CAPACITY, "ParticleRenderTest UV rects"));
	std::vector<TestParticle> stressParticles;
	std::vector<unsigned> stressOrder;
	Build_Stress_Scene(stressParticles, stressOrder);
	std::vector<TestParticle> overdrawParticles;
	std::vector<unsigned> overdrawOrder;
	Build_Overdraw_Scene(overdrawParticles, overdrawOrder);
	std::vector<unsigned> atlasOrder, overdrawAtlasOrder;
	// Depth-contiguous chunks avoid interleaved nodes in the triangle sorter.
	Build_Atlas_Order(stressParticles, stressOrder, atlasOrder);
	Build_Atlas_Order(overdrawParticles, overdrawOrder, overdrawAtlasOrder);
	int activeScene = selectedScene;
	PointGroupClass group;
	group.Set_Point_Mode(PointGroupClass::QUADS);
	group.Set_Billboard(true);
	group.Set_Flag(PointGroupClass::TRANSFORM, true);
	group.Set_Point_Size(1.4f);
	group.Set_Point_Frame(0);

	LARGE_INTEGER frequency, sampleStart;
	if (!QueryPerformanceFrequency(&frequency)) {
		error = "Could not initialize the frame statistics timer.";
		return false;
	}
	QueryPerformanceCounter(&sampleStart);
	unsigned sampleFrames = 0;
	LARGE_INTEGER lastComparison = sampleStart;
	ComparisonReadback readback;
	ParticleResolutionPass resolutionPass(WINDOW_WIDTH, WINDOW_HEIGHT);
	bool activeHalfResolution = halfResolution;
	MSG message = {};
	for (;;) {
		while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
			if (message.message == WM_QUIT) return true;
			TranslateMessage(&message);
			DispatchMessage(&message);
		}
		if (IsIconic(GetAncestor(window, GA_ROOT))) {
			WaitMessage();
			QueryPerformanceCounter(&sampleStart);
			sampleFrames = 0;
			continue;
		}
		if (activeScene != selectedScene) {
			if (!Select_Scene_Reference(error)) return false;
			activeScene = selectedScene;
			displayedDraws = 0;
			displayedTriangles = 0;
			QueryPerformanceCounter(&sampleStart);
			sampleFrames = 0;
		}
		if (activeHalfResolution != halfResolution) {
			activeHalfResolution = halfResolution;
			QueryPerformanceCounter(&sampleStart);
			sampleFrames = 0;
		}
		WW3D::Enable_Sorting(sorting);
		const float background = lightBackground ? 0.35f : 0.035f;
		if (WW3D::Begin_Render(true, true, Vector3(background, background, background)) != WW3D_ERROR_OK) {
			// Begin_Render also performs device-loss recovery. Keep pumping
			// messages and retry after alt-tab rather than closing the test.
			Sleep(50);
			QueryPerformanceCounter(&sampleStart);
			sampleFrames = 0;
			continue;
		}
		const bool converted = atlasEnabled || isolatedEnabled;
		const bool transparentPass = premultiplied || converted;
		if (halfResolution && !resolutionPass.Begin(transparentPass, background, halfResolutionError)) {
			halfResolution = false;
			MessageBoxA(GetAncestor(window, GA_ROOT), halfResolutionError.c_str(), "Half-resolution rendering unavailable", MB_OK | MB_ICONERROR);
		}
		camera.Apply();
		// Pixel shader binding is immediate, outside the deferred sorting state.
		// Every submission in this tool frame uses the same experiment setting.
		DX8Wrapper::Set_Pixel_Shader(premultiplied ? unifiedShader.Get_Handle() : 0);
		group.Set_Texture_UV_Rects(atlasEnabled ? uvRects.Peek() : nullptr);
		DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAXMIPLEVEL, converted ? forcedMip : 0);
		unsigned count = 0;
		unsigned batchTexture = 0;
		bool batchAdditive = false;
		auto flushBatch = [&]() {
			if (count == 0) return;
			group.Set_Texture(atlasEnabled ? atlas.Get_Texture() : isolatedEnabled ?
				atlas.Get_Isolated(batchTexture, batchAdditive) : textures[batchTexture].Peek());
			ShaderClass shader = batchAdditive ? ShaderClass::_PresetAdditiveSpriteShader : ShaderClass::_PresetAlphaSpriteShader;
			if (premultiplied || converted) {
				shader = ShaderClass::_PresetAlphaSpriteShader;
				shader.Set_Src_Blend_Func(ShaderClass::SRCBLEND_ONE);
			}
			group.Set_Shader(shader);
			group.Set_Arrays(positions.Peek(), colors.Peek(), nullptr, sizes.Peek(), nullptr, nullptr, count);
			group.Render(renderInfo);
			count = 0;
		};
		const unsigned particleCount = Scene_Particle_Count();
		const std::vector<TestParticle>& sceneParticles = selectedScene == 2 ? overdrawParticles : stressParticles;
		const std::vector<unsigned>& sceneOrder = selectedScene == 2 ?
			(atlasEnabled ? overdrawAtlasOrder : overdrawOrder) : (atlasEnabled ? atlasOrder : stressOrder);
		for (unsigned i = 0; i < particleCount; ++i) {
			const unsigned index = reverseSubmission ? particleCount - 1 - i : i;
			const TestParticle& particle = selectedScene == 0 ? PARTICLES[index] : sceneParticles[sceneOrder[index]];
			if (count == BATCH_CAPACITY || (count > 0 &&
				(!atlasEnabled && (particle.textureIndex != batchTexture || (!premultiplied && particle.additive != batchAdditive))))) flushBatch();
			if (count == 0) {
				batchTexture = particle.textureIndex;
				batchAdditive = particle.additive;
			}
			const float distance = mipStress ? 4.0f : 1.0f;
			positions->Get_Array()[count].Set(particle.x * distance, particle.y * distance, particle.z * distance);
			// Match the alpha quantization used by PointGroup, including fades
			// that round down to zero before reaching the pixel shader.
			const bool invisibleOver = premultiplied && !particle.additive &&
				(DX8Wrapper::Convert_Color_Clamp(Vector4(0, 0, 0, particle.alpha)) & 0xFF000000u) == 0;
			// Converted alpha textures already contain texture RGB * texture alpha.
			// Vertex RGB must also carry the quantized particle alpha; ADD ignores it.
			const float vertexAlpha = static_cast<float>(DX8Wrapper::Convert_Color_Clamp(Vector4(0, 0, 0, particle.alpha)) >> 24) / 255.0f;
			const float colorScale = converted && !particle.additive ? vertexAlpha : 1.0f;
			colors->Get_Array()[count].Set(
				invisibleOver ? 0.0f : particle.red * colorScale, invisibleOver ? 0.0f : particle.green * colorScale, invisibleOver ? 0.0f : particle.blue * colorScale,
				(premultiplied || converted) && particle.additive ? 0.0f : particle.alpha);
			if (atlasEnabled) uvRects->Get_Array()[count] = atlas.Get_Rect(particle.textureIndex, particle.additive);
			sizes->Get_Array()[count] = particle.size;
			++count;
		}
		flushBatch();
		if (halfResolution) resolutionPass.Composite(transparentPass);
		const bool capturing = captureReference;
		LARGE_INTEGER comparisonTime;
		QueryPerformanceCounter(&comparisonTime);
		const bool comparing = comparisonEnabled && (comparisonRequested || capturing ||
			static_cast<double>(comparisonTime.QuadPart - lastComparison.QuadPart) / frequency.QuadPart >= 0.5);
		if (WW3D::End_Render(!(capturing || comparing)) != WW3D_ERROR_OK) return false;
		DX8Wrapper::Set_Pixel_Shader(0);
		if (capturing) {
			captureReference = false;
			const bool saved = Save_Reference(error);
			if (!saved) {
				MessageBoxA(GetAncestor(window, GA_ROOT), error.c_str(), "Particle reference", MB_OK | MB_ICONERROR);
				if (!referenceBitmap) return false;
				error.clear();
			}
		}
		if (comparing) {
			comparisonRequested = false;
			lastComparison = comparisonTime;
			if (!Compare_Reference(readback.surface)) {
				comparisonState = COMPARISON_ERROR;
				Update_Comparison_Label("could not read rendered RGB pixels");
			}
			Update_Title(window);
		}
		if (capturing || comparing) WW3D::Flip_To_Primary();
		displayedDraws = DX8Wrapper::Get_Last_Frame_Statistics().draw_calls;
		displayedTriangles = Debug_Statistics::Get_DX8_Polygons();
		static bool loggedFrame = false;
		if (!loggedFrame) {
			loggedFrame = true;
			DEBUG_LOG(("Particle test: first frame draws=%u polygons=%d sorted=%d",
				displayedDraws, displayedTriangles, Debug_Statistics::Get_Sorting_Polygons()));
		}
		++sampleFrames;
		LARGE_INTEGER sampleEnd;
		QueryPerformanceCounter(&sampleEnd);
		const double elapsed = static_cast<double>(sampleEnd.QuadPart - sampleStart.QuadPart) / frequency.QuadPart;
		if (elapsed >= 0.5) {
			displayedFps = sampleFrames / elapsed;
			displayedFrameMs = elapsed * 1000.0 / sampleFrames;
			Update_Title(window);
			sampleStart = sampleEnd;
			sampleFrames = 0;
		}
	}
}

bool Find_Archives(std::vector<std::string>& paths, std::string& error)
{
	// Load only texture archives, with Zero Hour overriding inherited sources.
	const char* candidates[] = { "Textures.big", "TexturesZH.big" };
	for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
		const DWORD attributes = GetFileAttributesA(candidates[i]);
		if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
			paths.push_back(candidates[i]);
		}
	}
	if (paths.empty()) {
		error = "No Textures.big or TexturesZH.big found in the current working directory. Run the test from the game directory or pass explicit texture archive paths.";
		return false;
	}
	return true;
}

bool Run_Renderer(HWND window, const BigTextureFileFactory& archives, std::string& error)
{
	WW3DAssetManager assets;
	if (WW3D::Init(window) != WW3D_ERROR_OK) return false;
	bool success = false;
	if (WW3D::Set_Render_Device(0, WINDOW_WIDTH, WINDOW_HEIGHT, 32, 1) == WW3D_ERROR_OK) {
		// DX8 initially sizes its HWND to the backbuffer; presentation scales
		// the full 1440p buffer into this smaller desktop preview.
		MoveWindow(window, previewWidth, LABEL_HEIGHT, previewWidth, previewHeight, TRUE);
		IDirect3DSurface8* backbuffer = nullptr;
		if (SUCCEEDED(DX8Wrapper::_Get_D3D_Device8()->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backbuffer))) {
			D3DSURFACE_DESC desc;
			backbuffer->GetDesc(&desc);
			DEBUG_LOG(("Particle test: render target %ux%u, preview %dx%d", desc.Width, desc.Height, previewWidth, previewHeight));
			backbuffer->Release();
		}
		WW3D::Enable_Static_Sort_Lists(false);
		success = Run_Scene(window, archives, error);
	}
	// Run_Scene's particles, textures and camera have been released first.
	WW3D::Shutdown();
	return success;
}
} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show)
{
	DebugSession debug;
	// Keep the adapter alive until after W3D and its loader threads shut down.
	BigTextureFileFactory archives;
	std::string error;
	std::vector<std::string> archivePaths;
	if (__argc > 1) {
		for (int i = 1; i < __argc; ++i) archivePaths.push_back(__argv[i]);
	} else if (!Find_Archives(archivePaths, error)) {
		MessageBoxA(nullptr, error.c_str(), "Particle render test", MB_OK | MB_ICONERROR);
		return 1;
	}
	for (size_t i = 0; i < archivePaths.size(); ++i) {
		if (!archives.Add_Archive(archivePaths[i].c_str(), error)) {
			MessageBoxA(nullptr, error.c_str(), "Particle render test", MB_OK | MB_ICONERROR);
			return 1;
		}
	}
	WNDCLASSA windowClass = {};
	windowClass.lpfnWndProc = Window_Proc;
	windowClass.hInstance = instance;
	windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	windowClass.lpszClassName = WINDOW_CLASS;
	if (!RegisterClassA(&windowClass)) return 1;

	const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
	RECT workArea;
	SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
	RECT frame = { 0, 0, 0, 0 };
	AdjustWindowRect(&frame, style, FALSE);
	const int availableWidth = workArea.right - workArea.left - (frame.right - frame.left);
	const int availableHeight = workArea.bottom - workArea.top - (frame.bottom - frame.top) - LABEL_HEIGHT;
	previewWidth = std::min(WINDOW_WIDTH, availableWidth / 2);
	previewHeight = previewWidth * WINDOW_HEIGHT / WINDOW_WIDTH;
	if (previewHeight > availableHeight) {
		previewHeight = availableHeight;
		previewWidth = previewHeight * WINDOW_WIDTH / WINDOW_HEIGHT;
	}
	RECT bounds = { 0, 0, previewWidth * 2, previewHeight + LABEL_HEIGHT };
	AdjustWindowRect(&bounds, style, FALSE);
	HWND window = CreateWindowA(WINDOW_CLASS, "Particle render test", style,
		CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left, bounds.bottom - bounds.top,
		nullptr, nullptr, instance, nullptr);
	if (!window) {
		UnregisterClassA(WINDOW_CLASS, instance);
		return 1;
	}
	const DWORD childStyle = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS;
	referenceWindow = CreateWindowA(WINDOW_CLASS, "Saved reference", childStyle,
		0, LABEL_HEIGHT, previewWidth, previewHeight, window, nullptr, instance, nullptr);
	HWND liveWindow = CreateWindowA(WINDOW_CLASS, "Live rendering", childStyle,
		previewWidth, LABEL_HEIGHT, previewWidth, previewHeight, window, nullptr, instance, nullptr);
	referenceLabel = CreateWindowA("STATIC", "SAVED REFERENCE  (C replaces it)", WS_CHILD | WS_VISIBLE | SS_CENTER,
		0, 0, previewWidth, LABEL_HEIGHT, window, nullptr, instance, nullptr);
	liveLabel = CreateWindowA("STATIC", "LIVE RENDERING", WS_CHILD | WS_VISIBLE | SS_CENTER,
		previewWidth, 0, previewWidth, LABEL_HEIGHT, window, nullptr, instance, nullptr);
	char executable[MAX_PATH];
	const DWORD pathLength = GetModuleFileNameA(nullptr, executable, sizeof(executable));
	bool success = referenceWindow && liveWindow && pathLength > 0 && pathLength < sizeof(executable);
	if (success) {
		const char* separator = strrchr(executable, '\\');
		referenceDirectory.assign(executable, separator ? separator - executable + 1 : 0);
		success = Select_Scene_Reference(error);
	}
	Update_Title(window);
	ShowWindow(window, show);
	if (success) {
		WWMath::Init();
		success = Run_Renderer(liveWindow, archives, error);
		WWMath::Shutdown();
	}
	if (!success) {
		MessageBoxA(window, error.empty() ? "Could not initialize or render the particle comparison." : error.c_str(),
			"Particle render test", MB_OK | MB_ICONERROR);
	}
	DestroyWindow(window);
	if (referenceBitmap) DeleteObject(referenceBitmap);
	UnregisterClassA(WINDOW_CLASS, instance);
	return success ? 0 : 1;
}
