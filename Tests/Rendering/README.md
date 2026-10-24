# Direct3D 8 software rendering probe

Shared probe used by the standalone tool and the `Rendering.D3D8Triangle` Google
Test in both `g_googletest` and `z_googletest`. It creates a
real D3D8 device, draws an RGB triangle into a 256x256 offscreen render target,
reads it back, saves `triangle.bmp`, and exits with status 0 only if image writing
and background/triangle pixel checks pass. It needs no game assets or D3DX.
It exercises D3D8 device creation and readback, without initializing the game renderer.

On Linux, the rendering path is Wine -> DXVK D3D8/D3D9 -> Vulkan -> Mesa lavapipe.
Xvfb supplies a virtual display, so a physical display/GPU is unnecessary.

Prerequisites: CMake, i686-w64-mingw32-g++, Wine (with 32-bit Windows application
support), Xvfb/xvfb-run, xauth, and Mesa lavapipe/Vulkan loader. Older Wine setups
may also require 32-bit Vulkan/Mesa packages. Extract an official DXVK release
containing `x32/d3d8.dll` and `x32/d3d9.dll` first:

```sh
curl -fL -o /tmp/dxvk-2.6.2.tar.gz https://github.com/doitsujin/dxvk/releases/download/v2.6.2/dxvk-2.6.2.tar.gz
tar -C /tmp -xf /tmp/dxvk-2.6.2.tar.gz
Tests/Rendering/run-wine.sh /tmp/render-probe /tmp/dxvk-2.6.2
```

The script creates an isolated Wine prefix in the output directory, forces the
lavapipe ICD, selects llvmpipe in DXVK, and checks the DXVK log for llvmpipe.
Set `LAVAPIPE_ICD` if your distribution uses a different ICD filename.
Use a fresh output directory if Wine prefix initialization was interrupted.

On this machine, run inside the existing toolbox:

```sh
podman exec fedora-toolbox-43 /var/home/stephan/Dev/GeneralsGameCode/Tests/Rendering/run-wine.sh /tmp/render-probe /tmp/dxvk-2.6.2
```

Verified locally with Wine 11.0, DXVK 2.6.2 and Mesa llvmpipe 25.3.6
(LLVM 21.1.8), using the 32-bit executable and a 64-bit lavapipe ICD through
Wine's WoW64 support. Center pixel: RGB 85,85,85; background: RGB 16,32,48.
DXVK reports an emulated AMD adapter name to the application; its Vulkan device
log identifies the actual llvmpipe device.

## MSVC Google Tests in CI

The reusable build workflow packages each game's plain `win32` Release Google
Test executable with the MSVC x86 Release runtime DLLs. Its dependent
`Rendering with Wine and lavapipe` job on Ubuntu 24.04 downloads that package,
installs Wine and both architectures of Mesa/Vulkan, and runs only `Rendering.*`.
There is no Linux rebuild of the executable. Profile, Debug and vcpkg builds
continue to run their ordinary unit tests on Windows.

CTest excludes `Rendering.*` on every platform; direct invocation of a Google
Test executable can opt in with `--gtest_filter=Rendering.*`. Ordinary tests remain
selected with `--gtest_filter=-Rendering.*`.

To run a packaged MSVC executable locally inside the toolbox:

```sh
Tests/Rendering/run-google-wine.sh /absolute/path/to/z_googletest.exe /tmp/dxvk-2.6.2
```

Run this as the owner of the executable's directory. The script creates an
isolated prefix there, copies the DXVK DLLs beside the executable, and writes
`triangle.bmp`, `results.xml`, `rendering.log`, and DXVK logs there. It fails if
no rendering tests run, a test is skipped/fails, or DXVK does not report llvmpipe.
CI uploads these outputs on success and failure. Add future device tests to the
`Rendering` suite so Windows CTest excludes them and the Linux job includes them.

DXVK 2.6.2 is pinned and its download is checked against SHA256. Wine and Mesa
come from the Ubuntu 24.04 package repositories; these can receive updates.
