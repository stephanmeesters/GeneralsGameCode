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

void Update_Title(HWND window)
{
	char title[256];
	snprintf(title, sizeof(title),
		"Particle test | %.1f FPS | %.2f ms | %u draws | %d triangles | F: %s | S: sorting %s | R: %s | B: bg | Esc",
		displayedFps, displayedFrameMs, displayedDraws, displayedTriangles,
		frameLimiter ? "limited" : "uncapped", sorting ? "ON" : "OFF", reverseSubmission ? "reversed" : "original");
	SetWindowTextA(window, title);
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
		}
		Update_Title(window);
		return 0;
	case WM_CLOSE:
		// Keep the renderer's HWND alive until graphics resources are released.
		PostQuitMessage(0);
		return 0;
	case WM_ERASEBKGND:
		return 1;
	}
	return DefWindowProcA(window, message, wparam, lparam);
}

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
	MSG message = {};
	for (;;) {
		while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
			if (message.message == WM_QUIT) return true;
			TranslateMessage(&message);
			DispatchMessage(&message);
		}
		if (IsIconic(window)) {
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
		if (WW3D::End_Render() != WW3D_ERROR_OK) return false;
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

	const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	RECT bounds = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
	AdjustWindowRect(&bounds, style, FALSE);
	HWND window = CreateWindowA(WINDOW_CLASS, "Particle render test", style,
		CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left, bounds.bottom - bounds.top,
		nullptr, nullptr, instance, nullptr);
	if (!window) {
		UnregisterClassA(WINDOW_CLASS, instance);
		return 1;
	}
	Update_Title(window);
	ShowWindow(window, show);
	WWMath::Init();
	const bool success = Run_Renderer(window, archives, error);
	WWMath::Shutdown();
	if (!success) {
		MessageBoxA(window, error.empty() ? "Could not initialize or render the DX8 particle test." : error.c_str(),
			"Particle render test", MB_OK | MB_ICONERROR);
	}
	DestroyWindow(window);
	UnregisterClassA(WINDOW_CLASS, instance);
	return success ? 0 : 1;
}
