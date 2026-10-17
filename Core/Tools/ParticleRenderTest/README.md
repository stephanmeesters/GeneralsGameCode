# Particle render test

Three hardcoded scenes rendered at **2560×1440** (16:9), with a saved reference
on the left and live rendering on the right. Both preview panes are scaled to
fit the desktop; the backbuffer, reference capture, and pixel comparisons remain
full 1440p. The original scene has ten overlapping particles: five alpha and
five additive, at distinct depths. Uses the game's `PointGroupClass`, sprite shader
presets, camera, and `SortingRendererClass`. Particle simulation, INI parsing and
the game particle manager are not needed for this geometry/blending test.

Each particle uses a different real DDS texture read directly from a supplied
`.big` archive, with its mipmaps and the existing separate blend modes. The scene
needs no INI files. P toggles an experimental unified premultiplied-alpha mode.
T toggles the startup-built premultiplied texture atlas described below.

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
used by this executable. The comparison window is sized to fit the desktop work area at launch and can be minimized.

## Scenes

- **1**: the original ten overlapping particles.
- **2**: 4,000 static randomized particles, using seed **`0x5EED1234`**.
- **3**: 100 screen-filling particles, using seed **`0x0D12A100`**, for heavy overdraw.

The stress scene randomizes positions, sizes, colors, alpha, blend modes and the
choice among the ten loaded textures. Its explicit xorshift32 generator runs once
at startup; frame rate and scene switching do not consume random values. Depths
are stratified with seeded jitter to avoid equal-depth ordering ambiguity.

Stress-scene submissions are grouped into texture/blend batches of up to 512
particles, then the production triangle sorter orders all particles together.
This avoids exhausting the sorter's 4,096-node table. With sorting disabled,
rendering follows batch order. The 4,000 quads generate 16,000 vertices and 8,000
triangles. All three scenes use the same existing texture-loading and rendering paths.

Scene 3 deliberately covers the viewport with all 100 quads, using the same ten
archive textures in both alpha and additive modes. Positions, sizes, tint and
opacity are deterministic; depths are distinct. Low alpha and dim additive tint
keep the composite visible instead of saturating to white. With M off, this is
roughly 100 blended layers per pixel, or 369 million fragment samples per frame
at 2560×1440, despite only 200 submitted triangles. It is intended to expose
fill-rate, sampling and blend-bandwidth limits rather than draw-call overhead.
It uses normal 512-particle batching; the atlas scene fits in one draw.

## Saved reference

The comparison window displays a frozen screenshot on the left and the live scene
on the right. Both are rendered/captured at 2560×1440 and scaled into matching
preview panes. Each scene has a persistent reference next to the executable:

- `ParticleRenderTest-reference-10-2560x1440.bmp`
- `ParticleRenderTest-reference-4000-5EED1234-2560x1440.bmp`
- `ParticleRenderTest-reference-overdraw-100-0D12A100-2560x1440.bmp`

The older 960×720 reference files are left untouched; the resolution and aspect
ratio change requires new references. Historical FPS/image comparisons below
were recorded at 960×720 and do not describe the new 1440p workload.

Switching scenes loads the corresponding reference. If its file is absent, the first rendered frame becomes the
reference. An existing reference is never replaced automatically.

The live label and title report **MATCH** or **DIFFERENT**, the number of differing
pixels, and the largest RGB channel difference (0–255). Equality is exact across
all 2560×1440 RGB pixels (3,686,400 pixels); alpha is ignored. The check reads the application's GPU
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

- **1** / **2** / **3** select ten particles, 4,000 small particles, or 100 screen-filling particles.
- **V** toggles live RGB comparison (initially on).
- **C** saves the current live image as the new persistent reference.
- **T** toggles the premultiplied texture atlas (initially off).
- **I** toggles isolated converted textures, for checking the atlas against the same mip data.
- **M** moves particles four times farther away, keeping their screen centers, to stress minification.
- **H** switches bilinear/trilinear filtering (initially bilinear).
- **L** cycles the minimum sampled mip from 0 through 6 for T/I (0 restores automatic LOD).
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

Historical bequiet measurements at 960×720 for the fixed-seed 4,000-particle scene:

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

## Premultiplied texture atlas experiment

**T** enables a startup-built atlas, using the existing `PointGroupClass` and
production sorting renderer. **I** instead uses separate converted textures
containing exactly the same pixels and mip chains. T, I and P are mutually
exclusive. Toggle the active mode again to return to the original path. The
original saved references remain unchanged; C still explicitly replaces them.

For the ten current sources the atlas is **2048 x 1024 ARGB8**, with **20 entries**
and **7 mip levels** (about 10.67 MiB of texel data, excluding driver overhead).
Each source has two independent entries, even if both modes use the same source:

- Alpha: RGB is multiplied by source alpha, with source alpha retained.
- Additive: source RGB is retained, with alpha set to zero.

Entries are decoded from the already-loaded source texture at its native
resolution. Conversion and uploads happen once at startup. No atlas work or
texture uploads occur per frame. This is a static atlas experiment, not an
allocator with dynamic insertion, eviction or repacking. Device size/allocation
failures leave the original rendering available and report the reason on T/I.

Both entries use `ONE / INV_SRC_ALPHA` with the existing fixed-function texture
modulation, without a pixel shader. Alpha particles also premultiply their vertex
RGB by quantized particle alpha; additive vertices retain their RGB. Texture
alpha already distinguishes additive emission from OVER in this path. Converting
vertex RGB introduces another 8-bit quantization step compared with the original
path; exact agreement with the old reference is not expected.

### Mipmap isolation

The atlas never uses automatic whole-atlas mip generation. For each variant:

