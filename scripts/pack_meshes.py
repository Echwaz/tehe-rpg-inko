"""Pack the compact authoring headers into RomFS meshes; no Blender needed."""
from pathlib import Path
import re, struct, json, sys
root=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path(__file__).resolve().parent.parent
sources={
 'mesh_arena_data.h':{'battle3d_arena_'+n:n for n in ['ground','buildings','rubble','backdrop']},
 'mesh_slug_data.h':{'battle3d_slug':'slug'},
 'mesh_slug_awaken_data.h':{'battle3d_slug_awaken':'slug_awaken'},
 'mesh_skitter_data.h':{n:'skitter_'+n for n in ['road','sidewalk','details','buildings','sky','idle','break','down']},
 'mesh_white_tree.h':{n:'tree_'+n for n in ['light','shade','hole']}}
out=root/'romfs/meshes';out.mkdir(parents=True,exist_ok=True)
report={};slug_indices=None
for file,names in sources.items():
 text=(root/'source'/file).read_text()
 for prefix,name in names.items():
  suffix='_vertices' if file=='mesh_white_tree.h' else '_verts'
  match=re.search(r'\b'+prefix+suffix+r'\s*\[[^]]*\]\s*=\s*\{(.*?)\};',text,re.S)
  assert match,name
  vertices=[]
  for row in match[1].splitlines():
   vals=re.findall(r'([-+]?\d+(?:\.\d*)?(?:[eE][-+]?\d+)?)f',row)
   if vals:
    assert len(vals)==5,(name,row);vertices.extend(map(float,vals))
  idx=re.search(r'\b'+prefix+r'_indices\s*\[[^]]*\]\s*=\s*\{(.*?)\};',text,re.S)
  indices=list(map(int,re.findall(r'\b\d+\b',idx[1]))) if idx else slug_indices
  assert indices is not None,name
  if name=='slug':slug_indices=indices
  count=len(vertices)//5
  assert count<=65536 and len(indices)%3==0 and max(indices)<count
  data=struct.pack('<4sIII',b'TMH1',count,len(indices),0)
  data+=struct.pack('<'+'f'*len(vertices),*vertices)+struct.pack('<'+'H'*len(indices),*indices)
  target=out/(name+'.bin')
  if not target.exists() or target.read_bytes()!=data:target.write_bytes(data)
  report[name]=dict(vertices=count,indices=len(indices),bytes=len(data))
metadata=root/'assets/mesh_sizes.json'
metadata_bytes=(json.dumps(report,indent=2)+'\n').encode()
if not metadata.exists() or metadata.read_bytes()!=metadata_bytes:metadata.write_bytes(metadata_bytes)
print('Packed',len(report),'compact meshes:',sum(v['bytes'] for v in report.values()),'bytes')
