"""Apply the requested response changes without changing feature toggles or keys."""
import json,math,shutil
from pathlib import Path
root=Path(__file__).parent.parent
path=root/'configs/default.hbc'
before=json.loads(bytes(value^0x4b for value in path.read_bytes()))
smooth=float(before.get('aim_smooth',2))
if not math.isfinite(smooth): raise RuntimeError('Invalid smoothing in existing profile')
changes={'aim_delay_ms':0.0,'aim_smooth':max(1.0,min(smooth,2.0)),'aim_ads_only':False,'tb_delay_ms':0.0}
after={**before,**changes}
backup=root/'backup-mira-esp-trigger/default-antes-instalacao.hbc'
if backup.exists(): raise RuntimeError('Installation profile backup already exists; inspect before retrying')
shutil.copy2(path,backup)
encoded=bytes(value^0x4b for value in json.dumps(after,ensure_ascii=False,indent=2).encode('utf-8'))
temporary=path.with_name('default.hbc.response.tmp')
temporary.write_bytes(encoded)
verified=json.loads(bytes(value^0x4b for value in temporary.read_bytes()))
if any(verified.get(key)!=value for key,value in before.items() if key not in changes):
    raise RuntimeError('Unrelated settings changed')
if any(verified.get(key)!=value for key,value in changes.items()): raise RuntimeError('Profile verification failed')
temporary.replace(path)
print(json.dumps(changes))
