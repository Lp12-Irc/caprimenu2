"""Record actual model bone names; no game writes or player names."""
import contextlib,io,json,struct
from pathlib import Path
with contextlib.redirect_stdout(io.StringIO()):
    import inspect_game as g
g.handle=g.k.OpenProcess(0x410,False,g.pid)
models={}
try:
    system=g.read(g.base+g.o['dwEntityList'])
    def entity(index):
        chunk=g.read(system+0x10+8*((index&0x7fff)>>9))
        return g.read(chunk+0x70*(index&0x1ff)) if chunk else 0
    for i in range(1,65):
        ctrl=entity(i)
        if not ctrl or g.field(ctrl,'C_BaseEntity','m_iTeamNum','B') not in (2,3): continue
        handle=g.field(ctrl,'CCSPlayerController','m_hPlayerPawn','I'); pawn=entity(handle&0x7fff) if handle!=0xffffffff else 0
        scene=g.field(pawn,'C_BaseEntity','m_pGameSceneNode') if pawn else 0
        state=scene+g.fields('CSkeletonInstance')['m_modelState']
        binding=g.read(state+g.fields('CModelState')['m_hModel']); model=g.read(binding)
        count=g.read(model+0x160,'I'); names=g.read(model+0x168)
        if not 16<=count<=512: continue
        name=g.raw(g.read(state+g.fields('CModelState')['m_ModelName']),200).split(b'\0')[0].decode(errors='replace')
        if name in models: continue
        model_names=[g.raw(g.read(names+j*8),80).split(b'\0')[0].decode(errors='replace') for j in range(count)]
        models[name]={'bone_count':count,'names':model_names}
    if not models: raise RuntimeError('No readable live player model')
    root=Path(__file__).parent/'cs2-local'
    report={'build':14194,'client_sha256':json.loads((root/'provenance.json').read_text())['client_sha256'],
        'CModel_bone_name_count':'0x160','CModel_bone_name_data':'0x168','binding_model_pointer':'0x0','models':models}
    (root/'rig_provenance.json').write_text(json.dumps(report,indent=2))
    print(json.dumps({name:{'bones':model['bone_count'],'body':model['names'][:23]} for name,model in models.items()},indent=2))
finally:
    g.k.CloseHandle(g.handle)
