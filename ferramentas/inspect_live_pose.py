"""Read only eye offsets, bone scales and recoil values for this installed build."""
import contextlib,io,json,struct
from pathlib import Path
with contextlib.redirect_stdout(io.StringIO()):
    import inspect_game as g
g.handle=g.k.OpenProcess(0x410,False,g.pid)
try:
    pawn=g.read(g.base+g.o['dwLocalPlayerPawn'])
    if not pawn:
        handle=g.field(g.controller,'CCSPlayerController','m_hPlayerPawn','I')
        chunk=g.read(g.entity_system+0x10+8*((handle&0x7fff)>>9))
        pawn=g.read(chunk+0x70*(handle&0x1ff)) if chunk and handle!=0xffffffff else 0
    scene=g.field(pawn,'C_BaseEntity','m_pGameSceneNode')
    array=g.read(scene+g.fields('CSkeletonInstance')['m_modelState']+0x80)
    punch=g.field(pawn,'C_CSPlayerPawn','m_pAimPunchServices')
    def vector(address):
        raw=g.raw(address,12)
        return list(struct.unpack('3f',raw)) if raw else None
    record={'build':g.read(g.modules['engine2.dll'][0]+g.offsets['engine2.dll']['dwBuildNumber'],'I'),'health':g.field(pawn,'C_BaseEntity','m_iHealth','i'),
        'origin':vector(scene+g.fields('CGameSceneNode')['m_vecAbsOrigin']),
        'view_offset':vector(pawn+g.fields('C_BaseModelEntity')['m_vecViewOffset']),
        'eye_angles':vector(g.base+g.o['dwViewAngles']),
        'head':vector(array+7*32),'head_scale':g.read(array+7*32+12,'f'),
        'predictable_punch':vector(punch+g.fields('CCSPlayer_AimPunchServices')['m_predictableBaseAngle']) if punch else None,
        'unpredictable_punch':vector(punch+g.fields('CCSPlayer_AimPunchServices')['m_unpredictableBaseAngle']) if punch else None}
    print(json.dumps(record,indent=2))
    (Path(__file__).parent/'cs2-local/live_pose_provenance.json').write_text(json.dumps(record,indent=2))
finally:g.k.CloseHandle(g.handle)
