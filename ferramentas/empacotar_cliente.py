from pathlib import Path
import argparse,zipfile,hashlib,json,configparser
root=Path(__file__).resolve().parent.parent
parser=argparse.ArgumentParser(description='Empacota o cliente com validacao online, sem banco ou chave salva.')
parser.add_argument('--incluir-admin',action='store_true')
args=parser.parse_args()
ini=root/'HillBlack.ini'
config=configparser.ConfigParser();config.read(ini,encoding='utf-8-sig')
endpoint=config.get('auth','endpoint',fallback='')
if config.get('auth','mode',fallback='')!='online' or not endpoint.startswith('https://'):
 raise RuntimeError('Inicie Iniciar-Servidor-Chaves.cmd para configurar o endereco HTTPS antes de empacotar.')
output=root/'entregas'/('Capri-Completo-Leve.zip' if args.incluir_admin else 'Capri-Cliente.zip')
output.parent.mkdir(parents=True,exist_ok=True)
manifest_path=root/'fontes/dist/client-version.json'
manifest=json.loads(manifest_path.read_text(encoding='utf-8-sig'))
if hashlib.sha256((root/'Capri.exe').read_bytes()).hexdigest().lower()!=manifest['sha256'].lower():
 raise RuntimeError('Instale primeiro o EXE compilado de fontes/dist/Capri.exe; a versao nao corresponde ao pacote.')
files=[root/'Capri.exe',ini,root/'client-version.json',root/'Atualizar-Capri.ps1']
(root/'client-version.json').write_bytes(manifest_path.read_bytes())
if args.incluir_admin:files.append(root/'Capri.Admin.exe')
for folder in ['assets','licenses']:
 files.extend(p for p in (root/folder).rglob('*') if p.is_file() and not p.is_relative_to(root/'assets/steam'))
assert all(p.is_file() for p in files),'Arquivo de execucao ausente'
config_bytes=None
profile=root/'configs/default.hbc'
if profile.is_file():
 settings=json.loads(bytes(x^0x4b for x in profile.read_bytes()));settings['saved_key']=''
 # Fresh installs receive inactive defaults; existing user profiles are preserved by the updater.
 for feature in ('esp','aim','rcs','radar','no_flash','crosshair','triggerbot','spectator_list','bomb_timer','item_esp','aim_fov_circle'):settings[feature]=False
 for obsolete in ('silent_aim','sa_fov','sa_bone','radar_scale','esp_utilities'):settings.pop(obsolete,None)
 config_bytes=bytes(x^0x4b for x in json.dumps(settings,ensure_ascii=False,indent=2).encode('utf-8'))
with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 for p in files:z.write(p,p.relative_to(root).as_posix())
 if config_bytes:z.writestr('configs/default.hbc',config_bytes)
 z.writestr('COMO-ABRIR.txt','Extraia tudo na mesma pasta. Abra Capri.exe e informe sua chave. PgUp abre o menu.\nO banco fica somente no PC do administrador. O servico e esse PC precisam estar ligados.\nEste pacote usa o endereco fixo gratuito. Apos instalar esta configuracao uma vez, nao e necessario trocar arquivos quando o servidor for reiniciado.\n')
with zipfile.ZipFile(output) as z:
 assert z.testzip() is None
 assert not any(Path(n).suffix.lower() in ['.db','.sqlite','.sqlite3'] for n in z.namelist())
 for p in files:assert hashlib.sha256(z.read(p.relative_to(root).as_posix())).digest()==hashlib.sha256(p.read_bytes()).digest(),str(p)
 if config_bytes:assert json.loads(bytes(x^0x4b for x in z.read('configs/default.hbc')))['saved_key']==''
print(f'Pacote online verificado, sem banco ou chave salva: {output} ({output.stat().st_size/1024**2:.2f} MiB)')
