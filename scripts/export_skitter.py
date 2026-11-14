"""Run with Blender: blender -b -P scripts/export_skitter.py -- <repository>."""
import bpy, math, sys, json
from pathlib import Path
from mathutils import Vector
root=Path(sys.argv[sys.argv.index('--')+1]).resolve()
bpy.ops.wm.open_mainfile(filepath=str(root/'assets/skitter/scene.blend'))
scene=bpy.context.scene
for node in bpy.data.materials['Existing_skyline'].node_tree.nodes:
 if node.type == 'TEX_IMAGE':
  node.image=bpy.data.images.load(str(root/'gfx/skitter_backdrop_diffuse.png'),check_existing=True)
arena=bpy.data.objects['Skitter_Street_Arena']
for polygon in arena.data.polygons:
 if arena.data.materials[polygon.material_index].name == 'Existing_skyline':
  for loop_index in polygon.loop_indices:
   arena.data.uv_layers.active.data[loop_index].uv.y /= 3.0
rig=bpy.data.objects['Skitter_Rig'];enemy=bpy.data.objects['Skitter_Prototype']
rig.animation_data_clear()
for p in rig.pose.bones:
 p.location=(0,0,0);p.rotation_mode='XYZ';p.rotation_euler=(0,0,0);p.scale=(1,1,1)
scene.frame_set(1);bpy.context.view_layer.update()
# Generate one stable surface UV layout before evaluating any pose.
bpy.ops.object.select_all(action='DESELECT')
enemy.select_set(True);bpy.context.view_layer.objects.active=enemy
bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(angle_limit=math.radians(66),island_margin=.025)
bpy.ops.object.mode_set(mode='OBJECT')
original_material=enemy.data.materials[0]
bake_material=bpy.data.materials.new('Skitter_colour_bake');bake_material.use_nodes=True
nodes=bake_material.node_tree.nodes;links=bake_material.node_tree.links
vertex_color=nodes.new('ShaderNodeVertexColor');vertex_color.layer_name='Color'
emit=nodes.new('ShaderNodeEmission');links.new(vertex_color.outputs['Color'],emit.inputs['Color'])
links.new(emit.outputs[0],nodes.get('Material Output').inputs['Surface'])
diffuse=bpy.data.images.new('Skitter_diffuse',width=256,height=256)
texture=nodes.new('ShaderNodeTexImage');texture.image=diffuse;nodes.active=texture
for node in nodes:node.select=node==texture
enemy.data.materials[0]=bake_material
scene.render.engine='CYCLES';scene.cycles.samples=1;scene.render.bake.margin=4
bpy.ops.object.bake(type='EMIT')
diffuse.filepath_raw=str(root/'gfx/skitter_diffuse.png');diffuse.file_format='PNG';diffuse.save()
(root/'gfx/skitter_diffuse.t3s').write_text('-f rgb565 -z auto\nskitter_diffuse.png\n')
enemy.data.materials[0]=original_material
break_image=bpy.data.images.load(str(root/'gfx/skitter_break_emissive.png'))
preview_material=bpy.data.materials.new('Skitter_break_preview');preview_material.use_nodes=True
pn=preview_material.node_tree.nodes;pl=preview_material.node_tree.links
base=pn.new('ShaderNodeTexImage');base.image=diffuse
cracks=pn.new('ShaderNodeTexImage');cracks.image=break_image
bsdf=pn.get('Principled BSDF');pl.new(base.outputs['Color'],bsdf.inputs['Base Color'])
pl.new(cracks.outputs['Color'],bsdf.inputs['Emission']);bsdf.inputs['Roughness'].default_value=.72

