# Particle render test

A hardcoded scene shown in two 960×720 panes (saved reference left, live rendering right) with ten overlapping particles: five alpha and five
additive, at distinct depths. Uses the game's `PointGroupClass`, sprite shader
presets, camera, and `SortingRendererClass`. Particle simulation, INI parsing and
the game particle manager are not needed for this geometry/blending test.

Each particle uses a different real DDS texture read directly from a supplied
`.big` archive, with its mipmaps and the existing separate blend modes. The scene
needs no INI files. P toggles an experimental unified premultiplied-alpha mode.
There is no texture atlas yet.

## Assets and launch

With no arguments, the test checks only `Textures.big` and `TexturesZH.big`
in the **current working directory** (not necessarily the executable directory).
It loads whichever are present, with `TexturesZH.big` overriding duplicate entries
from `Textures.big`. It does not scan audio, maps, language or other archives.
Some scene textures are inherited from Generals, so `TexturesZH.big` alone may
not contain all ten required textures.
For example:

```bat
cd /d D:\generalsv-weekly
D:\path\to\ParticleRenderTestZH.exe
```

Alternatively, pass one or more explicit archive paths, quoted when they contain
spaces. Explicit arguments replace automatic discovery and retain their command-line
order; later arguments override earlier ones. For example:

```bat
ParticleRenderTestZH.exe "D:\generalsv-weekly\Textures.big" "D:\generalszh-weekly\TexturesZH.big"
```

These ten sources were all found in Generals' `Textures.big`, so that archive alone
is sufficient for this scene. A Zero Hour-only `TexturesZH.big` does not contain
all inherited Generals textures. Use the executable's configuration suffix if set.

| Alpha particles | Additive particles |
|---|---|
| `EXSmokNew1.dds` | `EXexplo03.dds` |
| `EXSmokePuf07.dds` | `EXFire01.dds` |
| `EXCloud01.dds` | `EXLnzFlar2.dds` |
| `EXCloud02.dds` | `EXFireball03.dds` |
| `EXWater04.dds` | `EXSpark01.dds` |

Entries are under `Art\Textures` inside the archives. Missing entries or failed
texture decoding produce an error instead of silently displaying a placeholder.
The mode assignments above define this test scene; they are not loaded from the
original systems' INI definitions.

`BigTextureFileFactory` is a tool-only adapter for `BIGF`/`BIG4` archives. It
indexes their directories and returns bounded `RawFileClass`-derived views of each entry
to the existing W3D texture loader. Each reader has its own handle; the archives
are read directly without disk extraction. Offsets and sizes must fit the
engine's signed 32-bit file interface. The adapter implements entry-relative seeking
locally because this checkout's raw-file seek returns status instead of position.
DDS headers are validated before loading, so an invalid entry cannot silently
fall through to TGA decoding. This is explicit archive precedence,
not a recreation of the game's full filesystem/mod discovery rules.

## Build

Enable `RTS_BUILD_ZEROHOUR_TOOLS` and/or `RTS_BUILD_GENERALS_TOOLS` in a Windows
configuration. Targets:

- `z_particlerendertest`: `ParticleRenderTestZH` (plus configuration suffix).
- `g_particlerendertest`: `ParticleRenderTestV` (plus configuration suffix).

For example, from an x86 Visual Studio developer prompt:

```bat
cmake --preset win32-profile
cmake --build --preset win32-profile --target z_particlerendertest g_particlerendertest
```

The normal tool dependencies, DirectX 8 runtime and supplied game archives are required. No MFC UI is
used by this executable. The window has a fixed client size; it can be minimized.

## Scenes

- **1**: the original ten overlapping particles.
- **2**: 4,000 static randomized particles, using seed **`0x5EED1234`**.

The stress scene randomizes positions, sizes, colors, alpha, blend modes and the
choice among the ten loaded textures. Its explicit xorshift32 generator runs once
at startup; frame rate and scene switching do not consume random values. Depths
are stratified with seeded jitter to avoid equal-depth ordering ambiguity.

Stress-scene submissions are grouped into texture/blend batches of up to 512
particles, then the production triangle sorter orders all particles together.
This avoids exhausting the sorter's 4,096-node table. With sorting disabled,
rendering follows batch order. The 4,000 quads generate 16,000 vertices and 8,000
triangles. Both scenes use the same existing texture-loading and rendering paths.

## Saved reference

The comparison window displays a frozen screenshot on the left and the live scene
on the right, at identical resolutions. The reference is stored as
`ParticleRenderTest-reference.bmp` next to the executable and survives restarts
and code edits. Scene 2 uses its own file,
`ParticleRenderTest-reference-4000-5EED1234.bmp`, in the same directory.
Switching scenes loads the corresponding reference. If its file is absent, the first rendered frame becomes the
reference. An existing reference is never replaced automatically.

The live label and title report **MATCH** or **DIFFERENT**, the number of differing
pixels, and the largest RGB channel difference (0–255). Equality is exact across
all 960×720 RGB pixels; alpha is ignored. The check reads the application's GPU
backbuffer twice per second, before presentation. It excludes labels, window
borders and desktop composition. Readback failure reports **CHECK FAILED**.

**V** pauses/resumes comparison for performance measurements. GPU readback can
stall rendering; FPS includes this cost while checking is enabled. The left
reference remains displayed when checking is paused.

**C** explicitly replaces the reference with the current live frame. Sorting,
submission order and background controls affect only the live pane until C is
pressed. The capture reads the test's own GPU backbuffer, not the desktop.
The reference uses a separate Windows bitmap panel, adding no GPU draw calls to
the particle statistics. FPS includes the comparison application's normal overhead.

