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
#include "WW3D2/pointgr.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/texture.h"
#include "WW3D2/ww3d.h"
#include "WWMath/wwmath.h"
#include "BigTextureFileFactory.h"
#include <d3d8.h>
#include <d3dx8.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

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

const int WINDOW_WIDTH = 960;
const int WINDOW_HEIGHT = 720;
const int LABEL_HEIGHT = 24;
HWND referenceWindow = nullptr;
HWND liveLabel = nullptr;
std::vector<unsigned int> referencePixels;
enum ComparisonState { COMPARISON_PENDING, COMPARISON_MATCH, COMPARISON_DIFFERENT, COMPARISON_ERROR, COMPARISON_PAUSED };
ComparisonState comparisonState = COMPARISON_PENDING;
bool comparisonEnabled = true;
bool comparisonRequested = true;
HBITMAP referenceBitmap = nullptr;
std::string referencePath;
bool captureReference = false;
const char WINDOW_CLASS[] = "GeneralsParticleRenderTest";

bool sorting = true;
bool reverseSubmission = false;
bool lightBackground = false;
bool frameLimiter = true;
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
};

// Distinct depths and alternating modes make additive/alpha ordering visible.
// The deliberately scrambled submission order exercises the real triangle sorter.
const TestParticle PARTICLES[] = {
	{ -0.9f,  0.4f, -8.0f, 0.90f, 0.15f, 0.10f, 0.65f, false, "Art/Textures/EXSmokNew1.dds" },
	{  0.7f,  0.2f, -6.0f, 0.10f, 0.35f, 0.80f, 1.00f, true, "Art/Textures/EXexplo03.dds" },
	{  0.2f, -0.5f, -9.0f, 0.10f, 0.75f, 0.30f, 0.65f, false, "Art/Textures/EXSmokePuf07.dds" },
	{ -0.5f, -0.2f, -5.0f, 0.65f, 0.20f, 0.05f, 1.00f, true, "Art/Textures/EXFire01.dds" },
	{  0.8f,  0.5f, -7.0f, 0.50f, 0.15f, 0.75f, 0.65f, false, "Art/Textures/EXCloud01.dds" },
	{ -0.1f,  0.6f, -9.5f, 0.10f, 0.40f, 0.65f, 1.00f, true, "Art/Textures/EXLnzFlar2.dds" },
	{ -0.6f, -0.6f, -5.5f, 0.10f, 0.55f, 0.65f, 0.65f, false, "Art/Textures/EXCloud02.dds" },
	{  0.4f, -0.3f, -7.5f, 0.55f, 0.35f, 0.05f, 1.00f, true, "Art/Textures/EXFireball03.dds" },
	{  0.0f,  0.0f, -4.5f, 0.75f, 0.20f, 0.40f, 0.55f, false, "Art/Textures/EXWater04.dds" },
	{ -0.3f,  0.0f, -6.5f, 0.15f, 0.55f, 0.10f, 1.00f, true, "Art/Textures/EXSpark01.dds" },
};
const int PARTICLE_COUNT = sizeof(PARTICLES) / sizeof(PARTICLES[0]);

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
	char title[256];
	snprintf(title, sizeof(title),
		"Particle test | %s | %.1f FPS | %.2f ms | %u draws | %d triangles | F: %s | S: sorting %s | R: %s | B: bg | C: capture | V: compare | Esc",
		Comparison_Name(), displayedFps, displayedFrameMs, displayedDraws, displayedTriangles,
		frameLimiter ? "limited" : "uncapped", sorting ? "ON" : "OFF", reverseSubmission ? "reversed" : "original");
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
		case 'F': frameLimiter = !frameLimiter; break;
		case 'C': captureReference = true; break;
		case 'V': comparisonEnabled = !comparisonEnabled; break;
		}
		if (wparam == 'S' || wparam == 'R' || wparam == 'B' || wparam == 'C' || wparam == 'V') {
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
				BitBlt(dc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, memory, 0, 0, SRCCOPY);
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
		error = std::string("Could not load a 960x720 reference bitmap: ") + path;
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
	CameraClass camera;
	camera.Set_Clip_Planes(0.1f, 100.0f);
	camera.Set_Aspect_Ratio(static_cast<float>(WINDOW_WIDTH) / WINDOW_HEIGHT);
	camera.Set_View_Plane(0.9f);
	RenderInfoClass renderInfo(camera);

	RefCountPtr<ShareBufferClass<Vector3> > positions;
	positions.Assign_No_Add_Ref(new ShareBufferClass<Vector3>(1, "ParticleRenderTest positions"));
	PointGroupClass group;
	group.Set_Point_Mode(PointGroupClass::QUADS);
	group.Set_Billboard(true);
	group.Set_Flag(PointGroupClass::TRANSFORM, true);
	group.Set_Point_Size(1.4f);
	group.Set_Point_Frame(0);
	group.Set_Arrays(positions.Peek(), nullptr, nullptr, nullptr, nullptr, nullptr, 1);

	LARGE_INTEGER frequency, sampleStart;
	if (!QueryPerformanceFrequency(&frequency)) {
		error = "Could not initialize the frame statistics timer.";
		return false;
	}
	QueryPerformanceCounter(&sampleStart);
	unsigned sampleFrames = 0;
	LARGE_INTEGER lastComparison = sampleStart;
	ComparisonReadback readback;
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
		camera.Apply();
		for (int i = 0; i < PARTICLE_COUNT; ++i) {
			const int index = reverseSubmission ? PARTICLE_COUNT - 1 - i : i;
			const TestParticle& particle = PARTICLES[index];
			group.Set_Texture(textures[index].Peek());
			positions->Get_Array()[0].Set(particle.x, particle.y, particle.z);
			group.Set_Point_Color(Vector3(particle.red, particle.green, particle.blue));
			group.Set_Point_Alpha(particle.alpha);
			group.Set_Shader(particle.additive ? ShaderClass::_PresetAdditiveSpriteShader
				: ShaderClass::_PresetAlphaSpriteShader);
			// Each call captures one node; End_Render flushes all ten together.
			group.Render(renderInfo);
		}
		const bool capturing = captureReference;
		LARGE_INTEGER comparisonTime;
		QueryPerformanceCounter(&comparisonTime);
		const bool comparing = comparisonEnabled && (comparisonRequested || capturing ||
			static_cast<double>(comparisonTime.QuadPart - lastComparison.QuadPart) / frequency.QuadPart >= 0.5);
		if (WW3D::End_Render(!(capturing || comparing)) != WW3D_ERROR_OK) return false;
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
		if (frameLimiter) Sleep(16);
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
	RECT bounds = { 0, 0, WINDOW_WIDTH * 2, WINDOW_HEIGHT + LABEL_HEIGHT };
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
		0, LABEL_HEIGHT, WINDOW_WIDTH, WINDOW_HEIGHT, window, nullptr, instance, nullptr);
	HWND liveWindow = CreateWindowA(WINDOW_CLASS, "Live rendering", childStyle,
		WINDOW_WIDTH, LABEL_HEIGHT, WINDOW_WIDTH, WINDOW_HEIGHT, window, nullptr, instance, nullptr);
	CreateWindowA("STATIC", "SAVED REFERENCE  (C replaces it)", WS_CHILD | WS_VISIBLE | SS_CENTER,
		0, 0, WINDOW_WIDTH, LABEL_HEIGHT, window, nullptr, instance, nullptr);
	liveLabel = CreateWindowA("STATIC", "LIVE RENDERING", WS_CHILD | WS_VISIBLE | SS_CENTER,
		WINDOW_WIDTH, 0, WINDOW_WIDTH, LABEL_HEIGHT, window, nullptr, instance, nullptr);
	char executable[MAX_PATH];
	const DWORD pathLength = GetModuleFileNameA(nullptr, executable, sizeof(executable));
	bool success = referenceWindow && liveWindow && pathLength > 0 && pathLength < sizeof(executable);
	if (success) {
		const char* separator = strrchr(executable, '\\');
		referencePath.assign(executable, separator ? separator - executable + 1 : 0);
		referencePath += "ParticleRenderTest-reference.bmp";
		const DWORD attributes = GetFileAttributesA(referencePath.c_str());
		if (attributes == INVALID_FILE_ATTRIBUTES) {
			captureReference = true;
		} else {
			success = Load_Reference(referencePath.c_str(), referenceBitmap, referencePixels, error);
		}
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
