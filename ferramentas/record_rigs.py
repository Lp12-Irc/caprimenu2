"""Record actual model bone names; no game writes or player names."""
import contextlib,io,json,struct,hashlib
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
        # The main-menu agent is attached to the local controller with team 0.
        if not ctrl or (ctrl!=g.controller and g.field(ctrl,'C_BaseEntity','m_iTeamNum','B') not in (2,3)): continue
        handle=g.field(ctrl,'CCSPlayerController','m_hPlayerPawn','I'); pawn=entity(handle&0x7fff) if handle!=0xffffffff else 0
        scene=g.field(pawn,'C_BaseEntity','m_pGameSceneNode') if pawn else 0
        state=scene+g.fields('CSkeletonInstance')['m_modelState']
        binding=g.read(state+g.fields('CModelState')['m_hModel']); model=g.read(binding)
        count=g.read(model+0x160,'I'); names=g.read(model+0x168)
        if not 16<=count<=512: continue
        name=g.raw(g.read(state+g.fields('CModelState')['m_ModelName']),200).split(b'\0')[0].decode(errors='replace')
        if not name.startswith('agents/models/'): continue
        if name in models: continue
        model_names=[g.raw(g.read(names+j*8),80).split(b'\0')[0].decode(errors='replace') for j in range(count)]
        if not {'pelvis','spine_0','head_0','hand_L','hand_R'}.issubset(model_names):
            raise RuntimeError('Live rig names do not match a player skeleton')
        bone_array=g.read(state+0x80)
        if not g.raw(bone_array,count*32): raise RuntimeError('Unreadable live bone transforms')
        models[name]={'bone_count':count,'names':model_names}
    if not models: raise RuntimeError('No readable live player model')
    root=Path(__file__).parent/'cs2-local'
    info=json.loads((root/'info.json').read_text())
    client_path=Path(r'F:\SteamLibrary\steamapps\common\Counter-Strike Global Offensive\game\csgo\bin\win64\client.dll')
    report={'build':info['build_number'],'client_sha256':hashlib.sha256(client_path.read_bytes()).hexdigest(),
        'CModel_bone_name_count':'0x160','CModel_bone_name_data':'0x168','binding_model_pointer':'0x0','models':models}
    (root/'rig_provenance.json').write_text(json.dumps(report,indent=2))
    print(json.dumps({name:{'bones':model['bone_count'],'body':model['names'][:23]} for name,model in models.items()},indent=2))
finally:
    g.k.CloseHandle(g.handle)
