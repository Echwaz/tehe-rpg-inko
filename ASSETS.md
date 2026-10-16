# Asset sources and build inputs

The repository contains the inputs needed to build the current game. It does not contain all original authoring files or a complete asset attribution manifest.

| Files | Build/use | Provenance recorded in this repository |
| --- | --- | --- |
| `gfx/*.png`, `gfx/*.t3s` | `tex3ds` converts the listed PNGs into embedded `.t3x` textures and generated headers | Original source/permission records for the base arena textures, boss textures, background, and character portraits are not included |
| `gfx/death_slug_break_emissive.png` | Optional alternate emissive atlas | Its reference and image-generation prompt are recorded in `gfx/death_slug_break_emissive.md` |
| `source/mesh_arena_data.h`, `source/mesh_slug_data.h` | Compiled vertex/index data | Headers describe generated geometry; original generator/authoring files are not included |
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

Append `--preview` to render the Idle, Break and Down poses. This creates `skitter-previews/` for local review. `source/mesh_skitter_data.h` contains arena groups and three static enemy poses; no Blender dependency is required to build the game. Pose deformation is baked into those meshes for the 3DS renderer.

Skitter now uses `gfx/skitter_diffuse.png` and `gfx/skitter_break_emissive.png` with surface UVs shared across its three poses. The Break texture provenance, prompt and validation notes are in `gfx/skitter_break_emissive.md`.
