# Blue break cracks

`death_slug_break_emissive.png` is a 256 x 256 RGB emissive atlas. Its `.t3s`
uses RGB565, matching the original atlas and UVs on both boss poses.
Electric blue/cyan fissures cover the black areas; the red mouth markings remain.

The renderer substitutes this for the regular emissive texture only while the
delayed visual break flag is set. Stun alone does not activate it. Awakening and
restart clear the flag through the existing battle screen logic. The diffuse
map, hit flash, mesh, draw count and arena cache are unchanged. The old warm
break tint is disabled when the blue texture is available.

The extra texture uses 128 KiB of linear RAM to preserve the existing VRAM
budget for arena caching. It loads once, not on each hit or frame. If loading
fails, the existing break tint remains available. There is no additional draw
pass or texture sampler, but hardware FPS should still be checked while broken.

Validation on hardware: break the first boss, wait for stun to expire, reach
awakening, break the second form, then restart. Cracks should persist after
stun, clear during revival, appear on both forms when broken, and leave no
traces on the arena. Compare SELECT FPS before and after break.

Asset generated with the built-in imagegen tool from the original emissive
atlas, then downscaled to the original 256 x 256 dimensions.

Final refinement prompt:

Edit this square game emissive UV texture atlas. Keep the two red slit-mouth
shapes at exactly the same positions, proportions and color. Increase the
density of electric blue cracks substantially: divide each current big black
cell into roughly 4 to 9 smaller irregular black cells with fine blue branching
fissures. Aim for 20 to 25 irregular cells across image width. Retain black
dominant surfaces and narrow cobalt blue lines with small bright cyan
junctions. Fine sharp cracks like fractured obsidian, no broad blue haze, no
image lighting, no text, no objects. Preserve exact red shapes with no blue
crossing inside them. Output a square flat emissive map on pure black, all the
way to edges.
