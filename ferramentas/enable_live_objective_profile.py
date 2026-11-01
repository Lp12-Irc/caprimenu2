import json,shutil
from pathlib import Path
root=Path(__file__).parent.parent
path=root/'configs/default.hbc'
original=json.loads(bytes(value^0x4b for value in path.read_bytes()))
updated={**original,'bomb_timer':True}
backup=root/'backup-tempo-real/default-antes-instalacao.hbc'
if backup.exists(): raise RuntimeError('Installation profile backup already exists; inspect before retrying')
shutil.copy2(path,backup)
temporary=path.with_name('default.hbc.objective.tmp')
temporary.write_bytes(bytes(value^0x4b for value in json.dumps(updated,ensure_ascii=False,indent=2).encode('utf-8')))
verified=json.loads(bytes(value^0x4b for value in temporary.read_bytes()))
if any(verified.get(key)!=value for key,value in original.items() if key!='bomb_timer'): raise RuntimeError('Unrelated settings changed')
if verified.get('bomb_timer') is not True: raise RuntimeError('Objective timer was not enabled')
temporary.replace(path)
print('bomb_timer=true; demais ajustes preservados')
