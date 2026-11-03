"""API de licencas: apenas validacao; banco e administracao permanecem locais."""
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
import argparse,json,sqlite3,threading,time,re,ipaddress,hmac
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from admin_api import AdminApi,AdminError
from gateway_config import load as load_gateway
from urllib.parse import urlsplit
from contextlib import closing
VALIDATION_LIMIT=1800 # 30 polls/min/client; shared public IPs must support multiple clients.

def canonical_ip(value):
    if not isinstance(value,str) or len(value)>45 or '%' in value:raise ValueError('Invalid IP')
    address=ipaddress.ip_address(value.strip())
    return str(address.ipv4_mapped if isinstance(address,ipaddress.IPv6Address) and address.ipv4_mapped else address)

def request_ip(headers,peer,gateway_secret=None):
    address=canonical_ip(peer)
    if headers.get_all('X-Capri-Gateway',[]) or headers.get_all('X-Capri-Client-IP',[]):
        tokens=headers.get_all('X-Capri-Gateway',[]);ips=headers.get_all('X-Capri-Client-IP',[])
        if not ipaddress.ip_address(address).is_loopback or not gateway_secret or len(tokens)!=1 or len(ips)!=1:
            raise ValueError('Invalid gateway')
        if not hmac.compare_digest(tokens[0].encode(),gateway_secret.encode()):raise ValueError('Invalid gateway')
        return canonical_ip(ips[0])
    # The origin listens only on loopback. Cloudflared supplies the edge-observed IP.
    forwarded=headers.get_all('CF-Connecting-IP',[])
    if ipaddress.ip_address(address).is_loopback and forwarded:
        if len(forwarded)!=1:raise ValueError('Duplicate IP header')
        return canonical_ip(forwarded[0])
    return address

def migrate_ip_schema(database):
    with closing(sqlite3.connect(Path(database).resolve().as_uri()+'?mode=rw',uri=True,timeout=5)) as db:
        with db:
            db.executescript("""
            CREATE TABLE IF NOT EXISTS key_ip_policy(key_value TEXT PRIMARY KEY,ip_limit INTEGER NOT NULL DEFAULT 1 CHECK(ip_limit BETWEEN 1 AND 32));
            CREATE TABLE IF NOT EXISTS key_ip_bindings(key_value TEXT NOT NULL,ip TEXT NOT NULL,
                first_seen TEXT NOT NULL DEFAULT(strftime('%Y-%m-%dT%H:%M:%fZ','now')),
                last_seen TEXT NOT NULL DEFAULT(strftime('%Y-%m-%dT%H:%M:%fZ','now')),PRIMARY KEY(key_value,ip));
            """)

