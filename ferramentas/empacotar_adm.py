"""Build separate ADM update and co-owner delivery archives, without databases or credentials."""
from pathlib import Path
from contextlib import closing
from urllib.parse import urlsplit,urljoin
import argparse,configparser,hashlib,json,sqlite3,zipfile,shutil,urllib.request
import sys
if sys.platform=='win32':
    import truststore
    truststore.inject_into_ssl()
root=Path(__file__).resolve().parent.parent
parser=argparse.ArgumentParser();parser.add_argument('--publicar',action='store_true');args=parser.parse_args()
manifest=json.loads((root/'admin-version.json').read_text(encoding='utf-8-sig'))
assert hashlib.sha256((root/'Capri.Admin.exe').read_bytes()).hexdigest()==manifest['sha256']
client=json.loads((root/'client-version.json').read_text(encoding='utf-8-sig'))
assert hashlib.sha256((root/'Capri.exe').read_bytes()).hexdigest()==client['sha256']
config=configparser.ConfigParser();config.read(root/'HillBlack.ini',encoding='utf-8-sig');endpoint=config.get('auth','endpoint');origin=urlsplit(endpoint)
assert origin.scheme=='https' and origin.hostname and not origin.username and not origin.password
out=root/'entregas';out.mkdir(exist_ok=True)
update=out/'Capri-ADM-Atualizacao.zip'
with zipfile.ZipFile(update,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
    for name in ['Capri.Admin.exe','admin-version.json','Atualizar-ADM.ps1']:archive.write(root/name,name)
# The update archive intentionally preserves the destination connection configuration.
# First-time remote installations must use the full bundle below.
(out/'QUAL-ZIP-ENVIAR.txt').write_text('PRIMEIRA INSTALACAO: envie Painel ADM.zip e extraia todos os arquivos.\nCapri-ADM-Atualizacao.zip serve somente para atualizar uma instalacao existente.\nSe aparecer configuracao do servidor ausente, feche o ADM e extraia Reconectar-ADM.zip na pasta de Capri.Admin.exe.\nNao instale a configuracao remota no PC que hospeda o banco.\n',encoding='utf-8')
with zipfile.ZipFile(out/'Reconectar-ADM.zip','w',zipfile.ZIP_DEFLATED) as repair:
    repair.writestr('admin-connection.json',json.dumps({'endpoint':origin.scheme+'://'+origin.netloc},indent=2))
    repair.writestr('COMO-RECONECTAR-ADM.txt','Somente no PC remoto: feche Capri.Admin.exe. Extraia admin-connection.json junto de Capri.Admin.exe e abra novamente. Use a conta de administrador criada pelo dono. Nao aplicar no PC que hospeda o banco.\n')
bundle=out/'Painel ADM.zip'
with zipfile.ZipFile(bundle,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
    for name in ['Capri.Admin.exe','admin-version.json','Atualizar-ADM.ps1','Capri.exe','client-version.json','Atualizar-Capri.ps1','HillBlack.ini']:archive.write(root/name,name)
    archive.writestr('admin-connection.json',json.dumps({'endpoint':origin.scheme+'://'+origin.netloc},indent=2))
    archive.writestr('COMO-ABRIR.txt','PAINEL ADM COMPARTILHADO\n\nExtraia tudo na mesma pasta e abra Capri.Admin.exe.\nUse a conta separada criada pelo dono no PC do servidor, em Administradores.\nO banco fica no servidor; nao copie hillblack.db nem admin-login.dat.\nO PC do servidor deve ficar ligado.\n\nO ADM verifica atualizacoes ao abrir e a cada minuto enquanto estiver aberto.\nAtualizacoes preservam sua conexao e seu login salvo neste Windows.\n\nCAPRI CLIENTE\nCapri.exe tambem esta incluido. Ele exige uma chave de cliente.\nNa primeira abertura, clique em Atualizar agora para completar os arquivos do jogo; o pacote completo sera baixado e instalado.\n\nA conexao usa um endereco HTTPS fixo gratuito. O servidor atualiza a conexao ao iniciar; mantenha os arquivos de configuracao junto dos executaveis.\n')
for path in [update,bundle]:
    assert path.stat().st_size<104857600,'Pacote acima de 100 MiB'
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
        assert not any(Path(n).suffix.lower() in ('.db','.sqlite','.sqlite3') or Path(n).name.lower() in ('admin-login.dat','credentials.dat') for n in archive.namelist())
    print(path.name,round(path.stat().st_size/1048576,2),'MiB - sem banco ou credenciais',flush=True)
if args.publicar:
    digest=hashlib.sha256(update.read_bytes()).hexdigest();size=update.stat().st_size
    folder=root/'servidor/releases';folder.mkdir(exist_ok=True);target=folder/(digest+'.zip')
    if not target.exists():shutil.copyfile(update,target)
    assert hashlib.sha256(target.read_bytes()).hexdigest()==digest
    url=urljoin(endpoint,'/packages/'+digest+'.zip')
    with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'Capri-Updater/1.0'}),timeout=30) as response:assert hashlib.sha256(response.read(size+1)).hexdigest()==digest
    with closing(sqlite3.connect(root/'hillblack.db')) as db:
        with db:
            db.execute('CREATE TABLE IF NOT EXISTS admin_release(id INTEGER PRIMARY KEY CHECK(id=1),build INTEGER NOT NULL,version TEXT NOT NULL,url TEXT NOT NULL,notes TEXT NOT NULL,package_sha256 TEXT NOT NULL,package_size INTEGER NOT NULL)')
            db.execute('BEGIN IMMEDIATE')
            old=db.execute('SELECT build,package_sha256 FROM admin_release WHERE id=1').fetchone()
            if old and (old[0]>manifest['build'] or old[0]==manifest['build'] and old[1]!=digest):raise ValueError('Novo pacote exige build ADM maior.')
            db.execute('INSERT INTO admin_release VALUES(1,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET build=excluded.build,version=excluded.version,url=excluded.url,notes=excluded.notes,package_sha256=excluded.package_sha256,package_size=excluded.package_size',(manifest['build'],manifest['version'],url,'ADM remoto, atualizacao automatica e interface revisada.',digest,size))
    print('Atualizacao ADM publicada e download HTTPS verificado.')
