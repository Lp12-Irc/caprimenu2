"""Derive RIP-relative globals from installed binaries using cs2-dumper signatures."""
import json, re, struct, hashlib
from pathlib import Path
ROOT=Path(__file__).parent
GAME=Path(r'F:\SteamLibrary\steamapps\common\Counter-Strike Global Offensive\game')
def scan(path,patterns):
    data=path.read_bytes(); pe=struct.unpack_from('<I',data,0x3c)[0]
    sections=pe+24+struct.unpack_from('<H',data,pe+20)[0]
    count=struct.unpack_from('<H',data,pe+6)[0]
    result={}
    for name,pattern in patterns.items():
        regex=b''.join(b'.' if t=='?' else re.escape(bytes.fromhex(t)) for t in pattern.split())
        matches=[]
        for i in range(count):
            entry=sections+40*i
            virtual_size,rva,size,offset=struct.unpack_from('<IIII',data,entry+8)
            flags=struct.unpack_from('<I',data,entry+36)[0]
            if not flags&0x20000000: continue
            for m in re.finditer(regex,data[offset:offset+size],re.DOTALL):
                at=offset+m.start(); rel=struct.unpack_from('<i',data,at+3)[0]
                matches.append(rva+m.start()+7+rel)
        if len(matches)!=1: raise RuntimeError(f'{name}: {len(matches)} matches')
        result[name]=matches[0]
    return result,hashlib.sha256(data).hexdigest()
client,client_hash=scan(GAME/'csgo/bin/win64/client.dll',{
    'dwEntityList':'48 89 0D ? ? ? ? E9 ? ? ? ? CC',
    'dwGameEntitySystem':'48 8B 1D ? ? ? ? 48 89 1D ? ? ? ? 4C 63 B3',
    'dwLocalPlayerController':'48 8B 05 ? ? ? ? 41 89 BE',
    'dwViewMatrix':'48 8D 0D ? ? ? ? 48 C1 E0 06',
    'dwGlobalVars':'48 89 15 ? ? ? ? 48 89 42',
    'dwPlantedC4':'48 8B 1D ? ? ? ? 45 32 F6',
})
result={'client.dll':client,'client_sha256':client_hash,'signature_source':'a2x/cs2-dumper@21ee85107074fdf0347dc8afd4a8c3dd8d2398e2/src/analysis/offsets.rs'}
(ROOT/'installed_signatures.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2))
