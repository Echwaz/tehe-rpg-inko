# Skitter skyline

Created with the built-in imagegen tool, using arena_backdrop_diffuse.png as the edit reference. Resized to 1024 x 512 RGB for RGB565 runtime import.

Final prompt: Keep the extremely simple flat polygon/block silhouette style and muted grey-green sky and taupe grey palette. Change squat low blocks into taller urban office building silhouettes. Only 10-14 plain rectangular towers with varied heights and simple rooftop setbacks, two overlapping flat silhouette layers. No windows, facade detail, outlines, antennas, texture or realism. Top quarter empty pale grey-green sky. Buildings occupy bottom three quarters, extend beyond bottom edge. Distant frontal skyline elevation, 2:1 aspect ratio, no ground, road, characters or text.

Used only by Skitter. Each arena loads its own backdrop, preserving the 2176 KiB decoded texture budget.

The skyline UV height is divided by three relative to the original arena mapping. This expands the buildings vertically in world space, from roughly 10 metres to 35 metres at the same camera and distance. The exporter applies the same mapping to the authoring mesh.
