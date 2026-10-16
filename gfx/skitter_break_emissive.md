# Skitter Break texture

`skitter_break_emissive.png` is a 256×256 RGB565 emissive texture generated with the built-in imagegen tool and downscaled from the selected square result. Fine cobalt-blue and cyan cracks cover the black carapace.

The enemy now has a stable surface UV layout shared by Idle, Break and Down. `skitter_diffuse.png` bakes the original vertex colours onto that layout. The arena still uses `skitter_palette.png` independently. Regenerate the UVs, diffuse texture and mesh header with `scripts/export_skitter.py`; keep the generated crack image as its input.

The renderer adds the emissive texture only when the delayed visual Break flag is active. It persists after stun expires, including the Down pose when both conditions are active. Stun alone does not enable cracks. A new encounter clears the visual flag. If the optional emissive texture fails to load, the existing Break tint is used. The two new 256×256 RGB565 textures use 256 KiB of linear memory together; draw count is unchanged.

Final imagegen prompt:

A flat square seamless emissive texture map for a broken black alien insect carapace in a low-poly Nintendo 3DS RPG. Pure black background with a dense network of narrow electric cobalt blue cracks and fine branching cyan fissures, like fractured black obsidian glowing from inside. About 18 to 24 irregular cells across the width, with varied branching and occasional brighter cyan junctions. Black must dominate 85 percent of surface. Thin sharp blue lines, limited bright cyan core, very minimal halo so the cracks survive downscaling to 256 by 256. Uniform crack scale across the whole square, extends to all edges, no perspective, no highlights, no shading, no actual insect or objects, no red, no text, no border. This is a production UV emissive map, not a scene. Square 1:1.

Hardware validation still needed: break Skitter, wait until stun expires, stun without Break on a fresh encounter, then continue to Hellspider and restart. Check that cracks follow the legs and shield and do not leak into the arena or the next enemy.