1. Decode and convert each existing DDS mip independently, preserving its
   pixels and sampling alignment. Alpha entries multiply each level's RGB by
   that level's alpha; additive entries retain RGB with zero alpha. Some sources
   stop at 4x4: only the missing 2x2/1x1 tail is box-filtered from the last
   converted level. No filter ever combines neighboring entries.
2. Allocate a **64-pixel gutter on every side**, with content origins and extents
   aligned to 64 pixels. This makes the rectangle's position and extent integral
   at all seven retained levels.
3. Populate every mip's gutters independently. Repeat-addressed sources copy
   their opposite edge; clamp-addressed sources extend their nearest edge.
   Corners follow both addressing rules. This preserves local edge filtering
   without sampling a neighboring entry.
4. Use only L0..L6. The gutter shrinks to **one texel at L6**. Deeper atlas levels
   are not allocated, so the sampler clamps there instead of entering levels
   where entries would merge. A 64-pixel source reaches 1x1; a 256-pixel source
   stops at 4x4. Extreme minification may consequently alias more than a full
   per-source chain. A production atlas needs mip classes/pages or another
   explicit policy for that tail.

The sampler uses the engine's FAST min/mag filter selection (bilinear in this
configuration), with the existing mip filter. Anisotropic sampling is excluded:
one final-level gutter texel is sized for bilinear/trilinear footprints. No UV
half-texel inset is used: UV boundaries map to the content boundaries, and the
valid per-entry gutters provide the correct edge samples. Test **L** at every
retained level, and **M** for more severe minification. Compare T against I to
isolate packing/filtering effects from premultiplication and mip conversion.

### Batching and sorting

`PointGroupClass::Set_Texture_UV_Rects` accepts an optional refcounted array of
`(u0,v0,u1,v1)` rectangles. The vertex copy applies these after existing animation
frame coordinates, supports active-point indirection, and leaves the original
path unchanged when null. The atlas experiment passes one rectangle per particle
while retaining the existing quad generation and triangle sorter.

Atlas submissions split only at the existing **512-particle capacity**, crossing
both texture and blend-mode boundaries. The static stress scene caches a depth
order for these chunks, avoiding interleaved sorting nodes that would undo the
batching benefit. The production sorter still sorts triangles and preserves
back-to-front compositing. Dynamic particles would need to refresh that order as
they move. The ten-particle scene fits in one submission; the 4,000-particle scene
fits in eight. This does not change production game particle materials or add
dynamic atlas management to the game.

### Atlas verification on bequiet

The following historical checks were recorded at 960×720, before the 1440p
upgrade. Both Generals and Zero Hour test targets built successfully, with the
existing reference files preserved:

| Scene | Original + Ronin | Shader premultiplication | Texture atlas |
|---|---:|---:|---:|
| Ten particles | 10 draws | 10 draws | 1 draw |
| 4,000 particles | 3,713 draws | 3,597 draws | 8 draws |

The stress-scene reduction from the Ronin path is **99.78%**. The eight draws
correspond to the existing PointGroup limit of 2,048 vertices / 512 quads per
submission. The static depth-contiguous chunk order is part of this result.

Atlas-versus-isolated converted-texture comparisons reached at most **1/255 RGB
delta** in all seven retained levels, with both bilinear and trilinear filtering.
The coarsest level matched exactly. In the normal stress scene, 254 of 691,200
pixels differed, each by at most 1/255; at four times the distance, only 9 pixels
differed, again by at most 1/255. Reversed atlas submission matched exactly.
These checks found no neighboring-entry bleed in the tested scenes/filter modes.

The first implementation regenerated all mips from L0. That caused the reported
**top-left shift**: the legacy DDS lower levels have a different sampling phase
from a centered box reduction. For example, rebuilding EXFireball03 L3 moved its
RGB centroid about 0.43 mip texels left and 0.45 up. The conversion now retains
and premultiplies the original DDS levels; no geometry or UV offset was added.
The gutters and depth batching are unchanged. This preserves authored alignment
rather than compensating for it with a mip-dependent offset.

After the fix, the stress-scene mean absolute channel delta fell from
2.329/255 to 0.205/255, with maximum delta 16/255; the ten-particle maximum
was 8/255. Both target builds and the seven-level bilinear/trilinear checks
passed, and the draw counts remain 1 and 8.

The atlas is still not guaranteed bit-identical to the original path: texture
and vertex premultiplication round to 8 bits, filtering operates on premultiplied
colors, and missing mip tails are appended where needed. The original path and
saved references remain available for strict comparison.

Initial uncapped throughput before the authored-mip alignment fix, on bequiet,
scene 2, sorting on, comparison readback paused,
16 ms sleep disabled; six one-second samples per pass:

| Path | Median FPS, pass 1 | Median FPS, pass 2 |
|---|---:|---:|
| Original + Ronin | 124.1 | 116.8 |
| Shader premultiplication | 144.0 | not repeated |
| Texture atlas | 584.1 | 614.0 |

These are measurements of this static test on one machine, including its cached
atlas chunk order, different texture sampling/conversion, presentation and tool
overhead. They establish a strong improvement for this fixture, not a prediction
of full-game FPS. The test was left in scene 2, atlas enabled, with comparison and
the sleep limiter restored.

## 1440p and overdraw verification

Both win32-profile targets built successfully. The Zero Hour backbuffer and all
three new reference images were verified as 2560×1440. All three original-mode
scenes matched their saved RGB references; scene 3 also matched after reversing
submission and after restarting the executable. Older 960×720 references retained
their original hashes. Scene 3 submitted 200 triangles in 100 original-mode draws
or one atlas draw. P and T remain comparison experiments and report their actual
RGB differences rather than replacing the original reference.
