"""Export real CS2 agent meshes and bind transforms for the external neon renderer.

Source 2 Viewer 20.1 conversion: local bone translations use metres, while
local rotations retain Source axes (only roots carry the glTF axis rotation).
See ValveResourceFormat/IO/GltfModelExporter.Conversion.cs at tag 20.1.
"""
from pathlib import Path
import argparse, hashlib, json, struct, subprocess
from concurrent.futures import ThreadPoolExecutor
import numpy as np

ROOT = Path(__file__).resolve().parent.parent
GAME = Path('F:/SteamLibrary/steamapps/common/Counter-Strike Global Offensive/game/csgo')
CLI = ROOT / 'ferramentas/source2viewer/bin/Source2Viewer-CLI.exe'

def read_glb(path):
    raw = path.read_bytes()
    assert raw[:4] == b'glTF'
    length = struct.unpack_from('<I', raw, 12)[0]
    document = json.loads(raw[20:20+length])
    binary = raw[28+length:]
    def accessor(index):
        a = document['accessors'][index]; view = document['bufferViews'][a['bufferView']]
        dtype = {5121:'u1',5123:'<u2',5125:'<u4',5126:'<f4'}[a['componentType']]
        width = {'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}[a['type']]
        offset = view.get('byteOffset',0) + a.get('byteOffset',0)
        stride = view.get('byteStride',np.dtype(dtype).itemsize*width)
        result = np.ndarray((a['count'],width),dtype,buffer=binary,offset=offset,
                            strides=(stride,np.dtype(dtype).itemsize)).copy()
        if a.get('normalized') and a['componentType'] != 5126:
            result = result.astype(np.float32) / np.iinfo(dtype).max
        return result
    return document, accessor

def convert(path, output):
    g, read = read_glb(path)
    vertices=[]; indices=[]; joint_names=None; ibm=None; poses=None
    # glTF metres -> Source units: [x,y,z] = [gltf.z,gltf.x,gltf.y]/.0254.
    conversion=np.array([[0,0,1,0],[1,0,0,0],[0,1,0,0],[0,0,0,.0254]],dtype=float)/.0254
    for node in g['nodes']:
        if 'mesh' not in node or 'skin' not in node: continue
        name=node['name'].split('.')[-1]
        if name.startswith('firstperson') or name=='defusekit': continue
        skin=g['skins'][node['skin']]
        names=[g['nodes'][i]['name'] for i in skin['joints']]
        matrices=read(skin['inverseBindMatrices']).reshape(-1,4,4).transpose(0,2,1)
        if joint_names is None:
            joint_names=names
            ibm=matrices[:,:3,:]/.0254
            poses=np.array([conversion @ np.linalg.inv(m) for m in matrices])[:,:3,:]
            # Remove unit scale from the rest rotations.
            poses[:,:,:3]*=.0254
        assert names==joint_names and len(names)<=128
        assert np.allclose(ibm,matrices[:,:3,:]/.0254,atol=1e-3)
        for prim in g['meshes'][node['mesh']]['primitives']:
            a=prim['attributes']; position=read(a['POSITION']); joints=read(a['JOINTS_0']); weights=read(a['WEIGHTS_0'])
            assert 'WEIGHTS_1' not in a or not np.any(read(a['WEIGHTS_1'])>0), 'More than four influences'
            assert prim.get('mode',4)==4
            assert np.isfinite(position).all() and np.isfinite(weights).all()
            assert np.all(weights.sum(axis=1)>.99)
            weights/=weights.sum(axis=1,keepdims=True)
            base=len(vertices)
            for p,j,w in zip(position,joints,weights):
                vertices.append(struct.pack('<3f4H4f',*p,*j,*w))
            indices.extend((read(prim['indices']).reshape(-1)+base).tolist())
    assert joint_names and len(vertices)>100 and indices
    # Weld identical material-boundary vertices and retain Source's byte weights.
    unique={}; compact=[]; remap=[]
    for raw in vertices:
        values=struct.unpack('<3f4H4f',raw); weights=np.rint(np.array(values[7:])*255).astype(int)
        weights[np.argmax(weights)]+=255-int(weights.sum())
        coords=np.rint(np.array(values[:3])*8192).astype(int)
        assert np.max(np.abs(coords))<=32767
        packed=struct.pack('<3h4B4B',*coords,*values[3:7],*weights)
        if packed not in unique: unique[packed]=len(compact);compact.append(packed)
        remap.append(unique[packed])
    indices=[remap[i] for i in indices];vertices=compact
    assert len(vertices)<=65535
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open('wb') as f:
        f.write(struct.pack('<4sIII',b'CNM3',len(joint_names),len(vertices),len(indices)))
        for name,matrix,pose in zip(joint_names,ibm,poses):
            encoded=name.encode('ascii');assert len(encoded)<64
            f.write(encoded.ljust(64,b'\0'));f.write(matrix.astype('<f4').tobytes());f.write(pose.astype('<f4').tobytes())
        f.write(b''.join(vertices));f.write(struct.pack(f'<{len(indices)}H',*indices))
    return {'file':output.name,'vertices':len(vertices),'triangles':len(indices)//3,
            'bones':len(joint_names),'sha256':hashlib.sha256(output.read_bytes()).hexdigest()}

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--only',nargs='*');args=parser.parse_args()
    provenance=json.loads((ROOT/'ferramentas/cs2-local/provenance.json').read_text())
    vpk_hash=hashlib.sha256((GAME/'pak01_dir.vpk').read_bytes()).hexdigest()
    target=ROOT/'fontes/assets/models'
    cache=ROOT/'fontes/build/neon-models'/f'{provenance["build_number"]}-{vpk_hash[:12]}'
    cache.mkdir(parents=True,exist_ok=True)
    listed=subprocess.check_output([str(CLI),'-i',str(GAME/'pak01_dir.vpk'),'-l','-e','vmdl_c','-f','agents/models/'],text=True)
    models=[line.split(' CRC:')[0] for line in listed.splitlines() if '.vmdl_c CRC:' in line]
    models=[m for m in models if m.split('/')[2].startswith(('ctm_','tm_'))]
    if args.only: models=[m for m in models if Path(m).stem.removesuffix('.vmdl') in args.only]
    def export(model):
        stem=Path(model).name.removesuffix('.vmdl_c');glb=cache/(stem+'.glb')
        if not glb.exists():
            command=[str(CLI),'-i',str(GAME/'pak01_dir.vpk'),'-f',model,'-d','--gltf_export_format','glb',
                     '--gltf_export_animations','--gltf_animation_list','__bind_only__','--game',str(GAME/'gameinfo.gi'),'-o',str(glb)]
            print('Exporting',stem,flush=True)
            run=subprocess.run(command,capture_output=True,text=True)
            if run.returncode: raise RuntimeError(run.stdout+run.stderr)
        return model,glb
    with ThreadPoolExecutor(max_workers=3) as pool:
        exported=list(pool.map(export,models))
    manifest={};deduplicated={}
    for model,glb in exported:
        stem=glb.stem
        record=convert(glb,target/(stem+'.cnm'))
        digest=record['sha256']
        if digest in deduplicated:
            duplicate=target/record['file']
            assert duplicate.resolve().parent==target.resolve()
            duplicate.unlink()
            record['file']=deduplicated[digest]
        else: deduplicated[digest]=record['file']
        manifest[model.removesuffix('_c')]=record
        print(stem,record['vertices'],record['triangles'],flush=True)
    assert manifest
    provenance=json.loads((ROOT/'ferramentas/cs2-local/provenance.json').read_text())
    (target/'manifest.json').write_text(json.dumps({'build':provenance['build_number'],'client_sha256':provenance['client_sha256'],
        'vpk_index_sha256':vpk_hash,'exporter':'Source2Viewer-CLI 20.1','source':'Installed CS2 agents/models/*.vmdl_c; no materials or textures',
        'models':manifest},indent=2))
    print('Prepared',len(manifest),'models',flush=True)

if __name__=='__main__': main()
