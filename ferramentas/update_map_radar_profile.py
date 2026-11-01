import json,shutil
from pathlib import Path
root=Path(__file__).parent.parent
path=root/'configs/default.hbc'
original=json.loads(bytes(value^0x4b for value in path.read_bytes()))
changes={'radar_sz':max(320.0,min(float(original.get('radar_sz',320)),600.0)),'radar_zoom':1.0,'radar_names':True}
updated={**original,**changes}
backup=root/'backup-esp-radar-mapa/default-antes-instalacao.hbc'
if backup.exists(): raise RuntimeError('Installation profile backup already exists; inspect before retrying')
shutil.copy2(path,backup)
temporary=path.with_name('default.hbc.radar.tmp')
temporary.write_bytes(bytes(value^0x4b for value in json.dumps(updated,ensure_ascii=False,indent=2).encode('utf-8')))
verified=json.loads(bytes(value^0x4b for value in temporary.read_bytes()))
if any(verified.get(key)!=value for key,value in original.items() if key not in changes): raise RuntimeError('Unrelated settings changed')
if any(verified.get(key)!=value for key,value in changes.items()): raise RuntimeError('Radar settings were not saved correctly')
temporary.replace(path)
print(json.dumps(changes))
