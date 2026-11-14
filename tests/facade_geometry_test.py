"""Reject solid walls competing with the textured exploration facade."""
from pathlib import Path
import struct,sys
root=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parent.parent
def triangles(name):
 raw=(root/'romfs/meshes'/(name+'.bin')).read_bytes()
 magic,n,ni,reserved=struct.unpack_from('<4sIII',raw)
 assert magic==b'TMH1' and reserved==0 and len(raw)==16+n*20+ni*2
 vertices=[struct.unpack_from('<3f',raw,16+i*20) for i in range(n)]
 ids=struct.unpack_from('<'+'H'*ni,raw,16+n*20)
 return [[vertices[i] for i in ids[j:j+3]] for j in range(0,ni,3)]
def planes(tris):
 for tri in tris:
  low=[min(v[i] for v in tri) for i in range(3)]
  high=[max(v[i] for v in tri) for i in range(3)]
  if high[1]-low[1]<.01:continue
  for axis in (0,2):
   if high[axis]-low[axis]<.0001:
    yield axis,(low[axis]+high[axis])/2,low,high
facades=list(planes(triangles('explore_buildings')))
walls=list(planes(triangles('explore_props')))
overlaps=[]
for axis,position,low,high in facades:
 for other_axis,other_position,other_low,other_high in walls:
  if axis!=other_axis or abs(position-other_position)>.025:continue
  dimensions=[i for i in range(3) if i!=axis]
  overlap=[min(high[i],other_high[i])-max(low[i],other_low[i]) for i in dimensions]
  # Small lintel end caps can meet the facade at a corner; they are not a second wall.
  if all(v>.005 for v in overlap) and overlap[0]*overlap[1]>.25:
   overlaps.append((axis,round(position,3),round(low[1],3),round(high[1],3)))
assert not overlaps,f'{len(overlaps)} competing facade/wall triangle pairs: {overlaps[:4]}'
print('Facade geometry: no broad competing wall/lobby surfaces within 2.5 cm')
