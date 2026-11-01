"""Read-only validation of map name and player collision bounds."""
import contextlib,io,json,struct
from pathlib import Path
with contextlib.redirect_stdout(io.StringIO()):
    import inspect_game as g
g.handle=g.k.OpenProcess(0x410,False,g.pid)
try:
    globals_=g.read(g.base+g.o['dwGlobalVars'])
    names={hex(offset):g.raw(g.read(globals_+offset),128).split(b'\0')[0].decode(errors='replace') for offset in (0x178,0x180,0x188)}
    pawn=g.read(g.base+g.o['dwLocalPlayerPawn'])
    collision=g.field(pawn,'C_BaseEntity','m_pCollision')
    bounds={name:struct.unpack('3f',g.raw(collision+g.fields('CCollisionProperty')[name],12)) if g.raw(collision+g.fields('CCollisionProperty')[name],12) else None for name in ['m_vecMins','m_vecMaxs']}
    record={'engine_build':14194,'global_names':names,'local_collision':bounds}
    root=Path(__file__).parent/'cs2-local'
    (root/'map_camera_provenance.json').write_text(json.dumps(record,indent=2))
    print(json.dumps(record,indent=2))
finally:g.k.CloseHandle(g.handle)
