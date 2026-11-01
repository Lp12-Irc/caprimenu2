"""Read-only diagnostic for the installed CS2 build. Never sends input or writes memory."""
import ctypes as c
from ctypes import wintypes as w
import json
from pathlib import Path
import struct
import sys, time

k = c.WinDLL('kernel32', use_last_error=True)
k.CreateToolhelp32Snapshot.restype = w.HANDLE
k.OpenProcess.restype = w.HANDLE
k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
k.ReadProcessMemory.argtypes = [w.HANDLE, c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)]
k.CloseHandle.argtypes = [w.HANDLE]
class Process(c.Structure):
    _fields_ = [('size',w.DWORD),('usage',w.DWORD),('pid',w.DWORD),('heap',c.c_size_t),('module',w.DWORD),('threads',w.DWORD),('parent',w.DWORD),('priority',w.LONG),('flags',w.DWORD),('exe',w.WCHAR*260)]
class Module(c.Structure):
    _fields_ = [('size',w.DWORD),('id',w.DWORD),('pid',w.DWORD),('global_usage',w.DWORD),('usage',w.DWORD),('base',c.c_void_p),('length',w.DWORD),('handle',w.HMODULE),('name',w.WCHAR*256),('path',w.WCHAR*260)]
for name, kind in [('Process32FirstW',Process),('Process32NextW',Process),('Module32FirstW',Module),('Module32NextW',Module)]:
    getattr(k,name).argtypes = [w.HANDLE,c.POINTER(kind)]
snapshot=k.CreateToolhelp32Snapshot(2,0)
entry=Process(); entry.size=c.sizeof(entry)
ok=k.Process32FirstW(snapshot,c.byref(entry)); pid=0
while ok:
    if entry.exe.lower()=='cs2.exe': pid=entry.pid; break
    ok=k.Process32NextW(snapshot,c.byref(entry))
k.CloseHandle(snapshot)
if not pid: raise RuntimeError('CS2 is not running')
snapshot=k.CreateToolhelp32Snapshot(0x18,pid)
module=Module(); module.size=c.sizeof(module)
ok=k.Module32FirstW(snapshot,c.byref(module)); modules={}
while ok:
    modules[module.name.lower()]=(module.base,module.length)
    ok=k.Module32NextW(snapshot,c.byref(module))
k.CloseHandle(snapshot)
handle=k.OpenProcess(0x410,False,pid)
if not handle: raise c.WinError(c.get_last_error())
def raw(addr,size):
    buf=c.create_string_buffer(size); count=c.c_size_t()
    if not k.ReadProcessMemory(handle,c.c_void_p(addr),buf,size,c.byref(count)) or count.value!=size: return b''
    return buf.raw
def read(addr,fmt='Q'):
    b=raw(addr,struct.calcsize(fmt))
    return struct.unpack(fmt,b)[0] if b else 0
def fields(cls): return schema[cls]['fields']
def field(addr,cls,name,fmt='Q'): return read(addr+fields(cls)[name],fmt)
root=Path(__file__).parent
dump=root/'cs2-local'
if not (dump/'offsets.json').exists(): dump=root/'cs2-dumper'/'output'
offsets=json.loads((dump/'offsets.json').read_text())
schema=json.loads((dump/'client_dll.json').read_text())['client.dll']['classes']
base=modules['client.dll'][0]
o=offsets['client.dll']
if (root/'installed_signatures.json').exists():
    o.update(json.loads((root/'installed_signatures.json').read_text())['client.dll'])
entity_system=read(base+o['dwEntityList'])
controller=read(base+o['dwLocalPlayerController'])
local_pawn=read(base+o.get('dwLocalPlayerPawn',0))
print(json.dumps({'pid':pid,'dump':str(dump),'build':read(modules['engine2.dll'][0]+offsets['engine2.dll']['dwBuildNumber'],'I'),'entity_system':hex(entity_system),'local_controller':hex(controller),'local_pawn':hex(local_pawn),'matrix':struct.unpack('16f',raw(base+o['dwViewMatrix'],64)) if raw(base+o['dwViewMatrix'],64) else []}))
if controller:
    print('local',{'team':field(controller,'C_BaseEntity','m_iTeamNum','B'),'handle':hex(field(controller,'CCSPlayerController','m_hPlayerPawn','I')),'alive':field(controller,'CCSPlayerController','m_bPawnIsAlive','B')})
for chunk_offset in (0x10,0x18):
    chunk=read(entity_system+chunk_offset)
    for stride in (0x70,0x78):
        for ptr_offset in (0,0x10):
            matches=[i for i in range(65) if read(chunk+stride*i+ptr_offset)==controller and controller]
            if matches: print('controller-layout',{'chunk_offset':hex(chunk_offset),'stride':hex(stride),'ptr_offset':hex(ptr_offset),'indices':matches})
print('entity-system-head',raw(entity_system,128).hex())
print('entity-global-near',raw(base+o['dwEntityList']-32,96).hex())
for array in (entity_system, read(entity_system+0x10), base+o['dwEntityList']):
    for coff in range(0,0x80,8):
        chunk=read(array+coff)
        data=raw(chunk,0x10000)
        if not data: continue
        needle=struct.pack('Q',controller)
        start=0
        while controller and (at:=data.find(needle,start))>=0:
            print('controller-reference',{'array':hex(array),'chunk_offset':hex(coff),'at':hex(at),'chunk':hex(chunk)})
            start=at+8
if local_pawn:
    sn=field(local_pawn,'C_BaseEntity','m_pGameSceneNode')
    print('pawn',{'health':field(local_pawn,'C_BaseEntity','m_iHealth','i'),'team':field(local_pawn,'C_BaseEntity','m_iTeamNum','B'),'origin':struct.unpack('3f',raw(sn+fields('CGameSceneNode')['m_vecAbsOrigin'],12)) if sn else None})
    model=fields('CSkeletonInstance')['m_modelState']
    bones=read(sn+model+0x80)
    print('bone-array',hex(bones),'first',struct.unpack('8f',raw(bones,32)) if raw(bones,32) else None)
    print('global-vars-head',raw(read(base+o['dwGlobalVars']),128).hex())
if '--watch-bomb' in sys.argv:
    for sample in range(60):
        planted_list=read(base+o['dwPlantedC4'])
        if sample == 0:
            for candidate in (planted_list, read(planted_list)):
                print('bomb-candidate',hex(candidate),{'ticking':field(candidate,'C_PlantedC4','m_bBombTicking','B'),'blow':field(candidate,'C_PlantedC4','m_flC4Blow','f'),'length':field(candidate,'C_PlantedC4','m_flTimerLength','f')},flush=True)
        bomb=planted_list
        if bomb and field(bomb,'C_PlantedC4','m_bBombTicking','B'):
            now=read(read(base+o['dwGlobalVars'])+0x30,'f')
            blow=field(bomb,'C_PlantedC4','m_flC4Blow','f')
            length=field(bomb,'C_PlantedC4','m_flTimerLength','f')
            print('bomb-clock',json.dumps({'current_time':now,'blow_time':blow,'remaining':blow-now,'timer_length':length,'valid':0<=blow-now<=length}),flush=True)
        time.sleep(.5)
k.CloseHandle(handle)