class LicenseServer(ThreadingHTTPServer):
    daemon_threads=True
    def __init__(self,address,database):
        self.database=Path(database).resolve()
        migrate_ip_schema(self.database)
        self.admin=AdminApi(self.database)
        gateway=load_gateway() if self.database==Path(__file__).resolve().parent.parent/'hillblack.db' else None
        self.gateway_secret=gateway['forward_secret'] if gateway else None
        self.limits={};self.lock=threading.Lock();self.capacity=threading.BoundedSemaphore(32)
        super().__init__(address,Handler)
    def validate(self,key,client_build=0,ip=None):
        with closing(sqlite3.connect(self.database.as_uri()+'?mode=ro',uri=True,timeout=2)) as db:
            row=db.execute("SELECT status,expires_at,CASE WHEN expires_at='never' THEN 0 WHEN julianday(expires_at) IS NULL THEN 1 WHEN julianday(expires_at)<=julianday('now') THEN 1 ELSE 0 END FROM keys WHERE key_value=?",(key,)).fetchone()
            release=None
            if db.execute("SELECT 1 FROM sqlite_master WHERE type='table' AND name='client_release'").fetchone():
                columns={r[1] for r in db.execute('PRAGMA table_info(client_release)')}
                extra=',package_sha256,package_size' if 'package_sha256' in columns and 'package_size' in columns else ",'' AS package_sha256,0 AS package_size"
                release=db.execute('SELECT build,version,url,notes'+extra+' FROM client_release WHERE id=1').fetchone()
        if row is None:return {'valid':False,'reason':'bad_key','message':'Chave nao encontrada no servidor.'}
        status,expires,expired=row
        if status=='expired' or expired:return {'valid':False,'reason':'expired','message':'Esta chave expirou.'}
        if status!='active':return {'valid':False,'reason':'revoked','message':'Chave revogada ou inativa.'}
        policy={'latest_build':0,'min_build':0,'version':'','download_url':'','release_notes':''}
        if release:
            build,version,url,notes,package_hash,package_size=release
            parts=urlsplit(url)
            if not isinstance(build,int) or not 1<=build<=2147483647 or not version or parts.scheme!='https' or not parts.hostname or parts.username or parts.password:
                raise sqlite3.DatabaseError('Invalid release policy')
            if package_hash and (not re.fullmatch('[a-f0-9]{64}',package_hash) or type(package_size) is not int or not 0<package_size<=100*1024*1024):raise sqlite3.DatabaseError('Invalid package metadata')
            policy={'package_sha256':package_hash,'package_size':package_size,'latest_build':build,'min_build':build,'version':version,'download_url':url,'release_notes':notes}
            if client_build<build:return {**policy,'valid':False,'reason':'update_required','message':'Atualizacao obrigatoria: instale a versao '+version+' para continuar.'}
        try:ip=canonical_ip(ip)
        except ValueError:return {'valid':False,'reason':'bad_request','message':'Nao foi possivel identificar o IP da conexao.'}
        # The first admission and the limit check share one write transaction, including concurrent logins.
        with closing(sqlite3.connect(self.database.as_uri()+'?mode=rw',uri=True,timeout=2)) as db:
            with db:
                db.execute('BEGIN IMMEDIATE')
                active=db.execute("SELECT 1 FROM keys WHERE key_value=? AND status='active' AND (expires_at='never' OR julianday(expires_at)>julianday('now'))",(key,)).fetchone()
                if not active:return {'valid':False,'reason':'revoked','message':'Chave revogada, expirada ou inativa.'}
                setting=db.execute('SELECT ip_limit FROM key_ip_policy WHERE key_value=?',(key,)).fetchone()
                limit=setting[0] if setting else 1
                if type(limit) is not int or not 1<=limit<=32:raise sqlite3.DatabaseError('Invalid IP policy')
                known=db.execute('SELECT 1 FROM key_ip_bindings WHERE key_value=? AND ip=?',(key,ip)).fetchone()
                count=db.execute('SELECT COUNT(*) FROM key_ip_bindings WHERE key_value=?',(key,)).fetchone()[0]
                if not known:
                    if count>=limit:return {**policy,'valid':False,'reason':'ip_limit','message':'Limite de IPs por chave excedido. Solicite a liberacao de IP ao administrador.'}
                    db.execute('INSERT INTO key_ip_bindings(key_value,ip) VALUES(?,?)',(key,ip));count+=1
                else:
                    db.execute("UPDATE key_ip_bindings SET last_seen=strftime('%Y-%m-%dT%H:%M:%fZ','now') WHERE key_value=? AND ip=? AND julianday(last_seen)<julianday('now','-1 minute')",(key,ip))
        return {**policy,'valid':True,'expires_at':expires,'ip_count':count,'ip_limit':limit,'message':'Chave validada. Acesso vinculado ao IP publico desta conexao.'}

