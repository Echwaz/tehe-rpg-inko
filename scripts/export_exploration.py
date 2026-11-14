"""Blender -b -P scripts/export_exploration.py -- <repository>."""
import bpy,sys,struct,json,math
from pathlib import Path
from mathutils import Vector
root=Path(sys.argv[sys.argv.index('--')+1]).resolve()
bpy.ops.wm.open_mainfile(filepath=str(root/'assets/exploration/scene.blend'))
scene=bpy.context.scene;scene.frame_set(1)
palette=[];lookup={};groups={k:[] for k in ['ground','road','buildings','sky','props']}
texture_groups={'Ground':'ground','Asphalt':'road','Buildings':'buildings','Skyline':'sky'}

def facade_shell(obj):
 return scene.objects.get(obj.name+'_front') is not None and scene.objects.get(obj.name+'_back') is not None

def trim_lobby_facade(obj):
 if '_side' not in obj.name:return
 base=obj.name.split('_side')[0];lobby=scene.objects.get(base+'_ground_floor')
 if lobby is None:return
 mesh=obj.data
 points=[obj.matrix_world@v.co for v in mesh.vertices]
 if not points or abs(sum(v.x for v in points)/len(points)-lobby.location.x)>.12:return
 top=max(v.z for v in points);cut=lobby.location.z+lobby.dimensions.z/2+.4
 layer=mesh.uv_layers.active
 uv={loop.vertex_index:layer.data[loop.index].uv.copy() for loop in mesh.loops}
 for vertex,point in zip(mesh.vertices,points):
  if point.z>=cut:continue
  upper=next(v for v in mesh.vertices if abs(points[v.index].x-point.x)<.001 and abs(points[v.index].y-point.y)<.001 and points[v.index].z==top)
  t=(cut-point.z)/(top-point.z);new_uv=uv[vertex.index].lerp(uv[upper.index],t)
  point.z=cut;vertex.co=obj.matrix_world.inverted()@point
  for loop in mesh.loops:
   if loop.vertex_index==vertex.index:layer.data[loop.index].uv=new_uv
 mesh.update()
