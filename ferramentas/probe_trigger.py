"""Read-only observation while the user aims at a practice bot."""
import contextlib,io,json,time,ctypes as c
from pathlib import Path
with contextlib.redirect_stdout(io.StringIO()):
    import inspect_game as g
g.handle=g.k.OpenProcess(0x410,False,g.pid)
user=c.WinDLL('user32'); user.GetAsyncKeyState.argtypes=[c.c_int]; user.GetAsyncKeyState.restype=c.c_short
user.GetForegroundWindow.restype=c.c_void_p
user.GetWindowThreadProcessId.argtypes=[c.c_void_p,c.POINTER(c.c_ulong)]
records=[]
try:
    for sample in range(60):
        pawn=g.read(g.base+g.o['dwLocalPlayerPawn']); system=g.read(g.base+g.o['dwEntityList'])
        index=g.field(pawn,'C_CSPlayerPawn','m_iIDEntIndex','i')
        chunk=g.read(system+0x10+8*((index&0x7fff)>>9)) if index>0 else 0
        target=g.read(chunk+0x70*(index&0x1ff)) if chunk else 0
        foreground=c.c_ulong(); user.GetWindowThreadProcessId(user.GetForegroundWindow(),c.byref(foreground))
        record={'sample':sample,'alt':bool(user.GetAsyncKeyState(18)&0x8000),'game_foreground':foreground.value==g.pid,
            'health':g.field(pawn,'C_BaseEntity','m_iHealth','i'),'crosshair_index':index,
            'target_team':g.field(target,'C_BaseEntity','m_iTeamNum','B') if target else 0,
            'target_health':g.field(target,'C_BaseEntity','m_iHealth','i') if target else 0}
        records.append(record)
        if sample==0 or any(record[k]!=records[-2][k] for k in record if k!='sample'): print(json.dumps(record),flush=True)
        time.sleep(.5)
finally:
    g.k.CloseHandle(g.handle)
    (Path(__file__).parent/'cs2-local/trigger_observation.json').write_text(json.dumps(records,indent=2))