class Handler(BaseHTTPRequestHandler):
    server_version='CapriLicense/1.0'
    def setup(self):
        super().setup();self.connection.settimeout(5)
    def log_message(self,*args):pass
    def reply(self,status,data):
        body=json.dumps(data,separators=(',',':'),ensure_ascii=False).encode()
        self.send_response(status);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(body)))
        self.send_header('Cache-Control','no-store');self.send_header('X-Content-Type-Options','nosniff');self.end_headers()
        try:self.wfile.write(body)
        except (BrokenPipeError,ConnectionResetError):pass
    def do_GET(self):
        if self.path=='/health':self.reply(200,{'service':'capri-license-api','status':'ok','admin_features':['keys_delete']})
        elif self.path=='/admin/release':
            with closing(sqlite3.connect(self.server.database.as_uri()+'?mode=ro',uri=True)) as db:
                if not db.execute("SELECT 1 FROM sqlite_master WHERE type='table' AND name='admin_release'").fetchone():self.reply(200,{'build':0});return
                db.row_factory=sqlite3.Row;row=db.execute('SELECT build,version,url,package_sha256,package_size FROM admin_release WHERE id=1').fetchone()
                self.reply(200,dict(row) if row else {'build':0})
        elif re.fullmatch(r'/packages/[a-f0-9]{64}\.zip',self.path):
            folder=(self.server.database.parent/'servidor/releases').resolve()
            package=(folder/self.path.rsplit('/',1)[1]).resolve()
            if package.parent!=folder or not package.is_file():self.reply(404,{'valid':False,'reason':'not_found'});return
            if not self.server.capacity.acquire(blocking=False):self.reply(503,{'valid':False,'reason':'busy'});return
            try:
                with package.open('rb') as stream:
                    size=package.stat().st_size
                    if not 0<size<=100*1024*1024:self.reply(404,{'valid':False,'reason':'not_found'});return
                    self.send_response(200);self.send_header('Content-Type','application/zip');self.send_header('Content-Length',str(size));self.send_header('Content-Disposition','attachment; filename="Capri-Cliente.zip"');self.send_header('X-Content-Type-Options','nosniff');self.end_headers()
                    while chunk:=stream.read(65536):self.wfile.write(chunk)
            except (BrokenPipeError,ConnectionResetError,TimeoutError):pass
            finally:self.server.capacity.release()
        else:self.reply(404,{'valid':False,'reason':'not_found'})
    def do_POST(self):
        if self.path in ('/admin/login','/admin/action'):
            if not self.server.admin.requests.acquire(blocking=False):self.reply(503,{'ok':False,'error':'Administracao ocupada.'});return
            try:
                ip=request_ip(self.headers,self.client_address[0],self.server.gateway_secret);length=int(self.headers.get('Content-Length','0'))
                if not 0<length<=4096 or self.headers.get('Content-Type','').split(';')[0].strip()!='application/json':raise ValueError()
                payload=json.loads(self.rfile.read(length))
                if not isinstance(payload,dict):raise ValueError()
                result=self.server.admin.login(payload,ip) if self.path=='/admin/login' else self.server.admin.execute(payload,self.headers.get('Authorization'),ip)
                self.reply(200,{'ok':True,'result':result})
            except AdminError as error:self.reply(error.status,{'ok':False,'error':str(error)})
            except (ValueError,TypeError,TimeoutError):self.reply(400,{'ok':False,'error':'Solicitacao invalida.'})
            except sqlite3.Error:self.reply(503,{'ok':False,'error':'Administracao temporariamente indisponivel.'})
            finally:self.server.admin.requests.release()
            return
        if self.path!='/validate':self.reply(404,{'valid':False,'reason':'not_found'});return
        if not self.server.capacity.acquire(blocking=False):self.reply(503,{'valid':False,'reason':'busy'});return
        try:
            try:address=request_ip(self.headers,self.client_address[0],self.server.gateway_secret)
            except ValueError:self.reply(400,{'valid':False,'reason':'bad_request'});return
            now=time.monotonic()
            with self.server.lock:
                if len(self.server.limits)>4096:self.server.limits={k:v for k,v in self.server.limits.items() if now-v[0]<60}
                start,count=self.server.limits.get(address,(now,0))
                if now-start>=60:start,count=now,0
                self.server.limits[address]=(start,count+1)
            if count>=VALIDATION_LIMIT:self.reply(429,{'valid':False,'reason':'rate_limit'});return
            try:
                length=int(self.headers.get('Content-Length','0'))
                if length<=0 or length>4096 or self.headers.get('Content-Type','').split(';')[0].strip()!='application/json':raise ValueError()
                payload=json.loads(self.rfile.read(length))
                key=payload.get('key') if isinstance(payload,dict) else None
                if not isinstance(key,str) or not 1<=len(key.strip())<=128:raise ValueError()
            except (ValueError,TimeoutError,OSError):self.reply(400,{'valid':False,'reason':'bad_request'});return
            client_build=payload.get('client_build',0)
            if type(client_build) is not int or not 0<=client_build<=2147483647:self.reply(400,{'valid':False,'reason':'bad_request'});return
            try:self.reply(200,self.server.validate(key.strip(),client_build,address))
            except sqlite3.Error:self.reply(503,{'valid':False,'reason':'service_unavailable','message':'Servico de chaves indisponivel.'})
        finally:self.server.capacity.release()

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--db',type=Path,required=True);parser.add_argument('--port',type=int,default=18765)
    args=parser.parse_args()
    if not args.db.is_file():parser.error('Banco de dados nao encontrado')
    server=LicenseServer(('127.0.0.1',args.port),args.db)
    print('Servico de chaves ativo em loopback.',flush=True)
    try:server.serve_forever()
    except KeyboardInterrupt:pass
    finally:server.server_close()