palette=[]
def srgb(v):return 12.92*v if v<=.0031308 else 1.055*v**(1/2.4)-.055
def color_key(c):return tuple(round(max(0,min(1,srgb(v)))*255) for v in c[:3])
def color_uv(c):
 key=color_key(c)
 if key not in palette:palette.append(key)
 i=palette.index(key);assert i<256
 return ((i%16+.5)/16,(i//16+.5)/16)
def collect(obj,group=None,local_enemy=False):
 deps=bpy.context.evaluated_depsgraph_get();ev=obj.evaluated_get(deps);me=ev.to_mesh();me.calc_loop_triangles()
 colors=me.color_attributes.get('Color');uv=me.uv_layers.active
 verts=[];indices=[];lookup={}
 for tri in me.loop_triangles:
  if group is not None and tri.material_index!=group:continue
  for li in tri.loops:
   vi=me.loops[li].vertex_index;v=me.vertices[vi]
   pos=ev.matrix_world@v.co
   if local_enemy:pos-=rig.location
   # Blender Z-up to the game's Y-up, +Z toward the viewer.
   p=(pos.x,pos.z,-pos.y)
   n=ev.matrix_world.to_3x3()@v.normal;n.normalize();n=(n.x,n.z,-n.y)
   use_color=not local_enemy and obj.data.materials[tri.material_index].name=='Arena_vertex_palette'
   coords=color_uv(colors.data[li].color) if use_color else tuple(uv.data[li].uv)
   key=tuple(round(float(x),6) for x in (*p,*coords,*n))
   if key not in lookup:lookup[key]=len(verts);verts.append(key)
   indices.append(lookup[key])
 ev.to_mesh_clear();return verts,indices
meshes={}
for i,mat in enumerate(arena.data.materials):
 name={'Asphalt':'road','Existing_concrete':'sidewalk','Arena_vertex_palette':'details','Existing_buildings':'buildings','Existing_skyline':'sky'}[mat.name]
 meshes[name]=collect(arena,i)

def body_offset(z):
 p=rig.pose.bones['body'];p.location=p.bone.matrix_local.to_3x3().inverted()@Vector((0,0,z))
for pose in ('idle','break','down'):
 for p in rig.pose.bones:p.location=(0,0,0);p.rotation_euler=(0,0,0)
 if pose=='break':
  body_offset(-.18);rig.pose.bones['body'].rotation_euler.x=.18
  rig.pose.bones['body'].rotation_euler.z=-.13
 elif pose=='down':
  body_offset(-.34);rig.pose.bones['body'].rotation_euler.x=.30
  for p in rig.pose.bones:
   if p.name.startswith('target_'):
    foot=p.bone.head_local
    p.location=p.bone.matrix_local.to_3x3().inverted()@Vector((foot.x*.45,foot.y*.30,0))
 bpy.context.view_layer.update()
 meshes[pose]=collect(enemy,local_enemy=True)
 if '--preview' in sys.argv:
  enemy.data.materials[0]=preview_material if pose!='idle' else original_material
  out=root/'skitter-previews';out.mkdir(exist_ok=True)
  enemy.hide_render=False;bpy.data.objects['Ruka_Chibi'].hide_render=True
  scene.cycles.samples=32;scene.render.resolution_x=800;scene.render.resolution_y=480
  scene.render.filepath=str(out/(pose+'.png'));bpy.ops.render.render(write_still=True)

lines=['#pragma once','// Generated by scripts/export_skitter.py.','namespace skitter_mesh {','struct Vertex { float pos[3]; float uv[2];  };']
def fmt(xs):return ','.join(f'{x:.6f}f' for x in xs)
for name,(vs,ids) in meshes.items():
 lines.append(f'static const Vertex {name}_verts[] = {{')
 for v in vs:lines.append('  {{'+fmt(v[:3])+'},{'+fmt(v[3:5])+'}},')
 lines.append('};');lines.append(f'static const unsigned short {name}_indices[] = {{')
 for j in range(0,len(ids),24):lines.append('  '+','.join(map(str,ids[j:j+24]))+',')
 lines.append('};')
 assert len(vs)<65536
lines.append('}')
(root/'source/mesh_skitter_data.h').write_text('\n'.join(lines)+'\n')
# One 16x16 swatch grid; each cell has padding and is sampled at its centre.
img=bpy.data.images.new('Skitter_palette',width=256,height=256)
pixels=[1.]*(256*256*4)
for y in range(256):
 for x in range(256):
  i=(y//16)*16+x//16;c=palette[i] if i<len(palette) else (0,0,0)
  k=(y*256+x)*4;pixels[k:k+4]=[v/255 for v in c]+[1]
img.colorspace_settings.name='sRGB';img.pixels=pixels
img.filepath_raw=str(root/'gfx/skitter_palette.png');img.file_format='PNG';img.save()
# Embedded textures are packed in the native scene.
for image in bpy.data.images:
 if image.name.startswith('skitter_asphalt'):
  image.filepath_raw=str(root/'gfx/skitter_asphalt.png');image.file_format='PNG';image.save()
for name in ('skitter_palette','skitter_asphalt'):
 (root/'gfx'/(name+'.t3s')).write_text('-f rgb565 -z auto\n'+name+'.png\n')
summary={n:dict(vertices=len(v),triangles=len(i)//3) for n,(v,i) in meshes.items()}
(root/'assets/skitter/mesh_info.json').write_text(json.dumps(summary,indent=2))
print('SKITTER_EXPORT',json.dumps(summary),flush=True)
