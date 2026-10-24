// Shared D3D8 smoke test: render, read back, validate and write a BMP.
#include <windows.h>
#include <d3d8.h>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <vector>

#include "d3d8_probe.h"

static void Check(HRESULT result, const char *operation)
{
    if (FAILED(result)) {
        char message[256];
        std::snprintf(message, sizeof(message), "%s failed: 0x%08lx", operation, static_cast<unsigned long>(result));
        throw std::runtime_error(message);
    }
}

template<class T> struct ReleaseCom
{
    void operator()(T *object) const { if (object) object->Release(); }
};

bool RunD3D8Probe(const char *output)
{
    const unsigned width = 256, height = 256;
    HWND window = CreateWindowA("STATIC", "D3D8 probe", WS_OVERLAPPEDWINDOW,
        0, 0, width, height, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!window) throw std::runtime_error("CreateWindow failed");
    std::unique_ptr<HWND__, decltype(&DestroyWindow)> windowOwner(window, DestroyWindow);
    IDirect3D8 *d3d = Direct3DCreate8(D3D_SDK_VERSION);
    if (!d3d) throw std::runtime_error("Direct3DCreate8 failed");
    std::unique_ptr<IDirect3D8, ReleaseCom<IDirect3D8>> d3dOwner(d3d);
    D3DADAPTER_IDENTIFIER8 adapter = {};
    Check(d3d->GetAdapterIdentifier(0, 0, &adapter), "GetAdapterIdentifier");
    std::printf("Adapter: %s\n", adapter.Description);
    D3DPRESENT_PARAMETERS params = {};
    params.BackBufferWidth = width;
    params.BackBufferHeight = height;
    params.BackBufferFormat = D3DFMT_X8R8G8B8;
    params.BackBufferCount = 1;
    params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    params.hDeviceWindow = window;
    params.Windowed = TRUE;
    IDirect3DDevice8 *device = nullptr;
    Check(d3d->CreateDevice(0, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &params, &device), "CreateDevice");
    std::unique_ptr<IDirect3DDevice8, ReleaseCom<IDirect3DDevice8>> deviceOwner(device);
    IDirect3DSurface8 *target = nullptr, *readback = nullptr;
    Check(device->CreateRenderTarget(width, height, D3DFMT_X8R8G8B8,
        D3DMULTISAMPLE_NONE, FALSE, &target), "CreateRenderTarget");
    std::unique_ptr<IDirect3DSurface8, ReleaseCom<IDirect3DSurface8>> targetOwner(target);
    Check(device->SetRenderTarget(target, nullptr), "SetRenderTarget");
    Check(device->Clear(0, nullptr, D3DCLEAR_TARGET, 0xff102030, 1.0f, 0), "Clear");
    Check(device->SetRenderState(D3DRS_LIGHTING, FALSE), "Disable lighting");
    Check(device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE), "Disable culling");
    Check(device->SetRenderState(D3DRS_ZENABLE, FALSE), "Disable depth");
    Check(device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1), "Color op");
    Check(device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE), "Color arg");
    struct Vertex { float x, y, z, rhw; DWORD color; };
    const Vertex vertices[] = {
        {136, 32, 0.5f, 1, 0xffff0000},
        {224, 224, 0.5f, 1, 0xff00ff00},
        {32, 224, 0.5f, 1, 0xff0000ff}
    };
    Check(device->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE), "SetVertexShader");
    Check(device->BeginScene(), "BeginScene");
    Check(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, vertices, sizeof(Vertex)), "DrawPrimitiveUP");
    Check(device->EndScene(), "EndScene");
    Check(device->CreateImageSurface(width, height, D3DFMT_X8R8G8B8, &readback), "CreateImageSurface");
    std::unique_ptr<IDirect3DSurface8, ReleaseCom<IDirect3DSurface8>> readbackOwner(readback);
    Check(device->CopyRects(target, nullptr, 0, readback, nullptr), "CopyRects");
    D3DLOCKED_RECT locked = {};
    Check(readback->LockRect(&locked, nullptr, D3DLOCK_READONLY), "LockRect");
    std::vector<DWORD> pixels(width * height);
    for (unsigned y = 0; y < height; ++y) {
        const DWORD *row = reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(locked.pBits) + y * locked.Pitch);
        for (unsigned x = 0; x < width; ++x) pixels[y * width + x] = row[x] & 0x00ffffff;
    }
    Check(readback->UnlockRect(), "UnlockRect");
    BITMAPFILEHEADER header = {};
    BITMAPINFOHEADER info = {};
    header.bfType = 0x4d42;
    header.bfOffBits = sizeof(header) + sizeof(info);
    header.bfSize = header.bfOffBits + width * height * 4;
    info.biSize = sizeof(info);
    info.biWidth = width;
    info.biHeight = -static_cast<LONG>(height); // top-down rows
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biCompression = BI_RGB;
    FILE *file = std::fopen(output, "wb");
    if (!file) throw std::runtime_error("Cannot open output BMP");
    bool saved = std::fwrite(&header, sizeof(header), 1, file) == 1;
    saved = (std::fwrite(&info, sizeof(info), 1, file) == 1) && saved;
    saved = (std::fwrite(pixels.data(), width * height * 4, 1, file) == 1) && saved;
    saved = (std::fclose(file) == 0) && saved;
    // Sample well away from edges; allow interpolation rounding differences.
    DWORD center = pixels[160 * width + 128];
    unsigned r = (center >> 16) & 255, g = (center >> 8) & 255, b = center & 255;
    bool valid = pixels[8 * width + 8] == 0x102030 &&
        r >= 75 && r <= 95 && g >= 75 && g <= 95 && b >= 75 && b <= 95;
    std::printf("Center RGB: %u %u %u; image: %s; %s\n", r, g, b, output,
        saved && valid ? "PASS" : "FAIL");
    return saved && valid;
}