## Controls and visual checks

- **1** / **2** switch between the ten-particle scene and the fixed-seed 4,000-particle scene.
- **V** toggles live RGB comparison (initially on).
- **C** saves the current live image as the new persistent reference.
- **P** toggles unified premultiplied-alpha blending (initially off).
- **A** toggles Ronin's additive-run batching (initially on) for draw-count comparisons.
- **F** toggles the frame limiter (initially on); turn it off for throughput comparisons.
- **S** toggles production triangle sorting (initially on).
- **R** reverses particle submission order.
- **B** switches between dark and gray backgrounds.
- **Esc** closes the test.

With sorting on, reversing submission should preserve the image: all particles
have distinct depths. With sorting off, reversal should visibly change overlapping
alpha/additive results. Keep the camera and particles static for reproducible
comparisons. The title bar shows FPS, average frame time, actual DX8 draw calls per frame,
triangle count, and the current controls. FPS/frame time update twice per second
and include presentation and the optional 16 ms sleep. Minimized/device-recovery
periods reset the sampling interval. Uncapped mode removes the sleep; presentation
can still be limited by the driver or display synchronization.

Draw calls come from `DX8Wrapper::Get_Last_Frame_Statistics().draw_calls` after
`End_Render`, rather than the general debug counter that also counts sorting
submissions.

The original scene makes ten separate point-group submissions so the sorter
receives multiple interleaving nodes. This is a small correctness fixture with basic rendering statistics, not a
representative performance benchmark. It is a starting point for comparing the existing blend modes
with the experimental unified blend implementation.

## Diagnostic log

Logging-enabled builds write `ParticleRenderTest-DebugLogFile*.txt` next to the
executable, recording texture dimensions/mip counts and first-frame draw counts.
Debug initialization and shutdown bracket the renderer and archive adapter.

## Additive sorting optimization

The sorter ports Ronin's additive-run regrouping (`d4578f76f`, with the independent
correctness predicate from `28ddcc23b`). After depth sorting, each uninterrupted
run of additive triangles is stably grouped by original node ID. Alpha, multiply
and depth-writing nodes remain barriers. Both `ONE/ONE` and `SRC_ALPHA/ONE` are
eligible when depth writes are disabled. Shader classification does not depend
on diagnostic draw categories. Press A to compare against pure depth order.

This reduces repeated node switches; it does not merge different nodes by render
state, create an atlas, or combine alpha and additive into a shared blend mode.
Compare against the existing saved images without recapturing them. Small RGB
rounding differences, if any, remain visible in the exact-pixel comparison.

## Unified premultiplied-alpha experiment

Press **P** to switch the live scene between the original blend modes and a
shared `ONE / INV_SRC_ALPHA` mode. The saved reference is unchanged. This path
is confined to this test executable; game particle materials are unchanged.
It requires DX8 pixel shader 1.1 support. If unavailable, P reports the failure
and the original rendering path remains usable.

Let `C = texture.rgb * vertex.rgb` and `a = texture.a * vertex.a`:

| Particle | Original RGB blend | Experiment shader output `(RGB, alpha)` | Shared blend result |
|---|---|---|---|
| Alpha | `C*a + destination*(1-a)` | `(C*a, a)` | `C*a + destination*(1-a)` |
| Additive | `C + destination` | `(C, 0)` | `C + destination` |

For the experiment, additive vertices carry zero alpha; their original alpha
was ignored by the original `ONE / ONE` material. Zero-alpha OVER vertices also
carry zero RGB to distinguish an invisible particle from additive emission.
The PS 1.1 program premultiplies after sampling the original texture, preserving
the existing mipmaps and texture-filtering behavior without texture conversion.
This tests blending equivalence, rather than prefiltered premultiplied assets.
The backbuffer alpha channel is outside this RGB comparison.

Both modes now share one shader and blend state. Submission batches split only
at texture changes and the 512-particle capacity. The sorter still preserves
depth order and emits draws on node changes. Unified OVER is not commutative:
Ronin's additive regrouping does not classify the shared material as additive,
so A has no regrouping effect while P is enabled. Mixed-mode batching can thus
save some draws while losing the original additive-run regrouping opportunity.
Texture changes and capacity boundaries still limit batching; this experiment
does not promise one draw for all particles.

Compare scene 2 with P off/on, keeping S enabled. Use V and F to disable readback
and the sleep limiter for throughput measurements. Exact RGB comparison stays
strict: finite-precision differences between the fixed-function and programmable
paths are displayed as DIFFERENT, including the maximum channel delta.

Initial bequiet measurements for the fixed-seed 4,000-particle scene:

| Path | GPU draws/frame |
|---|---:|
| Original modes, additive regrouping off | 3,809 |
| Original modes, additive regrouping on | 3,713 |
| Unified premultiplied mode | 3,597 |

That is 116 fewer draws (3.1%) than the Ronin path, or 5.6% below pure depth
sorting. Scene 1 still needs ten draws because each particle has a different
texture. The initial unified stress-scene comparison differed in 161,448 of
691,200 pixels, with a maximum RGB delta of 3/255. The programmable and
fixed-function paths are therefore not bit-identical; small precision changes
can accumulate through overlapping particles. References were preserved and the
original path still matches exactly. Brief uncapped samples fluctuated enough
that they do not establish a reliable FPS improvement.

PS 1.1 conditional selection follows [Microsoft's CND instruction documentation](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/cnd---ps).
