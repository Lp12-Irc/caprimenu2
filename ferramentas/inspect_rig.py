"""Read-only rig/crosshair diagnostic for the installed build."""
import contextlib,io,json,struct,time
with contextlib.redirect_stdout(io.StringIO()):
    import inspect_game as g
g.handle=g.k.OpenProcess(0x410,False,g.pid)
try:
    pawn=g.read(g.base+g.o['dwLocalPlayerPawn'])
    scene=g.field(pawn,'C_BaseEntity','m_pGameSceneNode')
    state=scene+g.fields('CSkeletonInstance')['m_modelState']
    binding=g.read(state+g.fields('CModelState')['m_hModel'])
    print('model-binding',hex(binding),g.raw(binding,128).hex())
    for offset in (0,8,16,24):
        ptr=g.read(binding+offset)
        print('model-candidate',offset,hex(ptr),g.raw(ptr,128).hex())
    name=g.read(state+g.fields('CModelState')['m_ModelName'])
    print('model-name',g.raw(name,128).split(b'\0')[0])
    model=g.read(binding)
    print('permanent-model-name',g.raw(g.read(model+8),128).split(b'\0')[0])
    for at in range(0x10,0x600,8):
        count=g.read(model+at,'I')
        if not 16<=count<=512: continue
        for poff in (8,16):
            data=g.read(model+at+poff)
            name=g.raw(g.read(data),64).split(b'\0')[0]
            if name and all(32<=b<127 for b in name):
                print('bone-name-vector',hex(at),count,'data_offset',poff,hex(data),[(i,g.raw(g.read(data+i*8),64).split(b'\0')[0].decode('ascii',errors='replace')) for i in range(min(count,32))])
    for adjustment in (0,8,16):
        skeleton=model+0x188+adjustment
        print('skeleton-candidate',hex(skeleton),g.raw(skeleton,72).hex())
        for data_offset in (0,8,16):
            data=g.read(skeleton+data_offset)
            head=g.raw(data,64)
            if head:
                print('vector-candidate',data_offset,hex(data),head.hex())
                for i in range(3):
                    print('name-candidate',i,g.raw(g.read(data+i*8),80).split(b'\0')[0])
    array=g.read(state+0x80)
    origin=struct.unpack('3f',g.raw(scene+g.fields('CGameSceneNode')['m_vecAbsOrigin'],12))
    print('bone-relative-positions',[(i,tuple(round(v-origin[j],2) for j,v in enumerate(struct.unpack('3f',g.raw(array+i*32,12))))) for i in range(32)])
    for sample in range(12):
        ctrl=g.read(g.base+g.o['dwLocalPlayerController'])
        pawn=g.read(g.base+g.o['dwLocalPlayerPawn'])
        index=g.field(pawn,'C_CSPlayerPawn','m_iIDEntIndex','i')
        chunk=g.read(g.entity_system+0x10+8*((index&0x7fff)>>9)) if index>0 else 0
        target=g.read(chunk+0x70*(index&0x1ff)) if chunk else 0
        print('crosshair',{'index':index,'target':hex(target),'team':g.field(target,'C_BaseEntity','m_iTeamNum','B') if target else 0,'alive':g.field(pawn,'C_BaseEntity','m_iHealth','i')>0},flush=True)
        time.sleep(.25)
finally:
    g.k.CloseHandle(g.handle)
