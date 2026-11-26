"""Build the small animated glTF scene used by the 3DS HOME banner."""
import json
import math
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "meta" / "home-banner.glb"
blob = bytearray()
views, accessors, meshes, materials, nodes, animations = [], [], [], [], [], []


def add_data(data, target=None):
    while len(blob) % 4:
        blob.append(0)
    offset = len(blob)
    blob.extend(data)
    view = {"buffer": 0, "byteOffset": offset, "byteLength": len(data)}
    if target:
        view["target"] = target
    views.append(view)
    return len(views) - 1


def accessor(data, component, kind, count, target=None, minimum=None, maximum=None):
    view = add_data(data, target)
    item = {"bufferView": view, "componentType": component, "count": count, "type": kind}
    if minimum is not None:
        item["min"], item["max"] = minimum, maximum
    accessors.append(item)
    return len(accessors) - 1


def add_mesh(name, positions, normals, indices, color):
    pos = [v for p in positions for v in p]
    norm = [v for p in normals for v in p]
    pos_acc = accessor(struct.pack("<" + "f" * len(pos), *pos), 5126, "VEC3", len(positions), 34962,
                       [min(p[i] for p in positions) for i in range(3)],
                       [max(p[i] for p in positions) for i in range(3)])
    norm_acc = accessor(struct.pack("<" + "f" * len(norm), *norm), 5126, "VEC3", len(norm), 34962)
    idx_acc = accessor(struct.pack("<" + "H" * len(indices), *indices), 5123, "SCALAR", len(indices), 34963)
    material = len(materials)
    materials.append({"name": name, "pbrMetallicRoughness": {"baseColorFactor": color,
                      "metallicFactor": 0, "roughnessFactor": 0.78}})
    meshes.append({"name": name, "primitives": [{"attributes": {"POSITION": pos_acc, "NORMAL": norm_acc},
                  "indices": idx_acc, "material": material, "mode": 4}]})
    return len(meshes) - 1


def box(name, color):
    # Face-specific shades keep the cube readable even with stereoscopic 3D off.
    faces = [((1,0,0),[(.5,-.5,-.5),(.5,.5,-.5),(.5,.5,.5),(.5,-.5,.5)],.68),
             ((-1,0,0),[(-.5,-.5,.5),(-.5,.5,.5),(-.5,.5,-.5),(-.5,-.5,-.5)],.58),
             ((0,1,0),[(-.5,.5,-.5),(-.5,.5,.5),(.5,.5,.5),(.5,.5,-.5)],1.12),
             ((0,-1,0),[(-.5,-.5,.5),(-.5,-.5,-.5),(.5,-.5,-.5),(.5,-.5,.5)],.58),
             ((0,0,1),[(.5,-.5,.5),(.5,.5,.5),(-.5,.5,.5),(-.5,-.5,.5)],1.0),
             ((0,0,-1),[(-.5,-.5,-.5),(-.5,.5,-.5),(.5,.5,-.5),(.5,-.5,-.5)],.58)]
    primitives=[]
    for normal, vertices, shade in faces:
        positions=[tuple(p) for p in vertices]
        pos=[v for p in positions for v in p]
        norms=[v for _ in positions for v in normal]
        pos_acc=accessor(struct.pack("<"+"f"*len(pos),*pos),5126,"VEC3",4,34962,
                         [min(p[i] for p in positions) for i in range(3)],
                         [max(p[i] for p in positions) for i in range(3)])
        norm_acc=accessor(struct.pack("<"+"f"*len(norms),*norms),5126,"VEC3",4,34962)
        idx_acc=accessor(struct.pack("<6H",0,1,2,0,2,3),5123,"SCALAR",6,34963)
        mat_color=[min(1.0,c*shade) for c in color[:3]]+[color[3]]
        material=len(materials)
        materials.append({"name":f"{name} face {material%6+1}","pbrMetallicRoughness":{
                          "baseColorFactor":mat_color,"metallicFactor":0,"roughnessFactor":0.78}})
        primitives.append({"attributes":{"POSITION":pos_acc,"NORMAL":norm_acc},
                           "indices":idx_acc,"material":material,"mode":4})
    meshes.append({"name":name,"primitives":primitives})
    return len(meshes)-1


