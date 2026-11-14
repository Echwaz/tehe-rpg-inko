# Asset sources and build inputs

The third-person exploration authoring scene, native Skitter reference, layout and export notes are in `assets/exploration/`. Its exported meshes are checked into `romfs/meshes/explore_*.bin`; regenerate using `scripts/export_exploration.py` and Blender. The existing compact-header packer leaves these additional files intact.

Skitter's distant high-rise skyline is `gfx/skitter_backdrop_diffuse.png`; generation prompt and provenance are in the adjacent `.md` file. The exporter substitutes this image for the packed skyline material. Runtime loads one arena backdrop at a time.

The repository contains the inputs needed to build the current game. It does not contain all original authoring files or a complete asset attribution manifest.

| Files | Build/use | Provenance recorded in this repository |
| --- | --- | --- |
| `gfx/*.png`, `gfx/*.t3s` | `tex3ds` converts the listed PNGs into embedded `.t3x` textures and generated headers | Original source/permission records for the base arena textures, boss textures, background, and character portraits are not included |
| `gfx/death_slug_break_emissive.png` | Optional alternate emissive atlas | Its reference and image-generation prompt are recorded in `gfx/death_slug_break_emissive.md` |
| `source/mesh_arena_data.h`, `source/mesh_slug_data.h` | Authoring geometry, packed into `romfs/meshes/*.bin` | Headers describe generated geometry; original generator/authoring files are not included |
| `source/mesh_slug_awaken_data.h` | Alternate vertices using the base boss UVs and indices | Previously named `tools/generate_slug_awaken.py`; that script is absent from the available Git history |
| `source/mesh_white_tree.h` | Three solid-color mesh groups | Header identifies an original low-poly tree; the previously named `tools/generate_white_tree.py` is absent from the available Git history |
| `romfs/sfx/*.ogg` | Embedded in RomFS, decoded to PCM at startup | Individual source/author/license records are not included |
| `meta/icon.png`, `meta/banner.png`, `meta/banner_audio.wav` | SMDH/CIA metadata | Individual source/author/license records are not included |
| `source/third_party/lodepng.*` | PNG decoder | Copyright and license text remain in the vendored source files |
| Player BGM and replacement portraits | Loaded from `SD:/3ds/Tehe-RPG-inko/assets/` | Supplied by the player; BGM is not bundled |

## Changing assets

1. Edit a PNG and keep its `.t3s` input name and required UV atlas layout consistent. The Makefile tracks texture dependencies through `tex3ds` output.
2. A new texture requires a `.t3s` description plus its runtime loading/binding code. Run a clean build when changing the set of texture files.
3. Mesh headers are currently the authoritative geometry. Until original generators are restored, edit them directly or add an authoring source and exporter alongside a deliberate geometry change. Preserve the vertex layout, 16-bit indices, and shared base/Awaken topology.
4. Run `make test` after changing geometry or SFX and build the 3DS target after changing textures. Mesh tests check indices and several camera visibility regressions; visual checks remain necessary.
5. Record each new asset's author, source URL or source file, modifications, and permission/license terms here. Existing missing records require information from the project owner; they cannot be reconstructed from filenames.

The missing generators cannot be recovered from the checked-in geometry alone as their original authoring scripts. This document replaces stale source references rather than claiming that the original asset pipeline has been restored.

## Skitter authoring inputs

`assets/skitter/scene.blend` contains the procedural Skitter prototype, city street arena, packed textures, and close battle camera. The model and arena were created with AI-assisted Blender scripts from the user-provided Skitter reference and the existing arena palette. Existing concrete, building and skyline textures retain their original provenance above. The asphalt texture is new.

Regenerate the native mesh header and generated textures with:

```sh
blender -b -P scripts/export_skitter.py -- /absolute/path/to/repository
```

Append `--preview` to render the Idle, Break and Down poses. This creates `skitter-previews/` for local review. `source/mesh_skitter_data.h` contains arena groups and three static enemy poses; no Blender dependency is required to build the game. Pose deformation is baked into those meshes for the 3DS renderer. Run `make` or `make test` afterward to repack the RomFS meshes.

Skitter now uses `gfx/skitter_diffuse.png` and `gfx/skitter_break_emissive.png` with surface UVs shared across its three poses. The Break texture provenance, prompt and validation notes are in `gfx/skitter_break_emissive.md`.

## Compact runtime meshes and encounter assets

The authoring mesh headers contain five floats per vertex (position and UV). The unlit renderer does not consume normals. `scripts/pack_meshes.py` writes the 17 `romfs/meshes/*.bin` files; both `make` and `make test` repack changed headers automatically using Python 3. `assets/mesh_sizes.json` records payload counts. No Blender dependency is required for packing.

Each file starts with a 16-byte little-endian header (`TMH1`, vertex count, index count, reserved zero), followed by 20-byte vertices and 16-bit indices. Runtime loading rejects invalid lengths, counts, non-finite values and invalid indices. Data is read directly into linear GPU buffers. Skitter's poses share one index buffer, as do the Hellspider forms; shared index data is checked when loading each alternate pose.

`battle3d::init(top)` reserves the arena colour/depth cache before UI or encounter textures are imported. Shared ground/building/sky textures remain loaded across encounters. Exclusive mesh and texture buffers are released and replaced by `prepareScene()` between frames, after a GPU fence and before the draw frame begins. Tex3DS VRAM imports must run outside an active frame, so each staging-buffer upload finishes before Tex3DS frees that buffer. Failed loading cleans up partial encounter resources and uses the existing 2D fallback; it is not retried every frame. The existing direct-draw fallback remains available when the cache cannot be allocated.

The decoded 3D texture payload falls from 3,072 KiB with both encounters resident to 2,176 KiB for either current encounter (excluding embedded compressed texture sources and other application memory). Native section size and host lifecycle tests validate memory changes; hardware FPS and transition timing still need measurement.

### Texture upload regression fix

VRAM texture imports are no longer called from `renderTop()`. `prepareScene()` runs before the main draw frame, fences previous GPU work with an empty frame, then imports textures outside frame mode. This matters because Tex3DS releases its linear staging buffer immediately after `C3D_TexLoadImage`; Citro3D queues copies inside a frame but waits for them outside a frame. The old placement allowed staging memory to be reused before the queued upload, causing noisy/swapped textures and apparent Break cracks on intact enemies.

The host renderer regression test now models an explicit active-frame state, rejects texture imports and mesh allocation/free inside the drawing frame, and checks preparation separately from rendering. The earlier tests only checked resource counts and missed this transfer-lifetime requirement.

Primary implementation references: [Tex3DS importer](https://github.com/devkitPro/citro3d/blob/master/source/tex3ds.c), [Citro3D transfer queue](https://github.com/devkitPro/citro3d/blob/master/source/renderqueue.c).
