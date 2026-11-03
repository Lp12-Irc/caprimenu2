"""Refresh hosted package URLs after a temporary tunnel is recreated."""
from pathlib import Path
from urllib.parse import urlsplit,urlunsplit
from contextlib import closing
import configparser,sqlite3,re,zipfile,json,sys,urllib.request
if sys.platform=='win32':
    import truststore
    truststore.inject_into_ssl()
root=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(root/'servidor'))
from gateway_config import load as load_gateway
gateway=load_gateway()
if gateway:
    state=json.loads((root/'servidor/server-state.json').read_text(encoding='utf-8-sig'))
    request=urllib.request.Request(gateway['endpoint']+'/register',json.dumps({'origin':state['url']}).encode(),
        {'Content-Type':'application/json','User-Agent':'Capri-Server/1.0','Authorization':'Bearer '+gateway['register_secret']})
    with urllib.request.urlopen(request,timeout=20) as response:
        if not json.load(response).get('ok'):raise ValueError('Falha ao registrar o servidor no endereco fixo.')
    (root/'HillBlack.ini').write_text('[auth]\nmode=online\nendpoint='+gateway['endpoint']+'/validate\n',encoding='utf-8')
config=configparser.ConfigParser();config.read(root/'HillBlack.ini',encoding='utf-8-sig');origin=urlsplit(config.get('auth','endpoint'))
if origin.scheme!='https' or not origin.hostname or origin.username or origin.password:raise ValueError('Endpoint HTTPS invalido.')
with closing(sqlite3.connect(root/'hillblack.db')) as db:
    with db:
        for table in ['client_release','admin_release']:
            if not db.execute("SELECT 1 FROM sqlite_master WHERE type='table' AND name=?",(table,)).fetchone():continue
            row=db.execute('SELECT url FROM '+table+' WHERE id=1').fetchone()
            if not row:continue
            path=urlsplit(row[0]).path
            if not re.fullmatch(r'/packages/[a-f0-9]{64}\.zip',path):continue
            url=urlunsplit((origin.scheme,origin.netloc,path,'',''))
            db.execute('UPDATE '+table+' SET url=? WHERE id=1 AND url<>?',(url,url))
folder=root/'entregas';folder.mkdir(exist_ok=True)
with zipfile.ZipFile(folder/'Reconectar-Servidor.zip','w',zipfile.ZIP_DEFLATED) as archive:
    archive.write(root/'HillBlack.ini','HillBlack.ini')
    archive.writestr('COMO-RECONECTAR.txt','Feche o Capri. Extraia HillBlack.ini na mesma pasta de Capri.exe, substituindo o anterior. Abra o Capri novamente. Sua chave e seus perfis permanecem neste PC.\n')
with zipfile.ZipFile(folder/'Reconectar-ADM.zip','w',zipfile.ZIP_DEFLATED) as archive:
    archive.writestr('admin-connection.json',json.dumps({'endpoint':origin.scheme+'://'+origin.netloc},indent=2))
    archive.writestr('COMO-RECONECTAR-ADM.txt','Somente para o painel remoto do outro dono. Feche o ADM, extraia admin-connection.json na pasta dele e entre novamente. Nao extraia esse arquivo no painel local do PC que hospeda o banco.\n')
# Refresh local first-install bundles as well as hosted release metadata.
# Hosted content-addressed archives remain immutable.
for name in ['Capri-Cliente.zip','Painel ADM.zip']:
    package=folder/name
    if not package.exists():continue
    replacements={'HillBlack.ini':(root/'HillBlack.ini').read_bytes()}
    if gateway:
        replacements['COMO-ABRIR.txt']=('Este pacote usa um endereco fixo gratuito. Extraia tudo na mesma pasta. O PC do administrador e o servidor devem ficar ligados. Apos instalar este pacote uma vez, o reinicio do tunel nao exige novos arquivos. O servico gratuito tem limites de uso.\n').encode('utf-8')
    if name=='Painel ADM.zip':
        replacements['admin-connection.json']=json.dumps({'endpoint':origin.scheme+'://'+origin.netloc},indent=2).encode('utf-8')
    with zipfile.ZipFile(package) as source:
        if all(key in source.namelist() and source.read(key)==value for key,value in replacements.items()):continue
        temporary=package.with_suffix('.zip.syncing')
        with zipfile.ZipFile(temporary,'w',zipfile.ZIP_DEFLATED,compresslevel=1) as destination:
            for info in source.infolist():destination.writestr(info,replacements.pop(info.filename,source.read(info)))
            for key,value in replacements.items():destination.writestr(key,value)
    with zipfile.ZipFile(temporary) as verified:
        if verified.testzip() is not None:raise ValueError('Pacote de entrega corrompido.')
    temporary.replace(package)
print('Links e pacotes locais sincronizados; Reconectar-Servidor.zip e Reconectar-ADM.zip preparados.')