def coin_mesh():
    sides, radius, depth = 12, 0.5, 0.12
    p, n, ix = [], [], []
    # Front and back faces, then the polygon edge strip.
    for z, nz in ((depth/2, 1), (-depth/2, -1)):
        center = len(p); p.append((0,0,z)); n.append((0,0,nz))
        ring = len(p)
        for i in range(sides):
            a = 2*math.pi*i/sides
            p.append((radius*math.cos(a), radius*math.sin(a), z)); n.append((0,0,nz))
        for i in range(sides):
            if nz > 0: ix.extend((center, ring+i, ring+(i+1)%sides))
            else: ix.extend((center, ring+(i+1)%sides, ring+i))
    edge = len(p)
    for z in (depth/2, -depth/2):
        for i in range(sides):
            a=2*math.pi*i/sides; p.append((radius*math.cos(a),radius*math.sin(a),z)); n.append((math.cos(a),math.sin(a),0))
    for i in range(sides):
        j=(i+1)%sides; a=edge+i; b=edge+j; c=edge+sides+j; d=edge+sides+i
        ix.extend((a,b,c,a,c,d))
    return add_mesh("Golden token", p, n, ix, [1.0,0.72,0.12,1.0])


player = box("Player • coral", [1.0,0.16,0.24,1])
wall = box("Barrier • cyan", [0.12,0.77,0.95,1])
floor = box("Track • midnight", [0.07,0.16,0.28,1])
coin = coin_mesh()


def node(name, mesh, translation, scale):
    nodes.append({"name": name, "mesh": mesh, "translation": translation, "scale": scale})
    return len(nodes)-1


node("Midnight track", floor, [0,-3.1,0], [20,0.28,3.5])
node("Left edge light", wall, [-8,-2.82,0], [0.18,0.1,3.3])
node("Right edge light", wall, [8,-2.82,0], [0.18,0.1,3.3])
player_node = node("Runner", player, [-5,-1.9,0], [1.7,1.7,1.7])
wall_a = node("Sliding barrier A", wall, [0,-1.3,0], [0.42,3.2,1.6])
wall_b = node("Sliding barrier B", wall, [5,-1.6,0], [0.42,2.6,1.6])
coin_node = node("Spinning golden token", coin, [-1.8,0.2,0], [1.5,1.5,1.5])
node("Runner trail", wall, [-6.2,-1.9,0], [0.42,0.42,0.42])


def animation_channel(name, node_index, path, times, values, kind):
    time_bytes = struct.pack("<" + "f"*len(times), *times)
    time_acc = accessor(time_bytes,5126,"SCALAR",len(times),minimum=[times[0]],maximum=[times[-1]])
    value_bytes = struct.pack("<" + "f"*len(values), *values)
    value_acc = accessor(value_bytes,5126,kind,len(times))
    animation = {"name": name, "samplers": [{"input":time_acc,"output":value_acc,"interpolation":"LINEAR"}],
                 "channels": [{"sampler":0,"target":{"node":node_index,"path":path}}]}
    animations.append(animation)


t = [0,1,2,3,4]
animation_channel("Runner dash",player_node,"translation",t,
                  [-5,-1.9,0, -2,-1.9,0, 1,-1.9,0, 4,-1.9,0, -5,-1.9,0],"VEC3")
animation_channel("Barrier sweep",wall_a,"translation",t,
                  [0,-1.3,0, 0,0.4,0, 0,-1.3,0, 0,0.4,0, 0,-1.3,0],"VEC3")
animation_channel("Barrier pulse",wall_b,"translation",t,
                  [5,-1.6,0, 5,-0.2,0, 5,-1.6,0, 5,-0.2,0, 5,-1.6,0],"VEC3")
spin=[]
for degrees in (0,120,240,360):
    a=math.radians(degrees)/2
    spin.extend((0,0,math.sin(a),math.cos(a)))
animation_channel("Token spin",coin_node,"rotation",[0,1.333,2.667,4],spin,"VEC4")

while len(blob)%4: blob.append(0)
document = {"asset":{"version":"2.0","generator":"WHG Hub HOME banner scene"},
            "scene":0,"scenes":[{"nodes":list(range(len(nodes)))}],"nodes":nodes,"meshes":meshes,
            "materials":materials,"animations":animations,
            "buffers":[{"byteLength":len(blob)}],"bufferViews":views,"accessors":accessors}
json_bytes=json.dumps(document,separators=(",",":"),ensure_ascii=False).encode("utf-8")
json_bytes += b" "*((4-len(json_bytes)%4)%4)
binary=bytes(blob)
total=12+8+len(json_bytes)+8+len(binary)
OUT.parent.mkdir(parents=True,exist_ok=True)
with OUT.open("wb") as f:
    f.write(struct.pack("<4sII",b"glTF",2,total))
    f.write(struct.pack("<I4s",len(json_bytes),b"JSON")); f.write(json_bytes)
    f.write(struct.pack("<I4s",len(binary),b"BIN\x00")); f.write(binary)
print(f"Created {OUT} ({total} bytes, {len(animations)} animation tracks)")
