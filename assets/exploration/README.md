# Exploration authoring assets

The runtime exporter omits the solid building boxes where textured facade planes already enclose them, and trims street-facing facade planes above their lobby/lintel. These authoring layers must not compete in the runtime depth buffer. `tests/facade_geometry_test.py` rejects near-coplanar wall/facade surfaces. Exploration uses a 0.5 metre camera near plane to retain useful depth precision across the long street.

`scene.blend` is the approved third-person city revision with the original chibi Ruka mesh/rig and enlarged Skitter references. `skitter.blend` is the original Skitter prototype; the exporter reads its native vertex colors to avoid color changes from a GLB round trip.

`area.json` records the free-movement layout, obstacles and three patrol routes. `scripts/export_exploration.py` exports these to `source/exploration_data.h`, eleven compact RomFS meshes and `gfx/exploration_palette.png`. Run with Blender and pass the repository directory after `--`. Normal game builds use the checked-in binary meshes and do not need Blender.

The runtime reuses existing ground, building, asphalt and skyline textures. Ruka uses the original 1,637-vertex / 2,383-triangle mesh, one idle pose and four evaluated walking poses. The walking poses share indices at runtime. The original Color attribute supplies a padded palette atlas for the character, props and Skitter. Ruka is scaled by 0.65 and Skitter by 1.25.

Blender uses Z-up; the runtime uses Y-up and negative Z toward Hellspider. Geometry and collision data are converted consistently. The moving exploration camera draws the arena each frame, without reusing the static battle snapshot. Texture imports and releases remain between GPU frames.
