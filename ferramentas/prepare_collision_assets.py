"""Export installed static map physics and prebuild compact ray-query BVHs."""
from pathlib import Path
import argparse,ctypes,hashlib,json,struct,subprocess
import numpy as np
root=Path(__file__).resolve().parent.parent
def build(glb,target):
    data=glb.read_bytes();length=struct.unpack_from('<I',data,12)[0]
    doc=json.loads(data[20:20+length]);binary=data[28+length:]
    def accessor(index):
        a=doc['accessors'][index];view=doc['bufferViews'][a['bufferView']]
        dtype={5126:'<f4',5125:'<u4',5123:'<u2'}[a['componentType']]
        count=a['count'];width=3 if a['type']=='VEC3' else 1
        return np.frombuffer(binary,dtype=dtype,count=count*width,offset=view.get('byteOffset',0)+a.get('byteOffset',0)).reshape(count,width)
    groups=[]
    ignored={'npcclip','playerclip','csgo_grenadeclip','passbullets','ladder','trigger'}
    for node_info in doc['nodes']:
        if 'mesh' not in node_info:continue
        tags=set(node_info.get('extras',{}).get('InteractAs',[]))
        if tags and tags.issubset(ignored):continue
        mesh=doc['meshes'][node_info['mesh']]
        for primitive in mesh['primitives']:
            vertices=accessor(primitive['attributes']['POSITION']);indices=accessor(primitive['indices']).reshape(-1,3)
            groups.append(vertices[indices])
    triangles=np.concatenate(groups).astype('<f4');triangles=triangles[np.isfinite(triangles).all(axis=(1,2))]
    low=triangles.min(axis=1);high=triangles.max(axis=1);centers=(low+high)*.5
    order=np.arange(len(triangles),dtype=np.uint32);nodes=[]
    def node(start,stop):
        index=len(nodes);nodes.append(None);ids=order[start:stop]
        minimum=low[ids].min(axis=0);maximum=high[ids].max(axis=0)
        if stop-start<=64:left=right=0;count=stop-start
        else:
            axis=int(np.argmax(maximum-minimum));middle=(stop-start)//2
            order[start:stop]=ids[np.argpartition(centers[ids,axis],middle)]
            split=start+middle;left=node(start,split);right=node(split,stop);count=0
        nodes[index]=struct.pack('<6f4I',*minimum,*maximum,left,right,start,count)
        return index
    node(0,len(order))
    vertices,inverse=np.unique(triangles.reshape(-1,3),axis=0,return_inverse=True)
    indexed=inverse.reshape(-1,3).astype('<u4')
    raw=b'CAPRIBVH'+struct.pack('<4I',2,len(triangles),len(nodes),len(vertices))+vertices.astype('<f4').tobytes()+indexed[order].tobytes()+b''.join(nodes)
    cabinet=ctypes.WinDLL('cabinet',use_last_error=True)
    cabinet.CreateCompressor.argtypes=[ctypes.c_uint,ctypes.c_void_p,ctypes.POINTER(ctypes.c_void_p)]
    cabinet.Compress.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
    cabinet.CloseCompressor.argtypes=[ctypes.c_void_p]
    handle=ctypes.c_void_p();needed=ctypes.c_size_t()
    if not cabinet.CreateCompressor(4,None,ctypes.byref(handle)):raise ctypes.WinError(ctypes.get_last_error())
    try:
        cabinet.Compress(handle,raw,len(raw),None,0,ctypes.byref(needed));output=ctypes.create_string_buffer(needed.value)
        if not cabinet.Compress(handle,raw,len(raw),output,len(output),ctypes.byref(needed)):raise ctypes.WinError(ctypes.get_last_error())
        target.write_bytes(b'CAPRIZ01'+struct.pack('<I',len(raw))+output.raw[:needed.value])
    finally:cabinet.CloseCompressor(handle)
    return {'format':2,'leaf_size':64,'triangles':len(triangles),'nodes':len(nodes),'vertices':len(vertices),'bytes':target.stat().st_size,'raw_bounds_min':low.min(axis=0).tolist(),'raw_bounds_max':high.max(axis=0).tolist()}
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--game',type=Path,default=Path('F:/SteamLibrary/steamapps/common/Counter-Strike Global Offensive/game/csgo'));parser.add_argument('--maps',nargs='+',default=['de_dust2','de_mirage','de_inferno','de_nuke','de_ancient','de_anubis','de_overpass','de_vertigo','de_train','cs_office','cs_italy']);args=parser.parse_args()
    output=root/'fontes/assets/collision';output.mkdir(parents=True,exist_ok=True)
    cli=root/'ferramentas/source2viewer/bin/Source2Viewer-CLI.exe'
    if not cli.is_file():
        import zipfile
        with zipfile.ZipFile(root/'ferramentas/source2viewer/cli-windows-x64.zip') as archive:archive.extractall(cli.parent)
    for name in args.maps:
        vpk=args.game/'maps'/(name+'.vpk');work=root/'ferramentas/map-collision'/name
        work.mkdir(parents=True,exist_ok=True)
        target=output/(name+'.bvh')
        if target.exists() and json.loads(target.with_suffix('.json').read_text()).get('leaf_size')==64:print(name,'already prepared',flush=True);continue
        if not (work/'maps'/name/'world_physics_physics.glb').exists():subprocess.run([str(cli),'-i',str(vpk),'-f',f'maps/{name}/world_physics.vmdl_c','-o',str(work),'-d','--gltf_export_format','glb'],stdout=subprocess.DEVNULL,check=True)
        result=build(work/'maps'/name/'world_physics_physics.glb',target)
        result.update(map=name,vpk_sha256=hashlib.file_digest(vpk.open('rb'),'sha256').hexdigest(),source='Source 2 Viewer: world_physics.vmdl_c physics export; raw Source coordinates; static geometry only')
        target.with_suffix('.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        print(name,result['triangles'],result['bytes'],flush=True)