def srgb(v):return round(max(0,min(1,12.92*v if v<=.0031308 else 1.055*v**(1/2.4)-.055))*255)
def color_uv(color):
 rgb=tuple(srgb(v) for v in color[:3])
 if rgb not in lookup:
  assert len(palette)<256,'Palette overflow'
  lookup[rgb]=len(palette);palette.append(rgb)
 i=lookup[rgb];return ((i%16+.5)/16,(i//16+.5)/16)
def collect(obj,frame=1,actor=False):
 scene.frame_set(frame);deps=bpy.context.evaluated_depsgraph_get();ev=obj.evaluated_get(deps)
 mesh=ev.to_mesh();mesh.calc_loop_triangles();colors=mesh.color_attributes.get('Color') or mesh.color_attributes.active_color
 batches={};origin=Vector((0,0,0))
 if actor:
  parent=obj
  while parent.parent:parent=parent.parent
  origin=parent.location.copy()
 for tri in mesh.loop_triangles:
  mat=mesh.materials[tri.material_index] if mesh.materials else None
  group='actor' if actor else texture_groups.get(mat.name,'props')
  verts,indices,keys=batches.setdefault(group,([],[],{}))
  for li in tri.loops:
   vi=mesh.loops[li].vertex_index;p=ev.matrix_world@mesh.vertices[vi].co-origin
   if actor:
    # Remove root yaw; translation and heading are supplied by the runtime.
    parent=obj
    while parent.parent:parent=parent.parent
    p=parent.rotation_euler.to_matrix().inverted()@p
   if colors:
    c=colors.data[li if colors.domain=='CORNER' else vi].color
    uv=color_uv(c)
   elif group in texture_groups.values():uv=tuple(mesh.uv_layers.active.data[li].uv)
   else:
    node=mat.node_tree.nodes.get('Principled BSDF')
    uv=color_uv(node.inputs['Base Color'].default_value)
   key=(vi,tuple(round(v,6) for v in uv))
   if key not in keys:keys[key]=len(verts);verts.append((p.x,p.z,-p.y,*uv))
   indices.append(keys[key])
 ev.to_mesh_clear();return {k:(v,i) for k,(v,i,_) in batches.items()}
for obj in scene.objects:
 if obj.type=='MESH' and obj.parent is None and not obj.name.startswith('Icosphere'):
  # Textured walls and the roof already enclose the building. The underlying
  # solid box is only an authoring/collision reference; exporting it causes z-fighting.
  if facade_shell(obj):continue
  trim_lobby_facade(obj)
  for group,mesh in collect(obj).items():groups[group].append(mesh)
report={}
out=root/'romfs/meshes';out.mkdir(parents=True,exist_ok=True)
def save(name,meshes):
 vertices=[];indices=[]
 for v,i in meshes:
  offset=len(vertices);vertices.extend(v);indices.extend(n+offset for n in i)
 assert vertices and len(vertices)<=65536 and max(indices)<len(vertices)
 data=struct.pack('<4sIII',b'TMH1',len(vertices),len(indices),0)
 data+=b''.join(struct.pack('<5f',*v) for v in vertices)+struct.pack('<'+'H'*len(indices),*indices)
 (out/(name+'.bin')).write_bytes(data)
 report[name]=dict(vertices=len(vertices),triangles=len(indices)//3,bytes=len(data))
for group,meshes in groups.items():save('explore_'+group,meshes)
ruka=bpy.data.objects['Ruka_Chibi']
for name,frame in [('idle',1),('walk0',60),('walk1',66),('walk2',72),('walk3',78)]:
 save('explore_ruka_'+name,list(collect(ruka,frame,True).values()))
with bpy.data.libraries.load(str(root/'assets/exploration/skitter.blend'),link=False) as (src,dst):
 dst.objects=['Skitter_Prototype','Skitter_Rig']
for obj in dst.objects:scene.collection.objects.link(obj)
skitter=next(o for o in dst.objects if o.type=='MESH')
rig=next(o for o in dst.objects if o.type=='ARMATURE')
rig.location=(0,0,0);rig.rotation_euler=(0,0,0);rig.scale=(1.25,)*3
save('explore_skitter',list(collect(skitter,1,True).values()))
image=bpy.data.images.new('exploration_palette',256,256);pixels=[0.0]*(256*256*4)
for y in range(256):
 for x in range(256):
  i=(y//16)*16+x//16;c=palette[i] if i<len(palette) else (0,0,0)
  at=(y*256+x)*4;pixels[at:at+4]=[*(v/255 for v in c),1]
image.pixels=pixels;image.filepath_raw=str(root/'gfx/exploration_palette.png');image.file_format='PNG';image.save()
# Game coordinates use Y-up and -Z as the direction toward the boss.
area=json.loads((root/'assets/exploration/area.json').read_text())
def point(p):return (p[0],-p[1])
lines=['#pragma once','namespace exploration_data {',
 'struct Point { float x,z; };','struct Box { float x,z,hx,hz; };',
 'static const Point road[] = {'+','.join('{%sf,%sf}'%tuple(format(v,'.6f') for v in point(p)) for p in area['main_road']['centres'])+'};',
 'static const Point patrols[3][3] = {']
for patrol in area['patrols']:
 lines.append('{'+','.join('{%sf,%sf}'%tuple(format(v,'.6f') for v in point(p)) for p in patrol['route'])+'},')
lines+=['};','static const Box obstacles[] = {']
for c in area['colliders']:
 x,y,z=c['center'];sx,sy,sz=c['size']
 lines.append('{%.6ff,%.6ff,%.6ff,%.6ff},'%(x,-y,sx/2,sy/2))
lines+=['};','}']
(root/'source/exploration_data.h').write_text('\n'.join(lines)+'\n')
(root/'assets/exploration/mesh_info.json').write_text(json.dumps(dict(meshes=report,palette_colors=len(palette)),indent=2))
print('EXPORTED_EXPLORATION',json.dumps(report),flush=True)
