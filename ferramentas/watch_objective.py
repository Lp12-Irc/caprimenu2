"""Observe C4/defuse fields only; never sends input or writes game memory."""
import contextlib,io,json,time,math
from pathlib import Path
with contextlib.redirect_stdout(io.StringIO()):
    import inspect_game as g
g.handle=g.k.OpenProcess(0x410,False,g.pid)
path=Path(__file__).parent.parent/'verificacao/tempo-real/bomba-real.jsonl'
path.parent.mkdir(parents=True,exist_ok=True)
seen=False; saw_defuse=False; last=None; started=time.monotonic()
try:
    with path.open('w') as log:
        print('Aguardando bomba plantada/desarme; apenas leitura.',flush=True)
        while time.monotonic()-started<90:
            bomb=g.read(g.base+g.o['dwPlantedC4']); globals_=g.read(g.base+g.o['dwGlobalVars'])
            now=g.read(globals_+0x30,'f')
            ticking=g.field(bomb,'C_PlantedC4','m_bBombTicking','B')==1
            blow=g.field(bomb,'C_PlantedC4','m_flC4Blow','f'); length=g.field(bomb,'C_PlantedC4','m_flTimerLength','f')
            defusing=g.field(bomb,'C_PlantedC4','m_bBeingDefused','B')==1
            defused=g.field(bomb,'C_PlantedC4','m_bBombDefused','B')==1
            end=g.field(bomb,'C_PlantedC4','m_flDefuseCountDown','f')
            duration=g.field(bomb,'C_PlantedC4','m_flDefuseLength','f')
            remaining=blow-now
            valid=bomb and math.isfinite(remaining) and 0<length<=3600 and -2<=remaining<=length+1
            if valid and (ticking or defused) and (remaining>0 or seen):
                seen=True; saw_defuse|=defusing
                record={'elapsed':round(time.monotonic()-started,3),'game_time':round(now,4),'remaining':round(max(0,remaining),3),
                    'defusing':defusing,'defuse_remaining':round(max(0,end-now),3) if defusing else 0,
                    'defuse_length':duration,'defused':defused}
                log.write(json.dumps(record)+'\n'); log.flush()
                key=(int(remaining),defusing,defused,int(end-now) if defusing else 0)
                if key!=last: print(json.dumps(record),flush=True); last=key
                if defused or remaining<=0: break
            time.sleep(.25)
        print(json.dumps({'observed_plant':seen,'observed_defuse':saw_defuse}),flush=True)
finally:g.k.CloseHandle(g.handle)
