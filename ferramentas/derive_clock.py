"""Record the installed binary's C4 timer accesses for offset provenance."""
import hashlib, json, re, struct
from pathlib import Path
from capstone import Cs, CS_ARCH_X86, CS_MODE_64, CS_OP_MEM
root=Path(__file__).parent
offsets=json.loads((root/'cs2-local/offsets.json').read_text())['client.dll']
classes=json.loads((root/'cs2-local/client_dll.json').read_text())['client.dll']['classes']
blow=classes['C_PlantedC4']['fields']['m_flC4Blow']
path=Path(r'F:\SteamLibrary\steamapps\common\Counter-Strike Global Offensive\game\csgo\bin\win64\client.dll')
data=path.read_bytes(); pe=struct.unpack_from('<I',data,0x3c)[0]
section_table=pe+24+struct.unpack_from('<H',data,pe+20)[0]
md=Cs(CS_ARCH_X86,CS_MODE_64); md.detail=True; evidence=[]
for i in range(struct.unpack_from('<H',data,pe+6)[0]):
    entry=section_table+40*i
    _,rva,size,offset=struct.unpack_from('<IIII',data,entry+8)
    if not struct.unpack_from('<I',data,entry+36)[0]&0x20000000: continue
    code=data[offset:offset+size]
    for match in re.finditer(rb'\x48\x8b[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]....',code,re.DOTALL):
        at=match.start()
        target=rva+at+7+struct.unpack_from('<i',code,at+3)[0]
        if target!=offsets['dwGlobalVars'] or struct.pack('<I',blow) not in code[at:at+128]: continue
        instructions=list(md.disasm(code[at:at+128],rva+at))
        if not instructions: continue
        register=instructions[0].operands[0].reg
        timer_access=[ins for ins in instructions[1:] if ins.mnemonic in ('movss','subss') and any(op.type==CS_OP_MEM and op.mem.base==register and op.mem.disp==0x30 for op in ins.operands)]
        if timer_access:
            evidence.append({'global_load_rva':hex(rva+at),'clock_offset':'0x30',
                'instructions':[f'{ins.address:#x}: {ins.mnemonic} {ins.op_str}' for ins in instructions[:22]]})
if not evidence: raise RuntimeError('Could not verify timer clock access; inspect C4 timer function manually.')
info=json.loads((root/'cs2-local/info.json').read_text())
report={'client_sha256':hashlib.sha256(data).hexdigest(),'build':info['build_number'],'CGlobalVars_curtime':'0x30','CPlantedC4_blow':hex(blow),'evidence':evidence[:3]}
(root/'cs2-local/clock_provenance.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
